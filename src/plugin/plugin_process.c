/**
 * @file plugin_process.c
 * @brief Plugin process lifecycle management
 */

#include "plugin/plugin_process.h"
#include "plugin/plugin_data.h"
#include "log.h"
#include <shellapi.h>
#include <string.h>

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
    
    /* Python - pythonw.exe is the windowless version */
    if (_stricmp(ext, ".py") == 0 || _stricmp(ext, ".pyw") == 0) {
        return "pythonw.exe";
    }
    /* PowerShell */
    if (_stricmp(ext, ".ps1") == 0) {
        return "powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File";
    }
    /* Batch/CMD */
    if (_stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0) {
        return "cmd.exe /c";
    }
    /* VBScript - use cscript for console-less execution */
    if (_stricmp(ext, ".vbs") == 0 || _stricmp(ext, ".vbe") == 0) {
        return "cscript.exe //nologo //B";
    }
    /* Windows Script Host */
    if (_stricmp(ext, ".wsf") == 0 || _stricmp(ext, ".wsh") == 0) {
        return "cscript.exe //nologo //B";
    }
    /* JScript (encrypted) */
    if (_stricmp(ext, ".jse") == 0) {
        return "cscript.exe //nologo //B";
    }
    /* Node.js - .js/.mjs/.cjs all use node.exe */
    if (_stricmp(ext, ".js") == 0 || _stricmp(ext, ".mjs") == 0 || _stricmp(ext, ".cjs") == 0) {
        return "node.exe";
    }
    /* Lua */
    if (_stricmp(ext, ".lua") == 0) {
        return "lua.exe";
    }
    /* Ruby */
    if (_stricmp(ext, ".rb") == 0 || _stricmp(ext, ".rbw") == 0) {
        return "ruby.exe";
    }
    /* Perl */
    if (_stricmp(ext, ".pl") == 0 || _stricmp(ext, ".pm") == 0) {
        return "perl.exe";
    }
    /* PHP */
    if (_stricmp(ext, ".php") == 0) {
        return "php.exe";
    }
    /* Shell scripts (Git Bash) */
    if (_stricmp(ext, ".sh") == 0) {
        return "bash.exe";
    }
    
    /* Other types - use ShellExecute */
    return NULL;
}

/**
 * @brief Thread function to launch and monitor the plugin process
 */
