/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring
 */

#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "notification.h"
#include "../resource/resource.h"
#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Shared State (exported for plugin_exit.c)
 * ============================================================================ */

wchar_t* g_pluginDisplayText = NULL;
size_t g_pluginDisplayTextLen = 0;
BOOL g_hasPluginData = FALSE;

/* ============================================================================
 * Internal State
 * ============================================================================ */

static BOOL g_pluginModeActive = FALSE;
static volatile LONG g_forceNextUpdate = FALSE;
static CRITICAL_SECTION g_dataCS;
static BOOL g_pluginDataInitialized = FALSE;

/* Watcher thread */
static HANDLE g_hWatchThread = NULL;
static HANDLE g_hWatchStopEvent = NULL;
static HANDLE g_hWatchWakeEvent = NULL;
static HWND g_hNotifyWnd = NULL;
static volatile BOOL g_isRunning = FALSE;

/* Cache for change detection */
static char* g_lastContent = NULL;
static size_t g_lastContentSize = 0;

/* Dynamic poll interval (controlled by <fps:N> tag) */
#define DEFAULT_POLL_INTERVAL_MS 500
#define MIN_POLL_INTERVAL_MS 10
#define MAX_POLL_INTERVAL_MS 5000
#define MAX_PLUGIN_OUTPUT_BYTES (10 * 1024 * 1024)
static volatile DWORD g_pollIntervalMs = DEFAULT_POLL_INTERVAL_MS;

/* Notification throttling */
#define NOTIFY_MIN_INTERVAL_MS 1000
static DWORD g_lastNotifyTime = 0;

/* Pending notification for main thread */
typedef struct {
    wchar_t message[1025];
    int type;           /* -1 = default, 0/1/2 = specific type */
    int timeout;        /* 0 = default, >0 = custom timeout ms */
    BOOL pending;
} PendingNotification;

static PendingNotification g_pendingNotify = {0};

static void WakeWatcherThread(void) {
    if (g_hWatchWakeEvent) {
        SetEvent(g_hWatchWakeEvent);
    }
}

/**
 * @brief Parse and remove <fps:N> tag from content, update poll interval
 * @param content The content to parse (will be modified in place)
 * @return Pointer to content after fps tag removal (or original if no tag)
 */
static const char* ParseFpsTag(const char* content) {
    if (!content) return content;
    
    /* Look for <fps:N> pattern */
    const char* fpsStart = strstr(content, "<fps:");
    if (!fpsStart) return content;
    
    const char* numStart = fpsStart + 5;  /* Skip "<fps:" */
    const char* numEnd = numStart;
    
    /* Parse number */
    while (*numEnd >= '0' && *numEnd <= '9') {
        numEnd++;
    }
    
    /* Check for closing > */
    if (*numEnd != '>' || numEnd == numStart) {
        return content;  /* Invalid format */
    }
    
    /* Extract fps value */
    char numBuf[16] = {0};
    size_t numLen = numEnd - numStart;
    if (numLen >= sizeof(numBuf)) numLen = sizeof(numBuf) - 1;
    strncpy(numBuf, numStart, numLen);
    numBuf[numLen] = '\0';  /* Ensure null termination */
    
    int fps = atoi(numBuf);
    if (fps > 0) {
        /* Convert fps to poll interval: interval = 1000 / fps */
        DWORD interval = 1000 / fps;
        
        /* Clamp to valid range */
        if (interval < MIN_POLL_INTERVAL_MS) interval = MIN_POLL_INTERVAL_MS;
        if (interval > MAX_POLL_INTERVAL_MS) interval = MAX_POLL_INTERVAL_MS;
        
        if (g_pollIntervalMs != interval) {
            g_pollIntervalMs = interval;
            LOG_INFO("PluginData: FPS set to %d (poll interval: %lu ms)", fps, interval);
        }
    }
    
    return content;  /* Keep the tag in content for now (or remove if desired) */
}

/**
 * @brief Remove <fps:N> tag from wide string for display
 */
static void RemoveFpsTagW(wchar_t* text) {
    if (!text) return;
    
    wchar_t* fpsStart = wcsstr(text, L"<fps:");
    if (!fpsStart) return;
    
    wchar_t* fpsEnd = wcschr(fpsStart, L'>');
    if (!fpsEnd) return;
    
    /* Move everything after tag to tag position */
    memmove(fpsStart, fpsEnd + 1, (wcslen(fpsEnd + 1) + 1) * sizeof(wchar_t));
}

