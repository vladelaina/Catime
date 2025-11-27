/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring (replacing IPC)
 */

#include "plugin/plugin_data.h"
#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Internal data storage - dynamically allocated
static wchar_t* g_pluginDisplayText = NULL;
static size_t g_pluginDisplayTextLen = 0;
static BOOL g_hasPluginData = FALSE;
static BOOL g_pluginModeActive = FALSE;  // Only TRUE when user explicitly starts a plugin
static volatile BOOL g_forceNextUpdate = FALSE;  // Force file watcher to re-read
static CRITICAL_SECTION g_dataCS;

// Watcher thread
static HANDLE g_hWatchThread = NULL;
static HWND g_hNotifyWnd = NULL;
static volatile BOOL g_isRunning = FALSE;

// Cache for change detection
static char* g_lastContent = NULL;
static size_t g_lastContentSize = 0;

/**
 * @brief Parse plain text content and update display text
 * 
 * The file content is displayed as-is, supporting:
 * - Plain text
 * - Multi-line text (real newlines, no \n escaping needed)
 * - Markdown formatting
 */
static BOOL ParseContent(const char* content, size_t contentLen) {
    if (!content || contentLen == 0) return FALSE;

    EnterCriticalSection(&g_dataCS);

    // Calculate required wide char buffer size
    int requiredLen = MultiByteToWideChar(CP_UTF8, 0, content, (int)contentLen, NULL, 0);
    if (requiredLen <= 0) {
        LeaveCriticalSection(&g_dataCS);
        return FALSE;
    }

    // Reallocate buffer if needed
    size_t requiredSize = (size_t)(requiredLen + 1);
    if (g_pluginDisplayText == NULL || g_pluginDisplayTextLen < requiredSize) {
        wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, requiredSize * sizeof(wchar_t));
        if (!newBuf) {
            LOG_ERROR("PluginData: Failed to allocate %zu bytes", requiredSize * sizeof(wchar_t));
            LeaveCriticalSection(&g_dataCS);
            return FALSE;
        }
        g_pluginDisplayText = newBuf;
        g_pluginDisplayTextLen = requiredSize;
    }

    // Convert UTF-8 content to wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, content, (int)contentLen, g_pluginDisplayText, (int)g_pluginDisplayTextLen);
    if (len > 0) {
        g_pluginDisplayText[len] = L'\0';
        
        // Check for BOM (0xFEFF) at the beginning and remove it
        if (g_pluginDisplayText[0] == 0xFEFF) {
            memmove(g_pluginDisplayText, &g_pluginDisplayText[1], len * sizeof(wchar_t));
            len--;
        }
        
        // Trim trailing whitespace/newlines for cleaner display
        while (len > 0 && (g_pluginDisplayText[len - 1] == L'\n' || 
                           g_pluginDisplayText[len - 1] == L'\r' ||
                           g_pluginDisplayText[len - 1] == L' ')) {
            g_pluginDisplayText[--len] = L'\0';
        }
        
        g_hasPluginData = TRUE;
    }

    LeaveCriticalSection(&g_dataCS);

    return len > 0;
}

/* Plugin output file name */
#define PLUGIN_OUTPUT_FILENAME "output.txt"

/**
 * @brief Get plugin output file path
 * @return TRUE if successful, FALSE otherwise
 */
static BOOL GetPluginOutputPath(char* buffer, size_t bufferSize) {
    DWORD result = ExpandEnvironmentStringsA(
        "%LOCALAPPDATA%\\Catime\\resources\\plugins\\" PLUGIN_OUTPUT_FILENAME,
        buffer, (DWORD)bufferSize);
    return (result > 0 && result < bufferSize);
}

/**
 * @brief Ensure plugin output file exists (create empty if missing)
 */
static void EnsureOutputFileExists(const char* filePath) {
    /* Check if file exists */
    DWORD attrs = GetFileAttributesA(filePath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        /* File doesn't exist - create empty file */
        HANDLE hFile = CreateFileA(filePath, GENERIC_WRITE, 0, NULL,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            LOG_INFO("PluginData: Created output file %s", filePath);
        } else {
            LOG_WARNING("PluginData: Failed to create output file, error: %lu", GetLastError());
        }
    }
}

/**
 * @brief Background thread to monitor plugin data file
 */