static DWORD WINAPI PluginLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = args->plugin;
    
    /* Extract working directory from plugin path */
    char workDir[MAX_PATH];
    strncpy(workDir, plugin->path, sizeof(workDir) - 1);
    workDir[sizeof(workDir) - 1] = '\0';
    char* lastSlash = strrchr(workDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    
    HANDLE hProcess = NULL;
    const char* interpreter = GetInterpreter(plugin->path);
    
    if (interpreter) {
        /* Use CreateProcess with specific interpreter */
        char cmdLine[MAX_PATH * 2 + 128];
        snprintf(cmdLine, sizeof(cmdLine), "%s \"%s\"", interpreter, plugin->path);
        
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {0};
        
        /* CREATE_NO_WINDOW prevents console window for console apps */
        DWORD creationFlags = CREATE_NO_WINDOW;
        
        if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                           creationFlags, NULL, workDir, &si, &pi)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Process] CreateProcess FAILED! Error: %lu", error);
            LOG_ERROR("[Process] Tip: Make sure '%s' is installed and in PATH", interpreter);
            
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        LOG_INFO("[Process] Launched with interpreter, PID: %lu", pi.dwProcessId);
        
        hProcess = pi.hProcess;
        if (pi.hThread) CloseHandle(pi.hThread);
        plugin->pi.dwProcessId = pi.dwProcessId;
    } else {
        /* Use ShellExecute for other types */
        SHELLEXECUTEINFOA sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = "open";
        sei.lpFile = plugin->path;
        sei.lpDirectory = workDir;
        sei.nShow = SW_HIDE;
        
        LOG_INFO("[Process] Launching with ShellExecute: %s", plugin->path);
        
        if (!ShellExecuteExA(&sei)) {
            DWORD error = GetLastError();
            LOG_ERROR("[Process] ShellExecuteEx failed! Error: %lu", error);
            
            if (error == ERROR_NO_ASSOCIATION) {
                LOG_ERROR("Tip: No program associated with this file type");
            }
            
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        /* ShellExecute may not return a process handle for all file types */
        if (!sei.hProcess) {
            LOG_WARNING("[Process] ShellExecute succeeded but no process handle returned");
            LOG_WARNING("[Process] The script may have started but cannot be monitored");
            args->success = FALSE;
            SetEvent(args->hReadyEvent);
            return 0;
        }
        
        hProcess = sei.hProcess;
        plugin->pi.dwProcessId = GetProcessId(sei.hProcess);
    }
    
    /* Store process handle */
    plugin->pi.hProcess = hProcess;
    plugin->pi.hThread = NULL;
    plugin->pi.dwThreadId = 0;
    
    LOG_INFO("[Process] Script launched successfully. PID: %lu", plugin->pi.dwProcessId);
    
    /* Duplicate handle for safe waiting - this is important to avoid race conditions */
    HANDLE hWaitProcess = NULL;
    BOOL handleDuplicated = DuplicateHandle(GetCurrentProcess(), plugin->pi.hProcess,
                                            GetCurrentProcess(), &hWaitProcess,
                                            0, FALSE, DUPLICATE_SAME_ACCESS);
    if (!handleDuplicated) {
        LOG_WARNING("[Process] Failed to duplicate handle, using original");
        hWaitProcess = plugin->pi.hProcess;
    }
    
    /* Assign to Job Object for lifecycle management */
    if (g_hJob && hProcess) {
        if (!AssignProcessToJobObject(g_hJob, hProcess)) {
            LOG_WARNING("[Process] Failed to assign to Job Object, error: %lu", GetLastError());
        }
    }
    
    /* Signal success */
    plugin->isRunning = TRUE;
    args->success = TRUE;
    SetEvent(args->hReadyEvent);
    
    /* Record start time */
    DWORD startTime = GetTickCount();
    
    /* Wait for process exit */
    DWORD waitResult = WaitForSingleObject(hWaitProcess, INFINITE);
    
    DWORD runDuration = GetTickCount() - startTime;
    DWORD exitCode = 0;
    
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(hWaitProcess, &exitCode);
        LOG_INFO("[Process] Plugin exited (Code: %lu, Duration: %lu ms)", exitCode, runDuration);
    } else {
        LOG_WARNING("[Process] Wait failed or handle invalid (Result: %lu, Error: %lu)", 
                    waitResult, GetLastError());
    }
    
    /* Cleanup duplicate handle - only close if we successfully duplicated */
    if (handleDuplicated && hWaitProcess) {
        CloseHandle(hWaitProcess);
    }
    
    /* Handle unexpected exit (plugin crashed or finished on its own) */
    /* Use memory barrier to ensure we see the latest value */
    MemoryBarrier();
    if (plugin->isRunning) {
        /* Atomically check and clear isRunning to prevent race with Terminate */
        if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            LOG_INFO("[Process] Plugin %s exited unexpectedly", plugin->displayName);
            
            /* Atomically swap handle to NULL - only one thread can get non-NULL */
            HANDLE hProc = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
            if (hProc) {
                CloseHandle(hProc);
            }
            memset(&plugin->pi, 0, sizeof(plugin->pi));
            
            /* Quick exit might indicate missing interpreter */
            if (runDuration < 5000) {
                LOG_INFO("[Process] Quick exit (%lu ms), keeping hot-reload active", runDuration);
            } else {
                PluginData_Clear();
            }
            
            /* Trigger UI refresh */
            if (g_hNotifyWnd) {
                InvalidateRect(g_hNotifyWnd, NULL, TRUE);
            }
        }
    }
    
    return 0;
}

BOOL PluginProcess_Init(void) {
    g_hJob = CreateJobObject(NULL, NULL);
    if (g_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            LOG_ERROR("[Process] Failed to configure Job Object, error: %lu", GetLastError());
            CloseHandle(g_hJob);
            g_hJob = NULL;
            return FALSE;
        }
        LOG_INFO("[Process] Job Object initialized");
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
    
    /* Atomically mark as not running to prevent race with launcher thread */
    if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) != TRUE) {
        /* Already terminated by launcher thread */
        return FALSE;
    }
    
    /* Atomically get and clear process handle - only one thread can get non-NULL */
    HANDLE hProc = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
    HANDLE hThread = plugin->pi.hThread;
    plugin->pi.hThread = NULL;
    memset(&plugin->pi, 0, sizeof(plugin->pi));
    
    if (hProc) {
        TerminateProcess(hProc, 0);
        WaitForSingleObject(hProc, 2000);
        CloseHandle(hProc);
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
        return FALSE;
    }
    
    DWORD exitCode;
    /* If GetExitCodeProcess fails, handle may have been closed by another thread */
    if (!GetExitCodeProcess(hProc, &exitCode)) {
        return FALSE;
    }
    
    if (exitCode != STILL_ACTIVE) {
        /* Process exited - let the launcher thread handle cleanup */
        LOG_INFO("[Process] IsAlive check: process has exited");
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
