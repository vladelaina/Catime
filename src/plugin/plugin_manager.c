/**
 * @file plugin_manager.c
 * @brief Plugin manager implementation
 */

#include "plugin/plugin_manager.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

static PluginInfo g_plugins[MAX_PLUGINS];
static int g_pluginCount = 0;
static CRITICAL_SECTION g_pluginCS;

// Structure to pass data to the launcher thread
typedef struct {
    PluginInfo* plugin;
    HANDLE hReadyEvent;
    BOOL success;
} PluginLauncherArgs;

/**
 * @brief Thread function to launch and debug the plugin process
 * 
 * Using the DEBUG_ONLY_THIS_PROCESS flag attaches Catime as a debugger.
 * This guarantees that if Catime crashes or exits, the OS will automatically
 * terminate the debuggee (plugin).
 */
static DWORD WINAPI PluginDebugLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = args->plugin;
    
    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Create process as a debug target
    if (!CreateProcessA(
        plugin->path,
        NULL,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | DEBUG_ONLY_THIS_PROCESS,
        NULL,
        NULL,
        &si,
        &plugin->pi
    )) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to start plugin %s in debug mode, error: %lu", plugin->displayName, error);
        args->success = FALSE;
        SetEvent(args->hReadyEvent);
        return 0;
    }

    // Process created successfully
    plugin->isRunning = TRUE;
    args->success = TRUE;
    SetEvent(args->hReadyEvent); // Signal main thread that PI is ready

    // Debug loop - required to keep the process alive and catch exit events
    DEBUG_EVENT de;
    BOOL keepRunning = TRUE;

    while (keepRunning && WaitForDebugEvent(&de, INFINITE)) {
        DWORD continueStatus = DBG_CONTINUE;

        switch (de.dwDebugEventCode) {
            case EXIT_PROCESS_DEBUG_EVENT:
                keepRunning = FALSE;
                break;
            
            case EXCEPTION_DEBUG_EVENT:
                // Pass exceptions to the child process to handle (or crash)
                if (de.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT) {
                    continueStatus = DBG_EXCEPTION_NOT_HANDLED;
                }
                break;
                
            case RIP_EVENT:
                // System debugging error
                keepRunning = FALSE;
                break;
        }

        ContinueDebugEvent(de.dwProcessId, de.dwThreadId, continueStatus);
    }
    
    return 0;
}

/**
 * @brief Extract display name from plugin filename
 * @param filename Plugin filename (e.g., "catime_monitor.exe")
 * @param displayName Output buffer for display name
 * @param bufferSize Buffer size
 */
static void ExtractDisplayName(const char* filename, char* displayName, size_t bufferSize) {
    const char* prefix = "catime_";
    size_t prefixLen = strlen(prefix);

    if (strncmp(filename, prefix, prefixLen) == 0) {
        const char* name = filename + prefixLen;
        const char* ext = strrchr(name, '.');
        size_t nameLen = ext ? (size_t)(ext - name) : strlen(name);

        if (nameLen > 0 && nameLen < bufferSize) {
            strncpy(displayName, name, nameLen);
            displayName[nameLen] = '\0';

            // Capitalize first letter
            if (displayName[0] >= 'a' && displayName[0] <= 'z') {
                displayName[0] = displayName[0] - 'a' + 'A';
            }
        } else {
            strncpy(displayName, "Unknown", bufferSize - 1);
            displayName[bufferSize - 1] = '\0';
        }
    } else {
        strncpy(displayName, filename, bufferSize - 1);
        displayName[bufferSize - 1] = '\0';
    }
}

void PluginManager_Init(void) {
    InitializeCriticalSection(&g_pluginCS);
    memset(g_plugins, 0, sizeof(g_plugins));
    g_pluginCount = 0;
    LOG_INFO("Plugin manager initialized");
}

void PluginManager_Shutdown(void) {
    EnterCriticalSection(&g_pluginCS);

    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            PluginManager_StopPlugin(i);
        }
    }

    LeaveCriticalSection(&g_pluginCS);
    DeleteCriticalSection(&g_pluginCS);
    LOG_INFO("Plugin manager shutdown");
}

BOOL PluginManager_GetPluginDir(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }

    DWORD result = ExpandEnvironmentStringsA(PLUGIN_DIR_PATH, buffer, (DWORD)bufferSize);
    if (result == 0 || result > bufferSize) {
        LOG_ERROR("Failed to expand plugin directory path");
        return FALSE;
    }

    return TRUE;
}

