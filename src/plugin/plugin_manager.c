/**
 * @file plugin_manager.c
 * @brief Plugin manager - core plugin lifecycle and state management
 */

#include "plugin/plugin_manager.h"
#include "plugin/plugin_process.h"
#include "plugin/plugin_extensions.h"
#include "plugin/plugin_data.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shellapi.h>

/* Plugin state */
static PluginInfo g_plugins[MAX_PLUGINS];
static int g_pluginCount = 0;
static CRITICAL_SECTION g_pluginCS;

/* Hot-reload monitoring */
static HANDLE g_hHotReloadThread = NULL;
static volatile BOOL g_hotReloadRunning = FALSE;
static volatile int g_lastRunningPluginIndex = -1;
static volatile int g_activePluginIndex = -1;

/* Forward declarations */
static BOOL RestartPluginInternal(int index);
static BOOL GetFileModTime(const char* path, FILETIME* modTime);

/**
 * @brief Get file modification time
 */
static BOOL GetFileModTime(const char* path, FILETIME* modTime) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    BOOL result = GetFileTime(hFile, NULL, NULL, modTime);
    CloseHandle(hFile);
    return result;
}

/**
 * @brief Hot-reload monitoring thread
 */
static DWORD WINAPI HotReloadThread(LPVOID lpParam) {
    (void)lpParam;
    LOG_INFO("[HotReload] Thread started");
    
    while (g_hotReloadRunning) {
        Sleep(1000);
        if (!g_hotReloadRunning) break;
        
        EnterCriticalSection(&g_pluginCS);
        
        int indexToMonitor = -1;
        
        /* Find running plugin or last running */
        for (int i = 0; i < g_pluginCount; i++) {
            if (g_plugins[i].isRunning) {
                indexToMonitor = i;
                g_lastRunningPluginIndex = i;
                break;
            }
        }
        if (indexToMonitor < 0 && g_lastRunningPluginIndex >= 0 && 
            g_lastRunningPluginIndex < g_pluginCount) {
            indexToMonitor = g_lastRunningPluginIndex;
        }
        
        /* Check file modification */
        if (indexToMonitor >= 0) {
            FILETIME currentModTime;
            if (GetFileModTime(g_plugins[indexToMonitor].path, &currentModTime)) {
                if (CompareFileTime(&currentModTime, &g_plugins[indexToMonitor].lastModTime) != 0) {
                    LOG_INFO("[HotReload] File changed: %s", g_plugins[indexToMonitor].displayName);
                    g_plugins[indexToMonitor].lastModTime = currentModTime;
                    int idx = indexToMonitor;
                    LeaveCriticalSection(&g_pluginCS);
                    RestartPluginInternal(idx);
                    EnterCriticalSection(&g_pluginCS);
                }
            }
        }
        
        LeaveCriticalSection(&g_pluginCS);
    }
    
    LOG_INFO("[HotReload] Thread stopped");
    return 0;
}

/**
 * @brief Extract display name from plugin filename (keep extension)
 */
static void ExtractDisplayName(const char* filename, char* displayName, size_t bufferSize) {
    strncpy(displayName, filename, bufferSize - 1);
    displayName[bufferSize - 1] = '\0';
}

void PluginManager_Init(void) {
    InitializeCriticalSection(&g_pluginCS);
    memset(g_plugins, 0, sizeof(g_plugins));
    g_pluginCount = 0;

    /* Initialize process management */
    PluginProcess_Init();

    /* Start hot-reload monitoring thread */
    g_hotReloadRunning = TRUE;
    g_hHotReloadThread = CreateThread(NULL, 0, HotReloadThread, NULL, 0, NULL);
    if (!g_hHotReloadThread) {
        LOG_WARNING("Failed to start hot-reload thread");
    }

    LOG_INFO("Plugin manager initialized");
}

