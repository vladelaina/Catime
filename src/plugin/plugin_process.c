/**
 * @file plugin_process.c
 * @brief Plugin process lifecycle management
 */

#include "plugin/plugin_process.h"
#include "plugin/plugin_data.h"
#include "log.h"
#include <shellapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>

/* Job Object for automatic cleanup when Catime exits */
static HANDLE g_hJob = NULL;
static HWND g_hNotifyWnd = NULL;

#define JOB_PROCESS_STACK_CAPACITY 16
#define PROCESS_TREE_STACK_CAPACITY 256
#define PROCESS_TREE_MAX_DEPTH 32
#define PROCESS_TREE_VISITED_CAPACITY (PROCESS_TREE_MAX_DEPTH + 1)
#define PLUGIN_LAUNCH_READY_TIMEOUT_MS 5000
#define PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS 2000
#define PLUGIN_LAUNCH_START_FAILURE_COOLDOWN_MS 2000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

#define PLUGIN_LAUNCH_READY_PENDING 0
#define PLUGIN_LAUNCH_READY_SIGNALED 1
#define PLUGIN_LAUNCH_READY_ABANDONED 2

/* Forward declarations */
static void TerminateProcessTree(DWORD pid, int depth);

typedef struct {
    DWORD processId;
    DWORD parentProcessId;
} ProcessTreeEntry;

static BOOL GrowProcessTreeSnapshot(ProcessTreeEntry** entries,
                                    DWORD* capacity,
                                    DWORD count,
                                    const ProcessTreeEntry* stackEntries) {
    if (*capacity == 0 || *capacity > ((DWORD)~(DWORD)0) / 2) {
        return FALSE;
    }

    DWORD newCapacity = *capacity * 2;
    size_t newSize = (size_t)newCapacity * sizeof(ProcessTreeEntry);
    if (newSize / sizeof(ProcessTreeEntry) != newCapacity) {
        return FALSE;
    }

    if (*entries == stackEntries) {
        ProcessTreeEntry* newEntries = (ProcessTreeEntry*)malloc(newSize);
        if (!newEntries) return FALSE;

        memcpy(newEntries, stackEntries, (size_t)count * sizeof(ProcessTreeEntry));
        *entries = newEntries;
    } else {
        ProcessTreeEntry* newEntries = (ProcessTreeEntry*)realloc(*entries, newSize);
        if (!newEntries) return FALSE;

        *entries = newEntries;
    }

    *capacity = newCapacity;
    return TRUE;
}

static BOOL GetJobProcessIdListBufferSize(DWORD processCount, size_t* bufferSize) {
    if (!bufferSize) return FALSE;

    size_t extraProcessIds = processCount > 0 ? (size_t)(processCount - 1) : 0;
    if (extraProcessIds >
        (((size_t)-1) - sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)) / sizeof(ULONG_PTR)) {
        return FALSE;
    }

    size_t size = sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
                  extraProcessIds * sizeof(ULONG_PTR);
    if (size > (size_t)((DWORD)~(DWORD)0)) {
        return FALSE;
    }

    *bufferSize = size;
    return TRUE;
}

void PluginProcess_CloseMonitorThreadHandle(HANDLE hThread, BOOL waitForExit) {
    if (!hThread) return;

    if (waitForExit) {
        DWORD threadId = GetThreadId(hThread);
        if (threadId != 0 && threadId != GetCurrentThreadId()) {
            DWORD waitResult = WaitForSingleObject(hThread, 2000);
            if (waitResult == WAIT_TIMEOUT) {
                LOG_WARNING("[Process] Monitor thread did not exit before handle close");
            } else if (waitResult == WAIT_FAILED) {
                LOG_WARNING("[Process] Failed waiting for monitor thread: %lu", GetLastError());
            }
        }
    } else if (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT) {
        LOG_DEBUG("[Process] Closing monitor thread handle before thread has fully returned");
    }
    CloseHandle(hThread);
}