/**
 * @brief Parse and process <notify> tags from wide string
 * 
 * Supported formats:
 * - <notify>message</notify>             - Use default notification type and timeout
 * - <notify:catime>message</notify>      - Force Catime notification window
 * - <notify:os>message</notify>          - Force OS system notification
 * - <notify:modal>message</notify>       - Force system modal dialog
 * - <notify:catime:5000>message</notify> - Catime notification with custom timeout (ms)
 * 
 * @param text The text to parse (will be modified to remove tags)
 * @param hwnd Window handle for notification display
 * 
 * @note This function posts a message to the main thread to show the notification,
 *       avoiding UI operations from the background watcher thread.
 */
static void ParseAndShowNotifyTagW(wchar_t* text, HWND hwnd) {
    if (!text || !hwnd) return;
    
    /* Only process notifications when plugin mode is active */
    if (!g_pluginModeActive) return;
    
    wchar_t* searchStart = text;
    
    while (1) {
        /* Find <notify tag start */
        wchar_t* notifyStart = wcsstr(searchStart, L"<notify");
        if (!notifyStart) break;
        
        /* Find closing > of opening tag */
        wchar_t* tagEnd = wcschr(notifyStart, L'>');
        if (!tagEnd) break;
        
        /* Find </notify> closing tag */
        wchar_t* closeTag = wcsstr(tagEnd, L"</notify>");
        if (!closeTag) break;
        
        /* Extract message content (between > and </notify>) */
        size_t msgLen = closeTag - (tagEnd + 1);
        if (msgLen == 0 || msgLen > 1024) {
            /* Empty or too long message, skip this tag */
            searchStart = closeTag + 9;
            continue;
        }
        
        wchar_t message[1025] = {0};
        wcsncpy(message, tagEnd + 1, msgLen);
        message[msgLen] = L'\0';
        
        /* Parse notification type and timeout from tag attributes */
        /* Format: <notify> or <notify:type> or <notify:type:timeout> */
        int notifyType = -1;  /* -1 = use default */
        int customTimeout = 0;  /* 0 = use default */
        
        wchar_t* colonPos = wcschr(notifyStart + 7, L':');
        if (colonPos && colonPos < tagEnd) {
            /* Has type parameter */
            const wchar_t* typeStart = colonPos + 1;
            wchar_t* typeEnd = wcschr(typeStart, L':');
            if (!typeEnd || typeEnd > tagEnd) {
                typeEnd = tagEnd;
            }
            
            size_t typeLen = typeEnd - typeStart;
            if (typeLen > 0 && typeLen < 16) {
                wchar_t typeStr[16] = {0};
                wcsncpy(typeStr, typeStart, typeLen);
                typeStr[typeLen] = L'\0';
                
                if (_wcsicmp(typeStr, L"catime") == 0) {
                    notifyType = NOTIFICATION_TYPE_CATIME;
                } else if (_wcsicmp(typeStr, L"os") == 0) {
                    notifyType = NOTIFICATION_TYPE_OS;
                } else if (_wcsicmp(typeStr, L"modal") == 0) {
                    notifyType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
            }
            
            /* Check for timeout parameter (only for toast) */
            if (typeEnd < tagEnd && *typeEnd == L':') {
                const wchar_t* timeoutStart = typeEnd + 1;
                customTimeout = _wtoi(timeoutStart);
                if (customTimeout < 0) customTimeout = 0;
                if (customTimeout > 60000) customTimeout = 60000;  /* Max 60 seconds */
            }
        }
        
        /* Throttle: check if enough time has passed since last notification */
        /* Note: GetTickCount wraps around after ~49.7 days, but the subtraction
         * still works correctly due to unsigned arithmetic */
        DWORD now = GetTickCount();
        DWORD elapsed = now - g_lastNotifyTime;
        if (elapsed >= NOTIFY_MIN_INTERVAL_MS) {
            g_lastNotifyTime = now;
            
            /* Store pending notification for main thread to process
             * Note: This is called from ParseContent which already holds g_dataCS,
             * so we don't need to acquire the lock here. The main thread will
             * acquire the lock in PluginData_ProcessPendingNotification. */
            wcsncpy(g_pendingNotify.message, message, 1024);
            g_pendingNotify.message[1024] = L'\0';
            g_pendingNotify.type = notifyType;
            g_pendingNotify.timeout = customTimeout;
            g_pendingNotify.pending = TRUE;
            
            /* Post message to main thread to show notification */
            PostMessage(hwnd, WM_PLUGIN_NOTIFY, 0, 0);
            
            LOG_INFO("PluginData: Notification queued (type=%d, timeout=%d): %ls", 
                     notifyType, customTimeout, message);
        } else {
            LOG_INFO("PluginData: Notification throttled (elapsed=%lu ms)", elapsed);
        }
        
        /* Remove the entire <notify>...</notify> tag from text */
        wchar_t* afterClose = closeTag + 9;  /* Skip "</notify>" */
        memmove(notifyStart, afterClose, (wcslen(afterClose) + 1) * sizeof(wchar_t));
        
        /* Continue searching from same position (text shifted) */
        searchStart = notifyStart;
    }
}

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

    if (!g_pluginModeActive) {
        LeaveCriticalSection(&g_dataCS);
        return FALSE;
    }

    ParseFpsTag(content);

    if (PluginExit_IsInProgress()) {
        LeaveCriticalSection(&g_dataCS);
        return TRUE;
    }

    /* Calculate required buffer size */
    int requiredLen = MultiByteToWideChar(CP_UTF8, 0, content, (int)contentLen, NULL, 0);
    if (requiredLen <= 0) {
        LeaveCriticalSection(&g_dataCS);
        return FALSE;
    }

    /* Reallocate buffer if needed */
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

    /* Convert UTF-8 to wide string */
    int len = MultiByteToWideChar(CP_UTF8, 0, content, (int)contentLen, 
                                   g_pluginDisplayText, (int)g_pluginDisplayTextLen);
    if (len > 0) {
        g_pluginDisplayText[len] = L'\0';
        
        /* Remove BOM if present */
        if (g_pluginDisplayText[0] == 0xFEFF) {
            memmove(g_pluginDisplayText, &g_pluginDisplayText[1], len * sizeof(wchar_t));
            len--;
        }
        
        /* Trim trailing whitespace */
        while (len > 0 && (g_pluginDisplayText[len - 1] == L'\n' || 
                           g_pluginDisplayText[len - 1] == L'\r' ||
                           g_pluginDisplayText[len - 1] == L' ')) {
            g_pluginDisplayText[--len] = L'\0';
        }
        
        /* Remove <fps:N> tag from display */
        RemoveFpsTagW(g_pluginDisplayText);
        
        /* Process <notify> tags - show notifications and remove from display */
        ParseAndShowNotifyTagW(g_pluginDisplayText, g_hNotifyWnd);
        
        /* Recalculate length after tag removal */
        len = (int)wcslen(g_pluginDisplayText);
        
        /* Process <exit> tag - if countdown starts, set data flag and return */
        if (PluginExit_ParseTag(g_pluginDisplayText, &len, g_pluginDisplayTextLen)) {
            g_hasPluginData = TRUE;
            LeaveCriticalSection(&g_dataCS);
            return TRUE;
        }
        
        g_hasPluginData = TRUE;
    }

    LeaveCriticalSection(&g_dataCS);
    return len > 0;
}