void PluginManager_Shutdown(void) {
    /* Stop hot-reload thread */
    if (g_hHotReloadThread) {
        g_hotReloadRunning = FALSE;
        WaitForSingleObject(g_hHotReloadThread, 2000);
        CloseHandle(g_hHotReloadThread);
        g_hHotReloadThread = NULL;
    }

    EnterCriticalSection(&g_pluginCS);

    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            PluginManager_StopPlugin(i);
        }
    }

    /* Shutdown process management */
    PluginProcess_Shutdown();
    
    g_activePluginIndex = -1;
    g_lastRunningPluginIndex = -1;
    g_pluginCount = 0;

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

    // Scan into temporary list to preserve state
    PluginInfo newPlugins[MAX_PLUGINS];
    int newPluginCount = 0;
    memset(newPlugins, 0, sizeof(newPlugins));

    /* Scan for all supported script extensions */
    for (size_t ext = 0; ext < PLUGIN_EXTENSION_COUNT; ext++) {
        char searchPath[MAX_PATH];
        snprintf(searchPath, sizeof(searchPath), "%s\\%s", pluginDir, PLUGIN_EXTENSIONS[ext]);

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath, &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (newPluginCount >= MAX_PLUGINS) {
                    LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
                    break;
                }

                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    PluginInfo* plugin = &newPlugins[newPluginCount];

                    strncpy(plugin->name, findData.cFileName, sizeof(plugin->name) - 1);
                    plugin->name[sizeof(plugin->name) - 1] = '\0';
                    ExtractDisplayName(findData.cFileName, plugin->displayName, sizeof(plugin->displayName));
                    snprintf(plugin->path, sizeof(plugin->path), "%s\\%s", pluginDir, findData.cFileName);
                    
                    plugin->isRunning = FALSE;
                    memset(&plugin->pi, 0, sizeof(plugin->pi));

                    // Preserve state from existing list
                    for (int i = 0; i < g_pluginCount; i++) {
                        if (strcmp(g_plugins[i].name, plugin->name) == 0) {
                            plugin->isRunning = g_plugins[i].isRunning;
                            plugin->pi = g_plugins[i].pi;
                            plugin->lastModTime = g_plugins[i].lastModTime;
                            break;
                        }
                    }

                    LOG_INFO("Found plugin: %s (%s)", plugin->displayName, plugin->name);
                    newPluginCount++;
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }

    /* Clean up orphaned plugins (running but file removed) */
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            BOOL found = FALSE;
            for (int j = 0; j < newPluginCount; j++) {
                if (strcmp(g_plugins[i].name, newPlugins[j].name) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                LOG_INFO("Plugin file %s removed, stopping process", g_plugins[i].name);
                PluginProcess_Terminate(&g_plugins[i]);
            }
        }
    }

    // Remember old plugin names for re-mapping indices
    char lastRunningName[64] = {0};
    char activePluginName[64] = {0};
    if (g_lastRunningPluginIndex >= 0 && g_lastRunningPluginIndex < g_pluginCount) {
        strncpy(lastRunningName, g_plugins[g_lastRunningPluginIndex].name, sizeof(lastRunningName) - 1);
    }
    if (g_activePluginIndex >= 0 && g_activePluginIndex < g_pluginCount) {
        strncpy(activePluginName, g_plugins[g_activePluginIndex].name, sizeof(activePluginName) - 1);
    }

    // Update global list
    memcpy(g_plugins, newPlugins, sizeof(g_plugins));
    g_pluginCount = newPluginCount;
    
    // Re-map g_lastRunningPluginIndex to new list
    if (lastRunningName[0]) {
        g_lastRunningPluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (strcmp(g_plugins[i].name, lastRunningName) == 0) {
                g_lastRunningPluginIndex = i;
                break;
            }
        }
    }
    
    // Re-map g_activePluginIndex to new list
    if (activePluginName[0]) {
        g_activePluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (strcmp(g_plugins[i].name, activePluginName) == 0) {
                g_activePluginIndex = i;
                break;
            }
        }
    }

    LeaveCriticalSection(&g_pluginCS);

    LOG_INFO("Plugin scan complete, found %d plugins", g_pluginCount);
    return g_pluginCount;
}