static void ClearPluginProcessHandles(PluginInfo* plugin, BOOL waitForMonitorThread) {
    if (!plugin) return;

    HANDLE hProcToClose = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
    HANDLE hThreadToClose = InterlockedExchangePointer((PVOID*)&plugin->pi.hThread, NULL);
    if (hProcToClose) {
        CloseHandle(hProcToClose);
    }
    if (hThreadToClose) {
        PluginProcess_CloseMonitorThreadHandle(hThreadToClose, waitForMonitorThread);
    }
    memset(&plugin->pi, 0, sizeof(plugin->pi));
}

/* Structure to pass data to the launcher thread */
typedef struct {
    HANDLE hReadySignalEvent;
    HANDLE hJob;
    BOOL success;
    wchar_t errorMsg[128];  /* Error message for display */
    volatile LONG readyState;
    volatile LONG refCount;
    PluginInfo pluginSnapshot;
} PluginLauncherArgs;

static void ClosePluginLaunchJobHandle(PluginLauncherArgs* args) {
    if (!args || !args->hJob) return;

    CloseHandle(args->hJob);
    args->hJob = NULL;
}

static BOOL SignalPluginLaunchReady(PluginLauncherArgs* args) {
    if (!args) return FALSE;

    HANDLE signalEvent = args->hReadySignalEvent;
    LONG previous = InterlockedCompareExchange(&args->readyState,
                                               PLUGIN_LAUNCH_READY_SIGNALED,
                                               PLUGIN_LAUNCH_READY_PENDING);
    if (previous != PLUGIN_LAUNCH_READY_PENDING) {
        return FALSE;
    }

    if (signalEvent) {
        if (!SetEvent(signalEvent)) {
            LOG_WARNING("[Process] Failed to signal plugin launch ready event: %lu",
                        GetLastError());
        }
        CloseHandle(signalEvent);
        args->hReadySignalEvent = NULL;
    }
    return TRUE;
}

static BOOL IsValidPluginNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static void ReleaseAbandonedPluginLaunchArgs(PluginLauncherArgs* args) {
    if (!args) return;

    if (InterlockedDecrement(&args->refCount) != 0) {
        return;
    }

    ClosePluginLaunchJobHandle(args);
    if (args->hReadySignalEvent) {
        CloseHandle(args->hReadySignalEvent);
        args->hReadySignalEvent = NULL;
    }
    free(args);
}

static DWORD FinishPluginLaunchFailure(PluginLauncherArgs* args) {
    ClosePluginLaunchJobHandle(args);
    SignalPluginLaunchReady(args);
    ReleaseAbandonedPluginLaunchArgs(args);
    return 0;
}

/**
 * @brief Get interpreter command for script file
 * Returns the interpreter to use, or NULL to use ShellExecute
 */
static const wchar_t* GetInterpreter(const wchar_t* path) {
    const wchar_t* ext = wcsrchr(path, L'.');
    if (!ext) return NULL;

    /* Python - use pythonw.exe (GUI subsystem, no console) */
    if (_wcsicmp(ext, L".py") == 0 || _wcsicmp(ext, L".pyw") == 0) {
        return L"pythonw.exe";
    }
    /* PowerShell - use -WindowStyle Hidden for complete hiding */
    if (_wcsicmp(ext, L".ps1") == 0) {
        return L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -NonInteractive -File";
    }
    /* Batch/CMD - use wscript to launch hidden (cmd.exe always creates console) */
    /* Note: CREATE_NO_WINDOW flag handles this, cmd.exe /c is fine */
    if (_wcsicmp(ext, L".bat") == 0 || _wcsicmp(ext, L".cmd") == 0) {
        return L"cmd.exe /c";
    }
    /* VBScript - use wscript.exe (GUI host) instead of cscript.exe (console) */
    if (_wcsicmp(ext, L".vbs") == 0 || _wcsicmp(ext, L".vbe") == 0) {
        return L"wscript.exe //nologo //B";
    }
    /* Node.js */
    if (_wcsicmp(ext, L".js") == 0 || _wcsicmp(ext, L".mjs") == 0 || _wcsicmp(ext, L".cjs") == 0) {
        return L"node.exe";
    }
    /* Lua */
    if (_wcsicmp(ext, L".lua") == 0) {
        return L"lua.exe";
    }
    /* Ruby - use rubyw.exe if available (GUI subsystem) */
    if (_wcsicmp(ext, L".rbw") == 0) {
        return L"rubyw.exe";
    }
    if (_wcsicmp(ext, L".rb") == 0) {
        return L"ruby.exe";
    }
    /* Perl - use wperl.exe if available (GUI subsystem) */
    if (_wcsicmp(ext, L".pl") == 0 || _wcsicmp(ext, L".pm") == 0) {
        return L"perl.exe";
    }
    /* PHP */
    if (_wcsicmp(ext, L".php") == 0) {
        return L"php.exe";
    }
    /* Shell scripts */
    if (_wcsicmp(ext, L".sh") == 0) {
        return L"bash.exe";
    }

    return NULL;
}

