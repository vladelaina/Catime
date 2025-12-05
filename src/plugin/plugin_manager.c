/**
 * @file plugin_manager.c
 * @brief Plugin manager - core plugin lifecycle and state management
 */

#include "plugin/plugin_manager.h"
#include "plugin/plugin_process.h"
#include "plugin/plugin_extensions.h"
#include "plugin/plugin_data.h"
#include "config/config_plugin_security.h"
#include "dialog/dialog_plugin_security.h"
#include "utils/natural_sort.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
static BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime);
static void StopPluginInternal(int index);

/**
 * @brief Comparator for plugin sorting (by display name, natural order)
 */
static int ComparePluginInfo(const void* a, const void* b) {
    const PluginInfo* pa = (const PluginInfo*)a;
    const PluginInfo* pb = (const PluginInfo*)b;
    return NaturalCompareW(pa->displayName, pb->displayName);
}

/**
 * @brief Get file modification time
 */
static BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
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
                    LOG_INFO("[HotReload] File changed: %ls", g_plugins[indexToMonitor].displayName);
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
static void ExtractDisplayName(const wchar_t* filename, wchar_t* displayName, size_t bufferSize) {
    wcsncpy(displayName, filename, bufferSize - 1);
    displayName[bufferSize - 1] = L'\0';
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
        DWORD waitResult = WaitForSingleObject(g_hHotReloadThread, 5000);  /* Increased timeout to 5 seconds */
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARNING("Hot-reload thread did not stop within timeout, forcing termination");
            /* Note: TerminateThread is dangerous but we're shutting down anyway */
        }
        CloseHandle(g_hHotReloadThread);
        g_hHotReloadThread = NULL;
    }

    EnterCriticalSection(&g_pluginCS);

    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            StopPluginInternal(i);  /* Use internal version - already have lock */
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
    char pluginDirA[MAX_PATH];
    if (!PluginManager_GetPluginDir(pluginDirA, sizeof(pluginDirA))) {
        return 0;
    }

    /* Convert to wide string for Unicode support */
    wchar_t pluginDir[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, pluginDirA, -1, pluginDir, MAX_PATH);

    LOG_INFO("Scanning plugin directory: %s", pluginDirA);

    EnterCriticalSection(&g_pluginCS);

    // Scan into temporary list to preserve state
    PluginInfo newPlugins[MAX_PLUGINS];
    int newPluginCount = 0;
    memset(newPlugins, 0, sizeof(newPlugins));

    /* Scan for all supported script extensions */
    for (size_t ext = 0; ext < PLUGIN_EXTENSION_COUNT; ext++) {
        wchar_t searchPath[MAX_PATH];
        wchar_t extPattern[32];
        MultiByteToWideChar(CP_UTF8, 0, PLUGIN_EXTENSIONS[ext], -1, extPattern, 32);
        _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\%s", pluginDir, extPattern);

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (newPluginCount >= MAX_PLUGINS) {
                    LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
                    break;
                }

                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    PluginInfo* plugin = &newPlugins[newPluginCount];

                    wcsncpy(plugin->name, findData.cFileName, 63);
                    plugin->name[63] = L'\0';
                    ExtractDisplayName(findData.cFileName, plugin->displayName, 64);
                    _snwprintf_s(plugin->path, MAX_PATH, _TRUNCATE, L"%s\\%s", pluginDir, findData.cFileName);
                    
                    plugin->isRunning = FALSE;
                    memset(&plugin->pi, 0, sizeof(plugin->pi));

                    // Preserve state from existing list
                    for (int i = 0; i < g_pluginCount; i++) {
                        if (wcscmp(g_plugins[i].name, plugin->name) == 0) {
                            plugin->isRunning = g_plugins[i].isRunning;
                            plugin->pi = g_plugins[i].pi;
                            plugin->lastModTime = g_plugins[i].lastModTime;
                            break;
                        }
                    }

                    LOG_INFO("Found plugin: %ls (%ls)", plugin->displayName, plugin->name);
                    newPluginCount++;
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }

    /* Clean up orphaned plugins (running but file removed) */
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            BOOL found = FALSE;
            for (int j = 0; j < newPluginCount; j++) {
                if (wcscmp(g_plugins[i].name, newPlugins[j].name) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                LOG_INFO("Plugin file %ls removed, stopping process", g_plugins[i].name);
                PluginProcess_Terminate(&g_plugins[i]);
            }
        }
    }

    // Sort plugins by display name (natural order) for consistent menu ordering
    if (newPluginCount > 1) {
        qsort(newPlugins, newPluginCount, sizeof(PluginInfo), ComparePluginInfo);
    }

    // Remember old plugin names for re-mapping indices
    wchar_t lastRunningName[64] = {0};
    wchar_t activePluginName[64] = {0};
    if (g_lastRunningPluginIndex >= 0 && g_lastRunningPluginIndex < g_pluginCount) {
        wcsncpy(lastRunningName, g_plugins[g_lastRunningPluginIndex].name, 63);
        lastRunningName[63] = L'\0';
    }
    if (g_activePluginIndex >= 0 && g_activePluginIndex < g_pluginCount) {
        wcsncpy(activePluginName, g_plugins[g_activePluginIndex].name, 63);
        activePluginName[63] = L'\0';
    }

    // Update global list
    memcpy(g_plugins, newPlugins, sizeof(g_plugins));
    g_pluginCount = newPluginCount;
    
    // Re-map g_lastRunningPluginIndex to new list
    if (lastRunningName[0]) {
        g_lastRunningPluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].name, lastRunningName) == 0) {
                g_lastRunningPluginIndex = i;
                break;
            }
        }
    }
    
    // Re-map g_activePluginIndex to new list
    if (activePluginName[0]) {
        g_activePluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].name, activePluginName) == 0) {
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

/**
 * @brief Internal: terminate plugin process only (no PluginData_Clear)
 * Used when switching plugins to avoid clearing the "Loading..." message
 * NOTE: Caller must hold g_pluginCS lock
 */
static void TerminatePluginInternal(int index) {
    if (index < 0 || index >= g_pluginCount) return;
    
    PluginInfo* plugin = &g_plugins[index];
    if (!plugin->isRunning) return;
    
    PluginProcess_Terminate(plugin);
    LOG_INFO("Terminated plugin (internal): %ls", plugin->displayName);
}

/**
 * @brief Internal: stop plugin without acquiring lock
 * NOTE: Caller must hold g_pluginCS lock
 */
static void StopPluginInternal(int index) {
    if (index < 0 || index >= g_pluginCount) return;
    
    PluginInfo* plugin = &g_plugins[index];
    if (!plugin->isRunning) return;
    
    PluginProcess_Terminate(plugin);
    PluginData_Clear();
    LOG_INFO("Stopped plugin (internal): %ls", plugin->displayName);
}

BOOL PluginManager_StartPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    /* Exclusive execution: Stop ALL other plugins first */
    /* Use internal terminate to avoid clearing "Loading..." message set by caller */
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            if (i == index) {
                LOG_INFO("Plugin %ls is already running", g_plugins[index].displayName);
                LeaveCriticalSection(&g_pluginCS);
                return TRUE;
            }
            TerminatePluginInternal(i);
        }
    }
    
    /* Reset indices since we terminated other plugins */
    g_lastRunningPluginIndex = -1;

    /* Copy plugin info for security check (in case array is modified during dialog) */
    wchar_t pluginPath[MAX_PATH];
    wchar_t pluginDisplayName[64];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';
    
    /* Convert to UTF-8 for security check functions */
    char pluginPathUtf8[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, pluginPath, -1, pluginPathUtf8, MAX_PATH, NULL, NULL);
    
    /* Security check: verify plugin trust before launching */
    if (!IsPluginTrusted(pluginPathUtf8)) {
        LOG_INFO("Plugin not trusted, showing security dialog: %ls", pluginDisplayName);
        LeaveCriticalSection(&g_pluginCS);
        
        /* Show security confirmation dialog (critical section released during dialog) */
        HWND hwnd = PluginProcess_GetNotifyWindow();
        
        char displayNameUtf8[128];
        WideCharToMultiByte(CP_UTF8, 0, pluginDisplayName, -1, displayNameUtf8, 128, NULL, NULL);
        INT_PTR result = ShowPluginSecurityDialog(hwnd, pluginPathUtf8, displayNameUtf8);
        
        if (result == IDCANCEL) {
            /* User cancelled, don't run plugin */
            LOG_INFO("User cancelled plugin execution: %ls", pluginDisplayName);
            /* Note: Critical section was already released at line 370, no need to re-acquire */
            return FALSE;
        } else if (result == IDYES) {
            /* User chose "Trust & Run" - add to trust list */
            if (TrustPlugin(pluginPathUtf8)) {
                LOG_INFO("Plugin added to trust list: %ls", pluginDisplayName);
            } else {
                LOG_ERROR("Failed to add plugin to trust list: %ls (will still run once)", pluginDisplayName);
                /* Note: We continue to run the plugin even if trust list update failed,
                 * because user explicitly chose to run it. Next time will ask again. */
            }
        }
        /* For IDOK (Run Once), we just continue without adding to trust list */
        
        EnterCriticalSection(&g_pluginCS);
        
        /* Revalidate index after dialog (array might have changed) */
        if (index < 0 || index >= g_pluginCount) {
            LOG_ERROR("Plugin index invalid after security dialog");
            LeaveCriticalSection(&g_pluginCS);
            return FALSE;
        }
    }
    
    PluginInfo* plugin = &g_plugins[index];
    LOG_INFO("Starting plugin: %ls", plugin->displayName);

    /* Launch via process module */
    if (!PluginProcess_Launch(plugin)) {
        LOG_ERROR("Failed to launch plugin: %ls", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Record file modification time for hot-reload */
    GetFileModTime(plugin->path, &plugin->lastModTime);
    g_activePluginIndex = index;

    LOG_INFO("Plugin started: %ls (PID: %lu)", plugin->displayName, plugin->pi.dwProcessId);

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
    _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", plugin->displayName);
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
        LOG_WARNING("Plugin %ls is not running", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Terminate via process module */
    PluginProcess_Terminate(plugin);
    PluginData_Clear();
    
    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;

    LOG_INFO("Stopped plugin: %ls", plugin->displayName);

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
        LOG_INFO("Plugin %ls has exited", plugin->displayName);
    }

    isRunning = plugin->isRunning;
    LeaveCriticalSection(&g_pluginCS);

    return isRunning;
}

void PluginManager_StopAllPlugins(void) {
    EnterCriticalSection(&g_pluginCS);
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            StopPluginInternal(i);  /* Use internal version - already have lock */
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