/*
 * Design note: output.txt is intentionally a stable, shared local IPC surface,
 * but it is only consumed while plugin mode is active.
 * During plugin mode, the active plugin and other same-user helper processes may
 * write compatible content here to build broader local automation flows.
 * Outside plugin mode, Catime intentionally ignores this file.
 * Catime-owned local state text such as "Loading..." or "FAIL" is also allowed
 * to override the file-driven content when needed.
 */
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
 * @brief Ensure plugin output directory exists
 * @note Only creates directory, does NOT clear or modify output.txt
 *       User may have pre-defined content in output.txt
 */
static void EnsureOutputDirExists(const char* filePath) {
    /* First ensure the directory exists */
    char dirPath[MAX_PATH];
    strncpy(dirPath, filePath, MAX_PATH - 1);
    dirPath[MAX_PATH - 1] = '\0';
    
    /* Find last backslash to get directory path */
    char* lastSlash = strrchr(dirPath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        /* Create directory (and parent directories if needed) */
        /* SHCreateDirectoryExA creates all intermediate directories */
        SHCreateDirectoryExA(NULL, dirPath, NULL);
    }
}

static BOOL GetPluginOutputDirectory(wchar_t* buffer, size_t bufferSize) {
    char filePath[MAX_PATH];
    if (!GetPluginOutputPath(filePath, sizeof(filePath))) {
        return FALSE;
    }

    wchar_t fullPath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, fullPath, MAX_PATH) <= 0) {
        return FALSE;
    }
    wchar_t* lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) {
        return FALSE;
    }

    *lastSlash = L'\0';
    wcsncpy(buffer, fullPath, bufferSize - 1);
    buffer[bufferSize - 1] = L'\0';
    return TRUE;
}