/**
 * @brief Get interpreter name only (without arguments) for error messages
 */
static const wchar_t* GetInterpreterName(const wchar_t* path) {
    const wchar_t* ext = wcsrchr(path, L'.');
    if (!ext) return NULL;

    if (_wcsicmp(ext, L".py") == 0 || _wcsicmp(ext, L".pyw") == 0) return L"pythonw.exe";
    if (_wcsicmp(ext, L".ps1") == 0) return L"powershell.exe";
    if (_wcsicmp(ext, L".bat") == 0 || _wcsicmp(ext, L".cmd") == 0) return L"cmd.exe";
    if (_wcsicmp(ext, L".vbs") == 0 || _wcsicmp(ext, L".vbe") == 0) return L"wscript.exe";
    if (_wcsicmp(ext, L".js") == 0 || _wcsicmp(ext, L".mjs") == 0 || _wcsicmp(ext, L".cjs") == 0) return L"node.exe";
    if (_wcsicmp(ext, L".lua") == 0) return L"lua.exe";
    if (_wcsicmp(ext, L".rbw") == 0) return L"rubyw.exe";
    if (_wcsicmp(ext, L".rb") == 0) return L"ruby.exe";
    if (_wcsicmp(ext, L".pl") == 0 || _wcsicmp(ext, L".pm") == 0) return L"perl.exe";
    if (_wcsicmp(ext, L".php") == 0) return L"php.exe";
    if (_wcsicmp(ext, L".sh") == 0) return L"bash.exe";

    return NULL;
}

/**
 * @brief Thread function to launch and monitor the plugin process
 */
