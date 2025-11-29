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

/* Job Object for automatic cleanup when Catime exits */
static HANDLE g_hJob = NULL;
static HWND g_hNotifyWnd = NULL;

/* Structure to pass data to the launcher thread */
typedef struct {
    PluginInfo* plugin;
    HANDLE hReadyEvent;
    BOOL success;
} PluginLauncherArgs;

/**
 * @brief Get interpreter command for script file
 * Returns the interpreter to use, or NULL to use ShellExecute
 */
static const char* GetInterpreter(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return NULL;
    
    /* Python - use pythonw.exe (GUI subsystem, no console) */
    if (_stricmp(ext, ".py") == 0 || _stricmp(ext, ".pyw") == 0) {
        return "pythonw.exe";
    }
    /* PowerShell - use -WindowStyle Hidden for complete hiding */
    if (_stricmp(ext, ".ps1") == 0) {
        return "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -NonInteractive -File";
    }
    /* Batch/CMD - use wscript to launch hidden (cmd.exe always creates console) */
    /* Note: CREATE_NO_WINDOW flag handles this, cmd.exe /c is fine */
    if (_stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0) {
        return "cmd.exe /c";
    }
    /* VBScript - use wscript.exe (GUI host) instead of cscript.exe (console) */
    if (_stricmp(ext, ".vbs") == 0 || _stricmp(ext, ".vbe") == 0) {
        return "wscript.exe //nologo //B";
    }
    /* Node.js */
    if (_stricmp(ext, ".js") == 0 || _stricmp(ext, ".mjs") == 0 || _stricmp(ext, ".cjs") == 0) {
        return "node.exe";
    }
    /* Lua */
    if (_stricmp(ext, ".lua") == 0) {
        return "lua.exe";
    }
    /* Ruby - use rubyw.exe if available (GUI subsystem) */
    if (_stricmp(ext, ".rbw") == 0) {
        return "rubyw.exe";
    }
    if (_stricmp(ext, ".rb") == 0) {
        return "ruby.exe";
    }
    /* Perl - use wperl.exe if available (GUI subsystem) */
    if (_stricmp(ext, ".pl") == 0 || _stricmp(ext, ".pm") == 0) {
        return "perl.exe";
    }
    /* PHP */
    if (_stricmp(ext, ".php") == 0) {
        return "php.exe";
    }
    /* Shell scripts */
    if (_stricmp(ext, ".sh") == 0) {
        return "bash.exe";
    }
    
    return NULL;
}

/**
 * @brief Thread function to launch and monitor the plugin process
 */
static DWORD WINAPI PluginLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = args->plugin;
    
    LOG_INFO("[Thread] ========== LAUNCH START ==========");
    LOG_INFO("[Thread] Plugin name: %s", plugin->displayName);
    LOG_INFO("[Thread] Plugin path: %s", plugin->path);
    
    /* Extract working directory from plugin path */
    char workDir[MAX_PATH];
    strncpy(workDir, plugin->path, sizeof(workDir) - 1);
    workDir[sizeof(workDir) - 1] = '\0';
    char* lastSlash = strrchr(workDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    LOG_INFO("[Thread] Work directory: %s", workDir);
    
    HANDLE hProcess = NULL;
    DWORD dwProcessId = 0;
    const char* interpreter = GetInterpreter(plugin->path);
    
    if (interpreter) {
        /* Use CreateProcess with interpreter */
        char cmdLine[MAX_PATH * 2 + 256];
        snprintf(cmdLine, sizeof(cmdLine), "%s \"%s\"", interpreter, plugin->path);
        LOG_INFO("[Thread] Command: %s", cmdLine);
        
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {0};
        
        /* CREATE_NO_WINDOW prevents console window */
        if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                           CREATE_NO_WINDOW, NULL, workDir, &si, &pi)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Thread] CreateProcess FAILED! Error: %lu", error);
            LOG_ERROR("[Thread] Tip: Make sure '%s' is installed and in PATH", interpreter);
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        LOG_INFO("[Thread] CreateProcess SUCCESS! PID: %lu", pi.dwProcessId);
        hProcess = pi.hProcess;
        dwProcessId = pi.dwProcessId;
        if (pi.hThread) CloseHandle(pi.hThread);
    } else {
        /* Use ShellExecute for unknown types */
        SHELLEXECUTEINFOA sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = "open";
        sei.lpFile = plugin->path;
        sei.lpDirectory = workDir;
        sei.nShow = SW_HIDE;
        
        LOG_INFO("[Thread] Using ShellExecute for: %s", plugin->path);
        
        if (!ShellExecuteExA(&sei)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Thread] ShellExecuteEx FAILED! Error: %lu", error);
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        if (!sei.hProcess) {
            LOG_WARNING("[Thread] ShellExecute succeeded but no process handle");
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        hProcess = sei.hProcess;
        dwProcessId = GetProcessId(sei.hProcess);
        LOG_INFO("[Thread] ShellExecute SUCCESS! PID: %lu", dwProcessId);
    }
    
    /* Assign to Job Object for automatic cleanup */
    if (g_hJob && hProcess) {
        if (AssignProcessToJobObject(g_hJob, hProcess)) {
            LOG_INFO("[Thread] Process assigned to Job Object");
        } else {
            DWORD jobError = GetLastError();
            if (jobError == 5) {
                /* Process already in a job - on Windows 8+, processes can be in multiple jobs */
                LOG_INFO("[Thread] Process already in a job (OK - will terminate via StopPlugin)");
            } else {
                LOG_WARNING("[Thread] Failed to assign to Job: %lu (will terminate via StopPlugin)", jobError);
            }
        }
    }
    
    LOG_INFO("[Thread] ========== LAUNCH SUCCESS ==========");
    
    /* Store process info */
    plugin->pi.hProcess = hProcess;
    plugin->pi.hThread = NULL;
    plugin->pi.dwProcessId = dwProcessId;
    plugin->pi.dwThreadId = 0;
    
    /* Signal success */
    plugin->isRunning = TRUE;
    args->success = TRUE;
    SetEvent(args->hReadyEvent);
    
    /* If we have a process handle, wait for it to exit */
    if (hProcess) {
        LOG_INFO("[Thread] Monitoring process...");
        DWORD startTime = GetTickCount();
        
        DWORD monitorWaitResult = WaitForSingleObject(hProcess, INFINITE);
        
        DWORD runDuration = GetTickCount() - startTime;
        DWORD scriptExitCode = 0;
        
        if (monitorWaitResult == WAIT_OBJECT_0) {
            GetExitCodeProcess(hProcess, &scriptExitCode);
            LOG_INFO("[Thread] Script exited (Code: %lu, Duration: %lu ms)", scriptExitCode, runDuration);
        } else {
            LOG_WARNING("[Thread] Wait failed (Result: %lu, Error: %lu)", monitorWaitResult, GetLastError());
        }
        
        /* Handle process exit */
        if (plugin->isRunning) {
            if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
                LOG_INFO("[Thread] Plugin exited: %s", plugin->displayName);
                
                HANDLE hProc = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
                if (hProc) {
                    CloseHandle(hProc);
                }
                memset(&plugin->pi, 0, sizeof(plugin->pi));
                
                if (runDuration >= 5000) {
                    PluginData_Clear();
                }
                
                if (g_hNotifyWnd) {
                    InvalidateRect(g_hNotifyWnd, NULL, TRUE);
                }
            }
        }
    }
    
    return 0;
}