/* Async scan thread */
static volatile LONG g_asyncScanPending = 0;

static DWORD WINAPI AsyncScanThread(LPVOID lpParam) {
    (void)lpParam;
    PluginManager_ScanPlugins();
    // Reset flag after scan completes
    InterlockedExchange(&g_asyncScanPending, 0);
    return 0;
}

void PluginManager_RequestScanAsync(void) {
    /* Avoid multiple concurrent scans */
    if (InterlockedCompareExchange(&g_asyncScanPending, 1, 0) != 0) {
        return;
    }
    
    HANDLE hThread = CreateThread(NULL, 0, AsyncScanThread, NULL, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        // Failed to create thread, reset flag
        InterlockedExchange(&g_asyncScanPending, 0);
    }
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

    /* Exclusive execution: Stop ALL other plugins first */
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            if (i == index) {
                LOG_INFO("Plugin %s is already running", g_plugins[index].displayName);
                LeaveCriticalSection(&g_pluginCS);
                return TRUE;
            }
            PluginManager_StopPlugin(i);
        }
    }

    PluginInfo* plugin = &g_plugins[index];
    LOG_INFO("Starting plugin: %s", plugin->displayName);

    /* Launch via process module */
    if (!PluginProcess_Launch(plugin)) {
        LOG_ERROR("Failed to launch plugin: %s", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Record file modification time for hot-reload */
    GetFileModTime(plugin->path, &plugin->lastModTime);
    g_activePluginIndex = index;

    LOG_INFO("Plugin started: %s (PID: %lu)", plugin->displayName, plugin->pi.dwProcessId);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

/**
 * @brief Internal restart function for hot-reload
 */
static BOOL RestartPluginInternal(int index) {
    if (index < 0 || index >= g_pluginCount) return FALSE;
    
    PluginInfo* plugin = &g_plugins[index];
    
    /* Stop the plugin first */
    PluginManager_StopPlugin(index);
    
    /* Show "Loading..." message */
    wchar_t loadingText[256];
    wchar_t displayNameW[128];
    MultiByteToWideChar(CP_UTF8, 0, plugin->displayName, -1, displayNameW, 128);
    _snwprintf(loadingText, 256, L"Loading %s...", displayNameW);
    PluginData_SetText(loadingText);
    PluginData_SetActive(TRUE);
    
    /* Force redraw */
    HWND hwnd = PluginProcess_GetNotifyWindow();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    Sleep(100);
    
    return PluginManager_StartPlugin(index);
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

    /* Terminate via process module */
    PluginProcess_Terminate(plugin);
    PluginData_Clear();
    
    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;

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

    /* Check via process module */
    if (plugin->isRunning && !PluginProcess_IsAlive(plugin)) {
        LOG_INFO("Plugin %s has exited", plugin->displayName);
    }

    isRunning = plugin->isRunning;
    LeaveCriticalSection(&g_pluginCS);

    return isRunning;
}

void PluginManager_StopAllPlugins(void) {
    EnterCriticalSection(&g_pluginCS);
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            PluginManager_StopPlugin(i);
        }
    }
    // Always clear indices (even if no plugin was running)
    // This handles cases where plugin already exited (e.g., after <exit> tag)
    g_activePluginIndex = -1;
    g_lastRunningPluginIndex = -1;
    LeaveCriticalSection(&g_pluginCS);
}

BOOL PluginManager_OpenPluginFolder(void) {
    char pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDir(pluginDir, sizeof(pluginDir))) {
        return FALSE;
    }
    
    // Ensure directory exists
    CreateDirectoryA(pluginDir, NULL);
    
    // Open in explorer
    ShellExecuteA(NULL, "open", pluginDir, NULL, NULL, SW_SHOW);
    return TRUE;
}

void PluginManager_SetNotifyWindow(HWND hwnd) {
    PluginProcess_SetNotifyWindow(hwnd);
}

int PluginManager_GetActivePluginIndex(void) {
    return g_activePluginIndex;
}