static DWORD WINAPI PluginLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = &args->pluginSnapshot;

    args->errorMsg[0] = L'\0';  /* Clear error message */

    /* Extract working directory from plugin path */
    wchar_t workDir[MAX_PATH];
    wcsncpy(workDir, plugin->path, MAX_PATH - 1);
    workDir[MAX_PATH - 1] = L'\0';
    wchar_t* lastSlash = wcsrchr(workDir, L'\\');
    if (lastSlash) *lastSlash = L'\0';

    HANDLE hProcess = NULL;
    HANDLE hMonitorProcess = NULL;
    DWORD dwProcessId = 0;
    const wchar_t* interpreter = GetInterpreter(plugin->path);

    if (interpreter) {
        /* Use CreateProcess with interpreter */
        wchar_t cmdLine[MAX_PATH * 2 + 256];
        int written = _snwprintf_s(cmdLine, MAX_PATH * 2 + 256, _TRUNCATE,
                                   L"%s \"%s\"", interpreter, plugin->path);
        if (written < 0) {
            LOG_ERROR("[Thread] Command line too long for plugin: %ls", plugin->path);
            _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"Path too long");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }

        STARTUPINFOW si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {0};

        /* CREATE_NO_WINDOW prevents console window */
        if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                           CREATE_NO_WINDOW, NULL, workDir, &si, &pi)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Thread] CreateProcess FAILED! Error: %lu", error);
            LOG_ERROR("[Thread] Tip: Make sure '%ls' is installed and in PATH", interpreter);

            /* Set error message for display */
            const wchar_t* interpreterName = GetInterpreterName(plugin->path);
            if (interpreterName) {
                _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"%s not found", interpreterName);
            } else {
                _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"Launch failed");
            }

            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }

        hProcess = pi.hProcess;
        dwProcessId = pi.dwProcessId;
        if (pi.hThread) CloseHandle(pi.hThread);
    } else {
        /* Use ShellExecute for unknown types */
        SHELLEXECUTEINFOW sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"open";
        sei.lpFile = plugin->path;
        sei.lpDirectory = workDir;
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Thread] ShellExecuteEx FAILED! Error: %lu", error);
            _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"Launch failed");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }

        if (!sei.hProcess) {
            LOG_WARNING("[Thread] ShellExecute succeeded but no process handle");
            _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"No process");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }

        hProcess = sei.hProcess;
        dwProcessId = GetProcessId(sei.hProcess);
        if (dwProcessId == 0) {
            DWORD pidError = GetLastError();
            LOG_ERROR("[Thread] Failed to get ShellExecute process id: %lu", pidError);
            CloseHandle(hProcess);
            _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"Launch failed");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
    }

    if (hProcess) {
        if (!DuplicateHandle(GetCurrentProcess(), hProcess, GetCurrentProcess(),
                             &hMonitorProcess, SYNCHRONIZE,
                             FALSE, 0)) {
            DWORD dupError = GetLastError();
            LOG_ERROR("[Thread] Failed to duplicate monitor handle: %lu", dupError);
            if (dwProcessId != 0) {
                TerminateProcessTree(dwProcessId, 0);
            }
            CloseHandle(hProcess);
            _snwprintf_s(args->errorMsg, 128, _TRUNCATE, L"Launch failed");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
    }

    /* Assign to Job Object for automatic cleanup */
    if (args->hJob && hProcess) {
        if (!AssignProcessToJobObject(args->hJob, hProcess)) {
            DWORD jobError = GetLastError();
            if (jobError != 5) {
                LOG_WARNING("[Thread] Failed to assign to Job: %lu (will terminate via StopPlugin)", jobError);
            }
        }
    }
    ClosePluginLaunchJobHandle(args);

    /* Store process info */
    plugin->pi.hProcess = hProcess;
    plugin->pi.hThread = NULL;
    plugin->pi.dwProcessId = dwProcessId;
    plugin->pi.dwThreadId = 0;

    /* Signal success */
    plugin->isRunning = TRUE;
    args->success = TRUE;

    if (!SignalPluginLaunchReady(args)) {
        if (dwProcessId != 0) {
            TerminateProcessTree(dwProcessId, 0);
        }
        if (hMonitorProcess) {
            CloseHandle(hMonitorProcess);
        }
        if (hProcess) {
            CloseHandle(hProcess);
        }
        ReleaseAbandonedPluginLaunchArgs(args);
        return 0;
    }
    ReleaseAbandonedPluginLaunchArgs(args);

    /* If we have a process handle, wait for it to exit */
    if (hMonitorProcess) {
        DWORD monitorWaitResult = WaitForSingleObject(hMonitorProcess, INFINITE);

        if (monitorWaitResult != WAIT_OBJECT_0) {
            LOG_WARNING("[Thread] Wait failed (Result: %lu, Error: %lu)", monitorWaitResult, GetLastError());
        }

        /* Clean up any child processes that may still be running */
        /* This handles cases like: cmd.exe exits but PowerShell child is still running */
        if (dwProcessId != 0) {
            TerminateProcessTree(dwProcessId, 0);
            PluginManager_HandleProcessExit(dwProcessId);
        }
        CloseHandle(hMonitorProcess);
    }

    return 0;
}

