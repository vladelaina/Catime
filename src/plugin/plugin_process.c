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
 * @brief Thread function to launch and monitor the plugin process
 * Uses ShellExecuteEx to let Windows choose the appropriate interpreter
 */
static DWORD WINAPI PluginLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = args->plugin;
    
    LOG_INFO("[Process] Launcher thread started for: %s", plugin->displayName);
    
    /* Extract working directory from plugin path */
    char workDir[MAX_PATH];
    strncpy(workDir, plugin->path, sizeof(workDir) - 1);
    workDir[sizeof(workDir) - 1] = '\0';
    char* lastSlash = strrchr(workDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    
    /* Use ShellExecuteEx to launch script with system-associated interpreter */
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = "open";
    sei.lpFile = plugin->path;
    sei.lpDirectory = workDir;
    sei.nShow = SW_HIDE;
    
    if (!ShellExecuteExA(&sei)) {
        DWORD error = GetLastError();
        LOG_ERROR("[Process] ShellExecuteEx failed! Error: %lu (Path: %s)", error, plugin->path);
        
        if (error == ERROR_NO_ASSOCIATION) {
            LOG_ERROR("Tip: No program associated with this file type. Install the required interpreter.");
        } else if (error == ERROR_FILE_NOT_FOUND) {
            LOG_ERROR("Tip: Script file not found: %s", plugin->path);
        }
        
        args->success = FALSE;
        SetEvent(args->hReadyEvent);
        return 0;
    }
    
    /* Store process handle */
    plugin->pi.hProcess = sei.hProcess;
    plugin->pi.hThread = NULL;  /* ShellExecuteEx doesn't return thread handle */
    plugin->pi.dwProcessId = GetProcessId(sei.hProcess);
    plugin->pi.dwThreadId = 0;
    
    LOG_INFO("[Process] Script launched successfully. PID: %lu", plugin->pi.dwProcessId);
    
    /* Duplicate handle for safe waiting */
    HANDLE hWaitProcess = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), plugin->pi.hProcess,
                         GetCurrentProcess(), &hWaitProcess,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_WARNING("[Process] Failed to duplicate handle, using original");
        hWaitProcess = plugin->pi.hProcess;
    }
    
    /* Assign to Job Object for lifecycle management */
    if (g_hJob && sei.hProcess) {
        if (!AssignProcessToJobObject(g_hJob, sei.hProcess)) {
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
    WaitForSingleObject(hWaitProcess, INFINITE);
    
    DWORD runDuration = GetTickCount() - startTime;
    DWORD exitCode = 0;
    GetExitCodeProcess(hWaitProcess, &exitCode);
    LOG_INFO("[Process] Plugin exited (Code: %lu, Duration: %lu ms)", exitCode, runDuration);
    
    /* Cleanup duplicate handle */
    if (hWaitProcess != plugin->pi.hProcess) {
        CloseHandle(hWaitProcess);
    }
    
    /* Handle unexpected exit (plugin crashed or finished on its own) */
    if (plugin->isRunning) {
        LOG_INFO("[Process] Plugin %s exited unexpectedly", plugin->displayName);
        
        if (plugin->pi.hProcess) CloseHandle(plugin->pi.hProcess);
        plugin->isRunning = FALSE;
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
    
    /* Terminate process */
    if (plugin->pi.hProcess) {
        TerminateProcess(plugin->pi.hProcess, 0);
        WaitForSingleObject(plugin->pi.hProcess, 2000);
        CloseHandle(plugin->pi.hProcess);
    }
    if (plugin->pi.hThread) {
        CloseHandle(plugin->pi.hThread);
    }
    
    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));
    
    return TRUE;
}

BOOL PluginProcess_IsAlive(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning || !plugin->pi.hProcess) {
        return FALSE;
    }
    
    DWORD exitCode;
    if (GetExitCodeProcess(plugin->pi.hProcess, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            /* Process exited, clean up */
            CloseHandle(plugin->pi.hProcess);
            if (plugin->pi.hThread) CloseHandle(plugin->pi.hThread);
            plugin->isRunning = FALSE;
            memset(&plugin->pi, 0, sizeof(plugin->pi));
            return FALSE;
        }
    }
    return TRUE;
}

void PluginProcess_SetNotifyWindow(HWND hwnd) {
    g_hNotifyWnd = hwnd;
}

HWND PluginProcess_GetNotifyWindow(void) {
    return g_hNotifyWnd;
}