BOOL PluginProcess_Init(void) {
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
        
        /* Add Catime itself to the Job Object */
        /* This ensures: when Catime exits (normal or crash), Job handle closes, */
        /* triggering KILL_ON_JOB_CLOSE to terminate all plugin processes */
        if (!AssignProcessToJobObject(g_hJob, GetCurrentProcess())) {
            DWORD error = GetLastError();
            /* Error 5 (Access Denied) means we're already in a job - that's OK on Windows 8+ */
            if (error != 5) {
                LOG_WARNING("[Process] Failed to add Catime to Job: %lu", error);
            } else {
                LOG_INFO("[Process] Catime already in a job (nested jobs supported on Win8+)");
            }
        } else {
            LOG_INFO("[Process] Catime added to Job Object");
        }
        
        LOG_INFO("[Process] Job Object initialized (plugins will be killed on Catime exit/crash)");
        return TRUE;
    }
    LOG_ERROR("[Process] Failed to create Job Object, error: %lu", GetLastError());
    return FALSE;
}

void PluginProcess_Shutdown(void) {
    if (g_hJob) {
        CloseHandle(g_hJob);
        g_hJob = NULL;
    }
    LOG_INFO("[Process] Process management shutdown");
}

BOOL PluginProcess_Launch(PluginInfo* plugin) {
    if (!plugin) return FALSE;
    
    LOG_INFO("[Process] Launching plugin: %s", plugin->displayName);
    
    /* Prepare launcher thread arguments */
    PluginLauncherArgs args = {0};
    args.plugin = plugin;
    args.hReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    args.success = FALSE;
    
    if (!args.hReadyEvent) {
        LOG_ERROR("[Process] Failed to create sync event");
        return FALSE;
    }
    
    /* Create launcher thread */
    HANDLE hThread = CreateThread(NULL, 0, PluginLauncherThread, &args, 0, NULL);
    if (!hThread) {
        LOG_ERROR("[Process] Failed to create launcher thread");
        CloseHandle(args.hReadyEvent);
        return FALSE;
    }
    
    /* Wait for process creation */
    WaitForSingleObject(args.hReadyEvent, INFINITE);
    CloseHandle(args.hReadyEvent);
    CloseHandle(hThread);
    
    return args.success;
}

BOOL PluginProcess_Terminate(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning) return FALSE;
    
    LOG_INFO("[Process] Terminating plugin: %s", plugin->displayName);
    
    /* Atomically mark as not running */
    if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) != TRUE) {
        LOG_INFO("[Process] Already terminated");
        return FALSE;
    }
    
    /* Get and clear process handle atomically */
    HANDLE hProc = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
    HANDLE hThread = plugin->pi.hThread;
    DWORD pid = plugin->pi.dwProcessId;
    plugin->pi.hThread = NULL;
    memset(&plugin->pi, 0, sizeof(plugin->pi));
    
    if (hProc) {
        LOG_INFO("[Process] Terminating process PID: %lu", pid);
        TerminateProcess(hProc, 0);
        WaitForSingleObject(hProc, 2000);
        CloseHandle(hProc);
        LOG_INFO("[Process] Process terminated");
    } else {
        LOG_WARNING("[Process] No process handle available");
    }
    
    if (hThread) {
        CloseHandle(hThread);
    }
    
    return TRUE;
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
        return FALSE;
    }
    
    if (exitCode != STILL_ACTIVE) {
        LOG_INFO("[Process] IsAlive: process has exited");
        return FALSE;
    }
    return TRUE;
}

void PluginProcess_SetNotifyWindow(HWND hwnd) {
    g_hNotifyWnd = hwnd;
}

HWND PluginProcess_GetNotifyWindow(void) {
    return g_hNotifyWnd;
}