BOOL PluginProcess_Init(void) {
    g_hNotifyWnd = NULL;

    g_hJob = CreateJobObject(NULL, NULL);
    if (g_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        /* KILL_ON_JOB_CLOSE: Kill all processes when Job handle is closed */
        /* This ensures plugins are killed even if Catime crashes */
        /* Without BREAKAWAY flags: Child processes inherit Job membership */
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            LOG_ERROR("[Process] Failed to configure Job Object, error: %lu", GetLastError());
            CloseHandle(g_hJob);
            g_hJob = NULL;
            return FALSE;
        }

        return TRUE;
    }
    LOG_ERROR("[Process] Failed to create Job Object, error: %lu", GetLastError());
    return FALSE;
}

void PluginProcess_Shutdown(void) {
    g_hNotifyWnd = NULL;

    if (g_hJob) {
        CloseHandle(g_hJob);
        g_hJob = NULL;
    }
}

/* Last error message from plugin launch */
static wchar_t g_lastLaunchError[128] = {0};
static DWORD g_launchStartFailureCooldownUntil = 0;

static BOOL IsPluginLaunchStartFailureCoolingDown(DWORD now) {
    return g_launchStartFailureCooldownUntil != 0 &&
           (LONG)(g_launchStartFailureCooldownUntil - now) > 0;
}

static void MarkPluginLaunchStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PLUGIN_LAUNCH_START_FAILURE_COOLDOWN_MS;
    g_launchStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

/**
 * @brief Get last launch error message
 */
const wchar_t* PluginProcess_GetLastError(void) {
    return g_lastLaunchError;
}

/**
 * @brief Set last launch error message
 */
void PluginProcess_SetLastError(const wchar_t* errorMsg) {
    if (errorMsg) {
        wcsncpy(g_lastLaunchError, errorMsg, 127);
        g_lastLaunchError[127] = L'\0';
    } else {
        g_lastLaunchError[0] = L'\0';
    }
}