static BOOL UpdateLastContentCache(const char* content, DWORD contentSize) {
    size_t requiredSize = (size_t)contentSize + 1;
    char* newBuf = (char*)realloc(g_lastContent, requiredSize);
    if (!newBuf) {
        LOG_ERROR("PluginData: Failed to resize last-content cache to %zu bytes", requiredSize);
        return FALSE;
    }

    g_lastContent = newBuf;
    g_lastContentSize = requiredSize;
    memcpy(g_lastContent, content, contentSize);
    g_lastContent[contentSize] = '\0';
    return TRUE;
}

static BOOL ProcessPluginOutputFile(const char* filePath, BOOL forceRefresh,
                                    FILETIME* lastWriteTime, DWORD* lastFileSize) {
    HANDLE hFile = CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    FILETIME currentWriteTime = {0};
    GetFileTime(hFile, NULL, NULL, &currentWriteTime);

    LARGE_INTEGER sizeValue;
    if (!GetFileSizeEx(hFile, &sizeValue) || sizeValue.QuadPart <= 0) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = 0;
        CloseHandle(hFile);
        return FALSE;
    }

    if (sizeValue.QuadPart > MAX_PLUGIN_OUTPUT_BYTES) {
        LOG_WARNING("PluginData: Skipping output.txt larger than %d bytes", MAX_PLUGIN_OUTPUT_BYTES);
        *lastWriteTime = currentWriteTime;
        *lastFileSize = (DWORD)sizeValue.QuadPart;
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD fileSize = (DWORD)sizeValue.QuadPart;
    if (!forceRefresh &&
        CompareFileTime(&currentWriteTime, lastWriteTime) == 0 &&
        fileSize == *lastFileSize) {
        CloseHandle(hFile);
        return FALSE;
    }

    char* currentContent = (char*)malloc((size_t)fileSize + 1);
    if (!currentContent) {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, currentContent, fileSize, &bytesRead, NULL) && bytesRead > 0;
    CloseHandle(hFile);

    if (!readOk) {
        free(currentContent);
        return FALSE;
    }

    currentContent[bytesRead] = '\0';
    EnterCriticalSection(&g_dataCS);
    BOOL contentChanged = forceRefresh ||
                          g_lastContent == NULL ||
                          g_lastContentSize != (size_t)bytesRead + 1 ||
                          memcmp(currentContent, g_lastContent, (size_t)bytesRead + 1) != 0;

    *lastWriteTime = currentWriteTime;
    *lastFileSize = fileSize;

    if (contentChanged) {
        UpdateLastContentCache(currentContent, bytesRead);
    }
    LeaveCriticalSection(&g_dataCS);

    if (contentChanged && ParseContent(currentContent, bytesRead) && g_hNotifyWnd) {
        InvalidateRect(g_hNotifyWnd, NULL, FALSE);
    }

    free(currentContent);
    return contentChanged;
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

    wchar_t outputDir[MAX_PATH] = {0};
    HANDLE changeHandle = INVALID_HANDLE_VALUE;
    if (GetPluginOutputDirectory(outputDir, MAX_PATH)) {
        changeHandle = FindFirstChangeNotificationW(
            outputDir,
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE
        );
        if (changeHandle == INVALID_HANDLE_VALUE) {
            LOG_WARNING("PluginData: Change notification unavailable, falling back to polling");
        }
    }

    FILETIME lastWriteTime = {0};
    DWORD lastFileSize = 0;

    while (g_isRunning) {
        BOOL forceRefresh = InterlockedExchange(&g_forceNextUpdate, FALSE) != FALSE;
        if (forceRefresh) {
            ZeroMemory(&lastWriteTime, sizeof(lastWriteTime));
            lastFileSize = 0;
        }

        ProcessPluginOutputFile(filePath, forceRefresh, &lastWriteTime, &lastFileSize);

        HANDLE waitHandles[3];
        DWORD waitCount = 0;
        waitHandles[waitCount++] = g_hWatchStopEvent;
        waitHandles[waitCount++] = g_hWatchWakeEvent;
        if (changeHandle != INVALID_HANDLE_VALUE) {
            waitHandles[waitCount++] = changeHandle;
        }

        DWORD waitTimeout = (changeHandle != INVALID_HANDLE_VALUE) ? INFINITE : g_pollIntervalMs;
        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, waitTimeout);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            if (g_hWatchWakeEvent) {
                ResetEvent(g_hWatchWakeEvent);
            }
            continue;
        }
        if (changeHandle != INVALID_HANDLE_VALUE && waitResult == WAIT_OBJECT_0 + 2) {
            if (!FindNextChangeNotification(changeHandle)) {
                FindCloseChangeNotification(changeHandle);
                changeHandle = INVALID_HANDLE_VALUE;
                LOG_WARNING("PluginData: Change notification lost, switching to polling");
            }
            continue;
        }
    }

    if (changeHandle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(changeHandle);
    }
    
    return 0;
}