int PluginManager_ScanPlugins(void) {
    char pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDir(pluginDir, sizeof(pluginDir))) {
        return 0;
    }

    LOG_INFO("Scanning plugin directory: %s", pluginDir);

    EnterCriticalSection(&g_pluginCS);

    // Reset plugin list
    g_pluginCount = 0;
    memset(g_plugins, 0, sizeof(g_plugins));

    // Build search pattern
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\catime_*.exe", pluginDir);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            LOG_WARNING("Failed to scan plugin directory, error: %lu", error);
        } else {
            LOG_INFO("No plugins found in directory");
        }
        LeaveCriticalSection(&g_pluginCS);
        return 0;
    }

    do {
        if (g_pluginCount >= MAX_PLUGINS) {
            LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
            break;
        }

        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            PluginInfo* plugin = &g_plugins[g_pluginCount];

            // Store plugin name
            strncpy(plugin->name, findData.cFileName, sizeof(plugin->name) - 1);
            plugin->name[sizeof(plugin->name) - 1] = '\0';

            // Extract display name
            ExtractDisplayName(findData.cFileName, plugin->displayName, sizeof(plugin->displayName));

            // Build full path
            snprintf(plugin->path, sizeof(plugin->path), "%s\\%s", pluginDir, findData.cFileName);

            plugin->isRunning = FALSE;
            memset(&plugin->pi, 0, sizeof(plugin->pi));

            LOG_INFO("Found plugin: %s (%s)", plugin->displayName, plugin->name);
            g_pluginCount++;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    LeaveCriticalSection(&g_pluginCS);

    LOG_INFO("Plugin scan complete, found %d plugins", g_pluginCount);
    return g_pluginCount;
}

int PluginManager_GetPluginCount(void) {
    int count;
    EnterCriticalSection(&g_pluginCS);
    count = g_pluginCount;
    LeaveCriticalSection(&g_pluginCS);
    return count;
}

const PluginInfo* PluginManager_GetPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return NULL;
    }
    return &g_plugins[index];
}

BOOL PluginManager_StartPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    if (plugin->isRunning) {
        LOG_WARNING("Plugin %s is already running", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LOG_INFO("Attempting to start plugin: %s", plugin->displayName);

    // Prepare arguments for the launcher thread
    PluginLauncherArgs args = {0};
    args.plugin = plugin;
    args.hReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    args.success = FALSE;

    if (!args.hReadyEvent) {
        LOG_ERROR("Failed to create synchronization event");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    // Spawn the debugger thread
    // We don't keep the thread handle because it runs its own loop until plugin exit
    HANDLE hThread = CreateThread(
        NULL, 
        0, 
        PluginDebugLauncherThread, 
        &args, 
        0, 
        NULL
    );

    if (!hThread) {
        LOG_ERROR("Failed to create plugin launcher thread");
        CloseHandle(args.hReadyEvent);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    // Wait for the process to be created (or fail)
    WaitForSingleObject(args.hReadyEvent, INFINITE);
    CloseHandle(args.hReadyEvent);
    CloseHandle(hThread); // We don't need to control the thread, let it run

    if (!args.success) {
        LOG_ERROR("Plugin launcher thread failed to start process");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LOG_INFO("Started plugin: %s (PID: %lu)", plugin->displayName, plugin->pi.dwProcessId);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

BOOL PluginManager_StopPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    if (!plugin->isRunning) {
        LOG_WARNING("Plugin %s is not running", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    // Try graceful termination first
    if (!TerminateProcess(plugin->pi.hProcess, 0)) {
        DWORD error = GetLastError();
        LOG_WARNING("Failed to terminate plugin %s, error: %lu", plugin->displayName, error);
    }

    // Wait for process to exit (with timeout)
    WaitForSingleObject(plugin->pi.hProcess, 2000);

    CloseHandle(plugin->pi.hProcess);
    CloseHandle(plugin->pi.hThread);

    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));

    LOG_INFO("Stopped plugin: %s", plugin->displayName);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

BOOL PluginManager_TogglePlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    if (g_plugins[index].isRunning) {
        return PluginManager_StopPlugin(index);
    } else {
        return PluginManager_StartPlugin(index);
    }
}

BOOL PluginManager_IsPluginRunning(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    BOOL isRunning;
    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    if (plugin->isRunning) {
        // Check if process is still alive
        DWORD exitCode;
        if (GetExitCodeProcess(plugin->pi.hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                plugin->isRunning = FALSE;
                CloseHandle(plugin->pi.hProcess);
                CloseHandle(plugin->pi.hThread);
                memset(&plugin->pi, 0, sizeof(plugin->pi));
                LOG_INFO("Plugin %s has exited", plugin->displayName);
            }
        }
    }

    isRunning = plugin->isRunning;
    LeaveCriticalSection(&g_pluginCS);

    return isRunning;
}

int PluginManager_StartAllPlugins(void) {
    int startedCount = 0;

    LOG_INFO("Starting all plugins...");

    for (int i = 0; i < g_pluginCount; i++) {
        if (!g_plugins[i].isRunning) {
            if (PluginManager_StartPlugin(i)) {
                startedCount++;
            }
        }
    }

    LOG_INFO("Started %d plugins", startedCount);
    return startedCount;
}

BOOL PluginManager_OpenSettings(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    // Build command line with --config argument
    char cmdLine[MAX_PATH + 20];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\" --config", plugin->path);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};

    // Create process without console window
    if (!CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to open settings for plugin %s, error: %lu", plugin->displayName, error);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LOG_INFO("Opened settings for plugin: %s", plugin->displayName);

    // Close handles immediately as we don't need to track this process
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}