BOOL PluginProcess_Launch(PluginInfo* plugin) {
    if (!plugin) return FALSE;

    g_lastLaunchError[0] = L'\0';  /* Clear last error */
    DWORD now = GetTickCount();
    if (IsPluginLaunchStartFailureCoolingDown(now)) {
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    /* Prepare launcher thread arguments */
    PluginLauncherArgs* args = (PluginLauncherArgs*)calloc(1, sizeof(PluginLauncherArgs));
    if (!args) {
        LOG_ERROR("[Process] Failed to allocate launcher args");
        MarkPluginLaunchStartFailure(now);
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    args->pluginSnapshot = *plugin;
    args->success = FALSE;
    args->errorMsg[0] = L'\0';
    args->readyState = PLUGIN_LAUNCH_READY_PENDING;
    args->refCount = 1;

    HANDLE hReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hReadyEvent) {
        LOG_ERROR("[Process] Failed to create sync event");
        free(args);
        MarkPluginLaunchStartFailure(now);
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    if (g_hJob &&
        !DuplicateHandle(GetCurrentProcess(), g_hJob,
                         GetCurrentProcess(), &args->hJob,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_ERROR("[Process] Failed to duplicate Job Object handle: %lu", GetLastError());
        CloseHandle(hReadyEvent);
        free(args);
        MarkPluginLaunchStartFailure(now);
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    if (!DuplicateHandle(GetCurrentProcess(), hReadyEvent,
                         GetCurrentProcess(), &args->hReadySignalEvent,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_ERROR("[Process] Failed to duplicate sync event handle: %lu", GetLastError());
        ClosePluginLaunchJobHandle(args);
        CloseHandle(hReadyEvent);
        free(args);
        MarkPluginLaunchStartFailure(now);
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    /* Create launcher thread */
    InterlockedIncrement(&args->refCount);
    HANDLE hThread = CreateThread(NULL, 0, PluginLauncherThread, args, 0, NULL);
    if (!hThread) {
        LOG_ERROR("[Process] Failed to create launcher thread");
        ClosePluginLaunchJobHandle(args);
        CloseHandle(hReadyEvent);
        CloseHandle(args->hReadySignalEvent);
        args->hReadySignalEvent = NULL;
        ReleaseAbandonedPluginLaunchArgs(args);
        ReleaseAbandonedPluginLaunchArgs(args);
        MarkPluginLaunchStartFailure(now);
        wcscpy_s(g_lastLaunchError, 128, L"Internal error");
        return FALSE;
    }

    g_launchStartFailureCooldownUntil = 0;

    /* Wait for process creation */
    DWORD readyWait = WaitForSingleObject(hReadyEvent, PLUGIN_LAUNCH_READY_TIMEOUT_MS);
    CloseHandle(hReadyEvent);
    hReadyEvent = NULL;

    if (readyWait != WAIT_OBJECT_0) {
        LONG previous = InterlockedCompareExchange(&args->readyState,
                                                   PLUGIN_LAUNCH_READY_ABANDONED,
                                                   PLUGIN_LAUNCH_READY_PENDING);
        if (previous == PLUGIN_LAUNCH_READY_PENDING) {
            if (readyWait == WAIT_TIMEOUT) {
                LOG_ERROR("[Process] Plugin launch timed out: %ls", plugin->displayName);
                wcscpy_s(g_lastLaunchError, 128, L"Launch timed out");
            } else {
                LOG_ERROR("[Process] Plugin launch wait failed (wait=%lu, error=%lu)",
                          readyWait, GetLastError());
                wcscpy_s(g_lastLaunchError, 128, L"Launch failed");
            }
            CloseHandle(hThread);
            ReleaseAbandonedPluginLaunchArgs(args);
            return FALSE;
        }
        if (previous != PLUGIN_LAUNCH_READY_SIGNALED) {
            LOG_ERROR("[Process] Unexpected plugin launch state after wait failure: %ld",
                      previous);
            CloseHandle(hThread);
            ReleaseAbandonedPluginLaunchArgs(args);
            return FALSE;
        }
    }

    if (args->success && args->pluginSnapshot.isRunning &&
        args->pluginSnapshot.pi.dwProcessId != 0) {
        *plugin = args->pluginSnapshot;
        plugin->pi.hThread = hThread;
    } else {
        DWORD threadWait = WaitForSingleObject(hThread, PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS);
        if (threadWait == WAIT_TIMEOUT) {
            LOG_WARNING("[Process] Launcher thread did not exit after failed launch within %lu ms",
                        (DWORD)PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS);
        } else if (threadWait == WAIT_FAILED) {
            LOG_WARNING("[Process] Failed waiting for launcher thread after failed launch: %lu",
                        GetLastError());
        }
        CloseHandle(hThread);
    }

    /* Copy error message if failed */
    if (!args->success && args->errorMsg[0] != L'\0') {
        wcscpy_s(g_lastLaunchError, 128, args->errorMsg);
    }

    BOOL success = args->success;
    ReleaseAbandonedPluginLaunchArgs(args);
    return success;
}

static void TerminateSingleProcess(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (hProc) {
        TerminateProcess(hProc, 0);
        /* Brief wait to ensure termination */
        WaitForSingleObject(hProc, 500);
        CloseHandle(hProc);
    }
}

static BOOL HasVisitedProcessId(const DWORD* visitedPids, DWORD visitedCount, DWORD pid) {
    if (!visitedPids) return FALSE;
    for (DWORD i = 0; i < visitedCount; i++) {
        if (visitedPids[i] == pid) {
            return TRUE;
        }
    }
    return FALSE;
}

static void TerminateProcessTreeSlowVisited(DWORD pid,
                                            int depth,
                                            DWORD* visitedPids,
                                            DWORD visitedCount) {
    /* Safety: prevent infinite recursion and skip invalid PIDs */
    if (pid == 0 || pid == GetCurrentProcessId() ||
        depth > PROCESS_TREE_MAX_DEPTH ||
        HasVisitedProcessId(visitedPids, visitedCount, pid)) {
        return;
    }
    if (visitedCount < PROCESS_TREE_VISITED_CAPACITY) {
        visitedPids[visitedCount++] = pid;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {0};
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ParentProcessID == pid && pe.th32ProcessID != pid) {
                    /* Found a child process - recurse first (depth-first termination) */
                    TerminateProcessTreeSlowVisited(pe.th32ProcessID,
                                                    depth + 1,
                                                    visitedPids,
                                                    visitedCount);
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }

    TerminateSingleProcess(pid);
}

static void TerminateProcessTreeSlow(DWORD pid, int depth) {
    DWORD visitedPids[PROCESS_TREE_VISITED_CAPACITY] = {0};
    TerminateProcessTreeSlowVisited(pid, depth, visitedPids, 0);
}

static void TerminateProcessTreeFromSnapshotVisited(const ProcessTreeEntry* entries,
                                                    DWORD count,
                                                    DWORD pid,
                                                    int depth,
                                                    DWORD* visitedPids,
                                                    DWORD visitedCount) {
    if (pid == 0 || pid == GetCurrentProcessId() ||
        depth > PROCESS_TREE_MAX_DEPTH ||
        HasVisitedProcessId(visitedPids, visitedCount, pid)) {
        return;
    }
    if (visitedCount < PROCESS_TREE_VISITED_CAPACITY) {
        visitedPids[visitedCount++] = pid;
    }

    for (DWORD i = 0; i < count; i++) {
        if (entries[i].parentProcessId == pid && entries[i].processId != pid) {
            TerminateProcessTreeFromSnapshotVisited(entries,
                                                    count,
                                                    entries[i].processId,
                                                    depth + 1,
                                                    visitedPids,
                                                    visitedCount);
        }
    }

    TerminateSingleProcess(pid);
}

static void TerminateProcessTreeFromSnapshot(const ProcessTreeEntry* entries,
                                             DWORD count,
                                             DWORD pid,
                                             int depth) {
    DWORD visitedPids[PROCESS_TREE_VISITED_CAPACITY] = {0};
    TerminateProcessTreeFromSnapshotVisited(entries, count, pid, depth, visitedPids, 0);
}

/**
 * @brief Recursively terminate a process and all its descendants
 * @param pid Process ID to terminate
 * @param depth Current recursion depth (for logging and safety)
 */
static void TerminateProcessTree(DWORD pid, int depth) {
    if (pid == 0 || pid == GetCurrentProcessId() || depth > PROCESS_TREE_MAX_DEPTH) return;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        TerminateSingleProcess(pid);
        return;
    }

    DWORD capacity = PROCESS_TREE_STACK_CAPACITY;
    DWORD count = 0;
    ProcessTreeEntry stackEntries[PROCESS_TREE_STACK_CAPACITY];
    ProcessTreeEntry* entries = stackEntries;

    BOOL snapshotComplete = TRUE;
    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (count >= capacity) {
                if (!GrowProcessTreeSnapshot(&entries, &capacity, count, stackEntries)) {
                    snapshotComplete = FALSE;
                    break;
                }
            }

            entries[count].processId = pe.th32ProcessID;
            entries[count].parentProcessId = pe.th32ParentProcessID;
            count++;
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    if (snapshotComplete) {
        TerminateProcessTreeFromSnapshot(entries, count, pid, depth);
    } else {
        TerminateProcessTreeSlow(pid, depth);
    }
    if (entries != stackEntries) {
        free(entries);
    }
}

/**
 * @brief Terminate all processes in Job Object except Catime itself
 * This ensures orphaned child processes are cleaned up even if main process already exited
 */
static void TerminateAllJobProcesses(void) {
    if (!g_hJob) return;

    JOBOBJECT_BASIC_PROCESS_ID_LIST pidList;
    pidList.NumberOfAssignedProcesses = 0;
    pidList.NumberOfProcessIdsInList = 0;

    /* First call to get count */
    DWORD returnLength = 0;
    QueryInformationJobObject(g_hJob, JobObjectBasicProcessIdList,
                              &pidList, sizeof(pidList), &returnLength);

    if (pidList.NumberOfAssignedProcesses == 0) {
        /* No plugin processes assigned */
        return;
    }

    /* Allocate buffer for all PIDs; most plugin jobs fit in the stack path. */
    size_t bufSize = 0;
    if (!GetJobProcessIdListBufferSize(pidList.NumberOfAssignedProcesses, &bufSize)) {
        return;
    }

    union {
        JOBOBJECT_BASIC_PROCESS_ID_LIST list;
        BYTE bytes[sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
                   ((JOB_PROCESS_STACK_CAPACITY - 1) * sizeof(ULONG_PTR))];
    } stackList;
    JOBOBJECT_BASIC_PROCESS_ID_LIST* fullList =
        (pidList.NumberOfAssignedProcesses <= JOB_PROCESS_STACK_CAPACITY)
            ? (JOBOBJECT_BASIC_PROCESS_ID_LIST*)stackList.bytes
            : (JOBOBJECT_BASIC_PROCESS_ID_LIST*)malloc(bufSize);

    if (!fullList) return;

    fullList->NumberOfAssignedProcesses = pidList.NumberOfAssignedProcesses;
    fullList->NumberOfProcessIdsInList = 0;

    if (QueryInformationJobObject(g_hJob, JobObjectBasicProcessIdList,
                                  fullList, (DWORD)bufSize, &returnLength)) {
        /* Terminate assigned plugin processes; skip Catime defensively. */
        DWORD catimePid = GetCurrentProcessId();
        for (DWORD i = 0; i < fullList->NumberOfProcessIdsInList; i++) {
            DWORD childPid = (DWORD)fullList->ProcessIdList[i];
            if (childPid != catimePid && childPid != 0) {
                HANDLE hChild = OpenProcess(PROCESS_TERMINATE, FALSE, childPid);
                if (hChild) {
                    TerminateProcess(hChild, 0);
                    CloseHandle(hChild);
                }
            }
        }
    }
    if (fullList != (JOBOBJECT_BASIC_PROCESS_ID_LIST*)stackList.bytes) {
        free(fullList);
    }
}

BOOL PluginProcess_TerminateDetached(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning) return FALSE;

    HANDLE hProc = plugin->pi.hProcess;
    HANDLE hThread = plugin->pi.hThread;
    DWORD pid = plugin->pi.dwProcessId;
    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));

    if (pid != 0) {
        TerminateProcessTree(pid, 0);
    }

    if (hProc) {
        TerminateProcess(hProc, 0);
        WaitForSingleObject(hProc, 2000);
        CloseHandle(hProc);
    }

    if (hThread) {
        PluginProcess_CloseMonitorThreadHandle(hThread, TRUE);
    }

    return TRUE;
}

void PluginProcess_TerminateAllOrphans(void) {
    TerminateAllJobProcesses();
}

BOOL PluginProcess_IsAlive(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning) {
        return FALSE;
    }

    HANDLE hProc = plugin->pi.hProcess;
    if (!hProc) {
        /* No handle but marked running - trust the flag */
        return plugin->isRunning;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProc, &exitCode)) {
        DWORD error = GetLastError();
        LOG_WARNING("[Process] GetExitCodeProcess failed for plugin pid %lu: %lu",
                    plugin->pi.dwProcessId, error);
        if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            ClearPluginProcessHandles(plugin, FALSE);
        }
        return FALSE;
    }

    if (exitCode != STILL_ACTIVE) {
        /* Update state to reflect actual process status */
        if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            ClearPluginProcessHandles(plugin, FALSE);
        }
        return FALSE;
    }
    return TRUE;
}

void PluginProcess_SetNotifyWindow(HWND hwnd) {
    g_hNotifyWnd = IsValidPluginNotifyWindow(hwnd) ? hwnd : NULL;
}

HWND PluginProcess_GetNotifyWindow(void) {
    HWND hwnd = g_hNotifyWnd;
    if (!IsValidPluginNotifyWindow(hwnd)) {
        g_hNotifyWnd = NULL;
        return NULL;
    }
    return hwnd;
}