void PluginData_Init(HWND hwnd) {
    InitializeCriticalSection(&g_dataCS);
    g_pluginDataInitialized = TRUE;
    g_hNotifyWnd = hwnd;
    g_hasPluginData = FALSE;
    g_pluginDisplayText = NULL;
    g_pluginDisplayTextLen = 0;
    g_lastContent = NULL;
    g_lastContentSize = 0;
    g_isRunning = TRUE;
    g_hWatchStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_hWatchWakeEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_hWatchStopEvent || !g_hWatchWakeEvent) {
        LOG_ERROR("PluginData: Failed to create watcher events");
        if (g_hWatchStopEvent) {
            CloseHandle(g_hWatchStopEvent);
            g_hWatchStopEvent = NULL;
        }
        if (g_hWatchWakeEvent) {
            CloseHandle(g_hWatchWakeEvent);
            g_hWatchWakeEvent = NULL;
        }
        g_isRunning = FALSE;
    }

    /* Initialize exit subsystem */
    PluginExit_Init(hwnd, &g_dataCS);

    /* Ensure output directory exists (don't clear user's output.txt) */
    char outputPath[MAX_PATH];
    if (GetPluginOutputPath(outputPath, sizeof(outputPath))) {
        EnsureOutputDirExists(outputPath);
    }

    g_hWatchThread = g_isRunning ? CreateThread(NULL, 0, FileWatcherThread, NULL, 0, NULL) : NULL;
    if (g_hWatchThread) {
        LOG_INFO("PluginData: Initialized");
    } else {
        LOG_ERROR("PluginData: Failed to start watcher thread");
    }
}

void PluginData_Shutdown(void) {
    if (!g_pluginDataInitialized) return;
    
    /* Stop watcher thread */
    if (g_hWatchThread) {
        g_isRunning = FALSE;
        if (g_hWatchStopEvent) {
            SetEvent(g_hWatchStopEvent);
        }
        WakeWatcherThread();
        WaitForSingleObject(g_hWatchThread, INFINITE);
        CloseHandle(g_hWatchThread);
        g_hWatchThread = NULL;
    }
    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
    
    /* Shutdown exit subsystem */
    PluginExit_Shutdown();
    
    /* Free memory */
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
    g_pluginDataInitialized = FALSE;
    LOG_INFO("PluginData: Shutdown");
}

BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;
    if (!g_pluginDataInitialized) return FALSE;

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
    /* Cancel any pending exit countdown */
    PluginExit_Cancel();
    
    /* Reset poll interval to default */
    g_pollIntervalMs = DEFAULT_POLL_INTERVAL_MS;
    
    /* Reset notification throttle */
    g_lastNotifyTime = 0;
    
    if (!g_pluginDataInitialized) return;
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = FALSE;  // Deactivate plugin mode
    g_hasPluginData = FALSE;
    if (g_pluginDisplayText) {
        g_pluginDisplayText[0] = L'\0';
    }
    /* Clear any pending notification to prevent stale notifications */
    g_pendingNotify.pending = FALSE;
    LeaveCriticalSection(&g_dataCS);
}

void PluginData_SetText(const wchar_t* text) {
    if (!text) return;
    if (!g_pluginDataInitialized) return;
    
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
        wcscpy_s(g_pluginDisplayText, g_pluginDisplayTextLen, text);
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
    InterlockedExchange(&g_forceNextUpdate, TRUE);
    WakeWatcherThread();
}