static DWORD WINAPI FileWatcherThread(LPVOID lpParam) {
    (void)lpParam;
    
    char filePath[MAX_PATH];
    if (!GetPluginOutputPath(filePath, sizeof(filePath))) {
        LOG_WARNING("PluginData: Failed to get output file path");
        return 0;
    }
    
    LOG_INFO("PluginData: Watching file %s", filePath);

    while (g_isRunning) {
        // Check if we need to force an update (reset cache)
        if (g_forceNextUpdate) {
            g_forceNextUpdate = FALSE;
            if (g_lastContent) {
                g_lastContent[0] = '\0';
            }
        }
        
        // Use Win32 API for lower overhead (no CRT buffer)
        HANDLE hFile = CreateFileA(
            filePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            
            if (fileSize > 0 && fileSize != INVALID_FILE_SIZE) {
                // Allocate buffer for current content
                char* currentContent = (char*)malloc(fileSize + 1);
                if (currentContent) {
                    DWORD bytesRead = 0;
                    
                    if (ReadFile(hFile, currentContent, fileSize, &bytesRead, NULL) && bytesRead > 0) {
                        currentContent[bytesRead] = '\0';

                        // Check if content changed
                        BOOL contentChanged = FALSE;
                        if (g_lastContent == NULL || strcmp(currentContent, g_lastContent) != 0) {
                            contentChanged = TRUE;
                        }
                        
                        if (contentChanged) {
                            // Update last content cache
                            size_t newSize = bytesRead + 1;
                            if (g_lastContent == NULL || g_lastContentSize < newSize) {
                                char* newBuf = (char*)realloc(g_lastContent, newSize);
                                if (newBuf) {
                                    g_lastContent = newBuf;
                                    g_lastContentSize = newSize;
                                }
                            }
                            if (g_lastContent) {
                                memcpy(g_lastContent, currentContent, bytesRead + 1);
                            }
                            
                            if (ParseContent(currentContent, bytesRead)) {
                                // Notify main window to repaint immediately
                                if (g_hNotifyWnd) {
                                    InvalidateRect(g_hNotifyWnd, NULL, FALSE);
                                }
                            }
                        }
                    }
                    free(currentContent);
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
    g_pluginDisplayText = NULL;
    g_pluginDisplayTextLen = 0;
    g_lastContent = NULL;
    g_lastContentSize = 0;
    g_isRunning = TRUE;

    /* Ensure output file exists on startup */
    char outputPath[MAX_PATH];
    if (GetPluginOutputPath(outputPath, sizeof(outputPath))) {
        EnsureOutputFileExists(outputPath);
    }

    g_hWatchThread = CreateThread(NULL, 0, FileWatcherThread, NULL, 0, NULL);
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
    
    // Free dynamic memory
    EnterCriticalSection(&g_dataCS);
    if (g_pluginDisplayText) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
        g_pluginDisplayTextLen = 0;
    }
    if (g_lastContent) {
        free(g_lastContent);
        g_lastContent = NULL;
        g_lastContentSize = 0;
    }
    LeaveCriticalSection(&g_dataCS);
    
    DeleteCriticalSection(&g_dataCS);
    LOG_INFO("Plugin Data subsystem shutdown");
}

BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;

    BOOL hasData = FALSE;
    EnterCriticalSection(&g_dataCS);
    
    if (g_pluginModeActive) {
        if (g_hasPluginData && g_pluginDisplayText && wcslen(g_pluginDisplayText) > 0) {
            /* Has actual data */
            wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        } else {
            /* Plugin mode active but no data yet - show loading */
            wcsncpy(buffer, L"Loading...", maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        }
    }
    
    LeaveCriticalSection(&g_dataCS);
    return hasData;
}

void PluginData_Clear(void) {
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = FALSE;  // Deactivate plugin mode
    g_hasPluginData = FALSE;
    if (g_pluginDisplayText) {
        g_pluginDisplayText[0] = L'\0';
    }
    LeaveCriticalSection(&g_dataCS);
}

void PluginData_SetText(const wchar_t* text) {
    if (!text) return;
    
    EnterCriticalSection(&g_dataCS);
    
    size_t textLen = wcslen(text) + 1;
    if (g_pluginDisplayText == NULL || g_pluginDisplayTextLen < textLen) {
        wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, textLen * sizeof(wchar_t));
        if (newBuf) {
            g_pluginDisplayText = newBuf;
            g_pluginDisplayTextLen = textLen;
        }
    }
    
    if (g_pluginDisplayText) {
        wcscpy(g_pluginDisplayText, text);
        g_hasPluginData = TRUE;
        g_pluginModeActive = TRUE;
    }
    
    LeaveCriticalSection(&g_dataCS);
    
    // Clear the plugin data file to prevent showing stale content from previous plugin
    char filePath[MAX_PATH];
    if (GetPluginOutputPath(filePath, sizeof(filePath))) {
        // Truncate file to zero length
        HANDLE hFile = CreateFileA(filePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }
    
    // Force file watcher to re-read on next cycle
    g_forceNextUpdate = TRUE;
}

void PluginData_SetActive(BOOL active) {
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = active;
    if (active) {
        // When activating, force file watcher to re-read immediately
        g_forceNextUpdate = TRUE;
        // Clear last content cache to force re-read
        if (g_lastContent) {
            g_lastContent[0] = '\0';
        }
    } else {
        // When deactivating, also clear any stale data
        g_hasPluginData = FALSE;
        if (g_pluginDisplayText) {
            g_pluginDisplayText[0] = L'\0';
        }
    }
    LeaveCriticalSection(&g_dataCS);
    LOG_INFO("PluginData: Mode %s", active ? "ACTIVE" : "INACTIVE");
    
    // If activating, immediately read the file content (don't wait for watcher)
    if (active) {
        char filePath[MAX_PATH];
        if (GetPluginOutputPath(filePath, sizeof(filePath))) {
            HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD fileSize = GetFileSize(hFile, NULL);
                if (fileSize > 0 && fileSize < 10 * 1024 * 1024) {  // Max 10MB
                    char* content = (char*)malloc(fileSize + 1);
                    if (content) {
                        DWORD bytesRead = 0;
                        if (ReadFile(hFile, content, fileSize, &bytesRead, NULL) && bytesRead > 0) {
                            content[bytesRead] = '\0';
                            ParseContent(content, bytesRead);
                        }
                        free(content);
                    }
                }
                CloseHandle(hFile);
            }
        }
    }
}

BOOL PluginData_IsActive(void) {
    BOOL active;
    EnterCriticalSection(&g_dataCS);
    active = g_pluginModeActive;
    LeaveCriticalSection(&g_dataCS);
    return active;
}

BOOL PluginData_HasCatimeTag(void) {
    BOOL hasTag = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (g_pluginModeActive && g_hasPluginData && g_pluginDisplayText) {
        // Check for <catime> and </catime> tags
        wchar_t* start = wcsstr(g_pluginDisplayText, L"<catime>");
        wchar_t* end = wcsstr(g_pluginDisplayText, L"</catime>");
        if (start && end && end > start) {
            hasTag = TRUE;
        }
    }
    LeaveCriticalSection(&g_dataCS);
    return hasTag;
}
