/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring (replacing IPC)
 */

#include "plugin/plugin_data.h"
#include "utils/http_downloader.h"
#include "log.h"
#include "cJSON.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

// Internal data storage
static wchar_t g_pluginDisplayText[4096] = {0};
static wchar_t g_pluginImagePath[MAX_PATH] = {0};
static BOOL g_hasPluginData = FALSE;
static BOOL g_pluginModeActive = FALSE;  // Only TRUE when user explicitly starts a plugin
static volatile BOOL g_forceNextUpdate = FALSE;  // Force file watcher to re-read
static CRITICAL_SECTION g_dataCS;

// Watcher thread
static HANDLE g_hWatchThread = NULL;
static HWND g_hNotifyWnd = NULL;
static volatile BOOL g_isRunning = FALSE;

/**
 * @brief Parse JSON content and update display text
 */
static BOOL ParseJSON(const char* jsonStr) {
    cJSON* root = cJSON_Parse(jsonStr);
    if (!root) return FALSE;

    BOOL updated = FALSE;

    EnterCriticalSection(&g_dataCS);

    // Parse "text"
    cJSON* textItem = cJSON_GetObjectItem(root, "text");
    if (textItem && cJSON_IsString(textItem)) {
        const char* text = cJSON_GetStringValue(textItem);
        if (text) {
            MultiByteToWideChar(CP_UTF8, 0, text, -1, g_pluginDisplayText, 4096);
            
            // Check for BOM (0xFEFF) at the beginning and remove it
            if (g_pluginDisplayText[0] == 0xFEFF) {
                // Shift string left by one
                int len = wcslen(g_pluginDisplayText);
                memmove(g_pluginDisplayText, &g_pluginDisplayText[1], len * sizeof(wchar_t));
                g_pluginDisplayText[len-1] = L'\0'; // Ensure null termination
            }
            
            updated = TRUE;
        }
    } else {
        // Field missing: Clear text
        if (wcslen(g_pluginDisplayText) > 0) {
            memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
            updated = TRUE;
        }
    }

    // Parse "image"
    cJSON* imageItem = cJSON_GetObjectItem(root, "image");
    if (imageItem && cJSON_IsString(imageItem)) {
        const char* imgPath = cJSON_GetStringValue(imageItem);
        if (imgPath) {
            wchar_t wPath[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, imgPath, -1, wPath, MAX_PATH);
            
            if (IsHttpUrl(wPath)) {
                // Handle HTTP URL
                wchar_t cachePath[MAX_PATH];
                GetLocalCachePath(wPath, cachePath, MAX_PATH);
                
                // Check if file exists
                if (GetFileAttributesW(cachePath) == INVALID_FILE_ATTRIBUTES) {
                    // File doesn't exist, trigger download
                    DownloadFileAsync(wPath, cachePath, g_hNotifyWnd);
                }
                
                // Point to cache path (GDI+ will fail until download completes, which is fine)
                wcscpy(g_pluginImagePath, cachePath);
            } else {
                // Local file
                wcscpy(g_pluginImagePath, wPath);
            }
            updated = TRUE;
        }
    } else {
        // Field missing: Clear image path
        if (wcslen(g_pluginImagePath) > 0) {
            memset(g_pluginImagePath, 0, sizeof(g_pluginImagePath));
            updated = TRUE;
        }
    }
    
    if (updated) {
        g_hasPluginData = TRUE;
    }

    LeaveCriticalSection(&g_dataCS);

    cJSON_Delete(root);
    return updated;
}

/**
 * @brief Background thread to monitor plugin data file
 */
static DWORD WINAPI FileWatcherThread(LPVOID lpParam) {
    char desktopPath[MAX_PATH];
    if (!SHGetSpecialFolderPathA(NULL, desktopPath, CSIDL_DESKTOP, FALSE)) {
        LOG_WARNING("PluginData: Failed to get desktop path");
        return 0;
    }

    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\catime_plugin_debug.txt", desktopPath);
    LOG_INFO("PluginData: Watching file %s", filePath);

    char lastContent[4096] = {0};
    char currentContent[4096] = {0};

    while (g_isRunning) {
        // Check if we need to force an update (reset cache)
        if (g_forceNextUpdate) {
            g_forceNextUpdate = FALSE;
            lastContent[0] = '\0';  // Clear cache to force re-read
        }
        
        // Use Win32 API for lower overhead (no CRT buffer)
        HANDLE hFile = CreateFileA(
            filePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, // Allow others to write while we read
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            
            if (fileSize > 0 && fileSize < sizeof(currentContent) - 1) {
                DWORD bytesRead = 0;
                
                if (ReadFile(hFile, currentContent, fileSize, &bytesRead, NULL) && bytesRead > 0) {
                    currentContent[bytesRead] = '\0';

                    // Check if content changed
                    if (strcmp(currentContent, lastContent) != 0) {
                        strcpy(lastContent, currentContent);
                        
                        if (ParseJSON(currentContent)) {
                            // Notify main window to repaint immediately
                            if (g_hNotifyWnd) {
                                InvalidateRect(g_hNotifyWnd, NULL, FALSE);
                            }
                        }
                    }
                }
            }
            CloseHandle(hFile);
        }
        
        // Poll frequency: 500ms
        Sleep(500);
    }
    return 0;
}

