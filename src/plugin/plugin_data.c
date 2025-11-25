/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring (replacing IPC)
 */

#include "plugin/plugin_data.h"
#include "log.h"
#include "cJSON.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

// Internal data storage
static wchar_t g_pluginDisplayText[128] = {0};
static BOOL g_hasPluginData = FALSE;
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

    cJSON* textItem = cJSON_GetObjectItem(root, "text");
    if (!textItem || !cJSON_IsString(textItem)) {
        cJSON_Delete(root);
        return FALSE;
    }

    const char* text = cJSON_GetStringValue(textItem);
    if (text) {
        EnterCriticalSection(&g_dataCS);
        // Convert UTF-8 to Wide Char for Windows display
        MultiByteToWideChar(CP_UTF8, 0, text, -1, g_pluginDisplayText, 128);
        g_hasPluginData = TRUE;
        LeaveCriticalSection(&g_dataCS);
    }

    cJSON_Delete(root);
    return TRUE;
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
    if (g_hasPluginData && wcslen(g_pluginDisplayText) > 0) {
        wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasData = TRUE;
    }
    LeaveCriticalSection(&g_dataCS);
    return hasData;
}