void PluginData_SetActive(BOOL active) {
    if (!g_pluginDataInitialized) return;
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = active;
    if (active) {
        // When activating, force file watcher to re-read immediately
        InterlockedExchange(&g_forceNextUpdate, TRUE);
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
        // Clear any pending notification
        g_pendingNotify.pending = FALSE;
    }
    LeaveCriticalSection(&g_dataCS);
    LOG_INFO("PluginData: Mode %s", active ? "ACTIVE" : "INACTIVE");

    if (active) {
        WakeWatcherThread();

        // If activating, immediately read the file content (don't wait for watcher)
        char filePath[MAX_PATH];
        if (GetPluginOutputPath(filePath, sizeof(filePath))) {
            HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER fileSize = {0};
                if (GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart > 0 &&
                    fileSize.QuadPart < MAX_PLUGIN_OUTPUT_BYTES) {
                    char* content = (char*)malloc((size_t)fileSize.QuadPart + 1);
                    if (content) {
                        DWORD bytesRead = 0;
                        if (ReadFile(hFile, content, (DWORD)fileSize.QuadPart, &bytesRead, NULL) && bytesRead > 0) {
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
    if (!g_pluginDataInitialized) return FALSE;
    BOOL active;
    EnterCriticalSection(&g_dataCS);
    active = g_pluginModeActive;
    LeaveCriticalSection(&g_dataCS);
    return active;
}

BOOL PluginData_HasCatimeTag(void) {
    if (!g_pluginDataInitialized) return FALSE;
    BOOL hasTag = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (g_pluginModeActive && g_hasPluginData && g_pluginDisplayText) {
        // Check for <catime> and </catime> tags
        const wchar_t* start = wcsstr(g_pluginDisplayText, L"<catime>");
        const wchar_t* end = wcsstr(g_pluginDisplayText, L"</catime>");
        if (start && end && end > start) {
            hasTag = TRUE;
        }
    }
    LeaveCriticalSection(&g_dataCS);
    return hasTag;
}

void PluginData_ProcessPendingNotification(HWND hwnd) {
    if (!g_pluginDataInitialized) return;
    
    /* Copy pending notification data under lock, then process outside lock */
    PendingNotification localNotify;
    
    EnterCriticalSection(&g_dataCS);
    if (!g_pendingNotify.pending) {
        LeaveCriticalSection(&g_dataCS);
        return;
    }
    
    /* Copy to local and clear pending flag */
    memcpy(&localNotify, &g_pendingNotify, sizeof(PendingNotification));
    g_pendingNotify.pending = FALSE;
    LeaveCriticalSection(&g_dataCS);
    
    int notifyType = localNotify.type;
    int customTimeout = localNotify.timeout;
    
    /* Save and restore timeout if custom timeout specified */
    int savedTimeout = 0;
    if (customTimeout > 0 && (notifyType == -1 || notifyType == NOTIFICATION_TYPE_CATIME)) {
        savedTimeout = g_AppConfig.notification.display.timeout_ms;
        g_AppConfig.notification.display.timeout_ms = customTimeout;
    }
    
    /* Show notification based on type */
    if (notifyType == -1) {
        /* Use default configured type */
        ShowNotification(hwnd, localNotify.message);
    } else if (notifyType == NOTIFICATION_TYPE_CATIME) {
        ShowToastNotification(hwnd, localNotify.message);
    } else if (notifyType == NOTIFICATION_TYPE_SYSTEM_MODAL) {
        ShowModalNotification(hwnd, localNotify.message);
    } else if (notifyType == NOTIFICATION_TYPE_OS) {
        /* Convert to UTF-8 for tray notification */
        char msgUtf8[2048] = {0};
        if (WideCharToMultiByte(CP_UTF8, 0, localNotify.message, -1, msgUtf8, sizeof(msgUtf8), NULL, NULL) <= 0) {
            LOG_WARNING("PluginData: Failed to convert OS notification message to UTF-8");
            return;
        }
        extern void ShowTrayNotification(HWND hwnd, const char* message);
        ShowTrayNotification(hwnd, msgUtf8);
    }
    
    /* Restore timeout */
    if (savedTimeout > 0) {
        g_AppConfig.notification.display.timeout_ms = savedTimeout;
    }
    
    LOG_INFO("PluginData: Notification displayed (type=%d, timeout=%d)", notifyType, customTimeout);
}