void PluginData_Init(HWND hwnd) {
    InitializeCriticalSection(&g_dataCS);
    g_hNotifyWnd = hwnd;
    g_hasPluginData = FALSE;
    memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
    memset(g_pluginImagePath, 0, sizeof(g_pluginImagePath));
    g_isRunning = TRUE;

    // Use 64KB stack size to reduce memory footprint (default is 1MB)
    // STACK_SIZE_PARAM_IS_A_RESERVATION = 0x00010000
    g_hWatchThread = CreateThread(NULL, 64 * 1024, FileWatcherThread, NULL, 0x00010000, NULL);
    if (g_hWatchThread) {
        LOG_INFO("Plugin Data subsystem initialized (File Watcher Mode)");
    } else {
        LOG_ERROR("Failed to start Plugin Data watcher thread");
    }
}

void PluginData_Shutdown(void) {
    if (g_hWatchThread) {
        g_isRunning = FALSE;
        WaitForSingleObject(g_hWatchThread, 1000);
        CloseHandle(g_hWatchThread);
        g_hWatchThread = NULL;
    }
    
    DeleteCriticalSection(&g_dataCS);
    LOG_INFO("Plugin Data subsystem shutdown");
}

BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;

    BOOL hasData = FALSE;
    EnterCriticalSection(&g_dataCS);
    // Only return data if plugin mode is active (user started a plugin)
    if (g_pluginModeActive && g_hasPluginData && wcslen(g_pluginDisplayText) > 0) {
        wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasData = TRUE;
    }
    LeaveCriticalSection(&g_dataCS);
    return hasData;
}

BOOL PluginData_GetImagePath(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;

    BOOL hasPath = FALSE;
    EnterCriticalSection(&g_dataCS);
    // Only return data if plugin mode is active (user started a plugin)
    if (g_pluginModeActive && g_hasPluginData && wcslen(g_pluginImagePath) > 0) {
        wcsncpy(buffer, g_pluginImagePath, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasPath = TRUE;
    }
    LeaveCriticalSection(&g_dataCS);
    return hasPath;
}

void PluginData_Clear(void) {
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = FALSE;  // Deactivate plugin mode
    g_hasPluginData = FALSE;
    memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
    memset(g_pluginImagePath, 0, sizeof(g_pluginImagePath));
    LeaveCriticalSection(&g_dataCS);
}

void PluginData_SetText(const wchar_t* text) {
    if (!text) return;
    
    EnterCriticalSection(&g_dataCS);
    wcsncpy(g_pluginDisplayText, text, 4095);
    g_pluginDisplayText[4095] = L'\0';
    g_hasPluginData = TRUE;
    g_pluginModeActive = TRUE;  // Also activate plugin mode
    LeaveCriticalSection(&g_dataCS);
    
    // Clear the plugin data file to prevent showing stale content from previous plugin
    char desktopPath[MAX_PATH];
    if (SHGetSpecialFolderPathA(NULL, desktopPath, CSIDL_DESKTOP, FALSE)) {
        char filePath[MAX_PATH];
        snprintf(filePath, sizeof(filePath), "%s\\catime_plugin_debug.txt", desktopPath);
        
        // Truncate file to zero length
        HANDLE hFile = CreateFileA(filePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }
    
    // Force file watcher to re-read on next cycle
    // This ensures plugin data will be detected even if file content matches previous cache
    g_forceNextUpdate = TRUE;
}

void PluginData_SetActive(BOOL active) {
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = active;
    if (!active) {
        // When deactivating, also clear any stale data
        g_hasPluginData = FALSE;
        memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
        memset(g_pluginImagePath, 0, sizeof(g_pluginImagePath));
    }
    LeaveCriticalSection(&g_dataCS);
    LOG_INFO("PluginData: Mode %s", active ? "ACTIVE" : "INACTIVE");
}

BOOL PluginData_IsActive(void) {
    BOOL active;
    EnterCriticalSection(&g_dataCS);
    active = g_pluginModeActive;
    LeaveCriticalSection(&g_dataCS);
    return active;
}
