/**
 * @file resource_watcher.c
 * @brief File system watcher implementation
 */

#include "cache/resource_watcher.h"
#include "cache/resource_cache.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#define WATCH_BUFFER_SIZE 4096

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct {
    HANDLE hDirectory;
    HANDLE hThread;
    OVERLAPPED overlapped;
    BYTE buffer[WATCH_BUFFER_SIZE];
    wchar_t dirPath[MAX_PATH];
    const char* dirName;
} DirectoryWatcher;

static DirectoryWatcher g_fontWatcher = {0};
static DirectoryWatcher g_animWatcher = {0};
static volatile BOOL g_shutdownRequested = FALSE;
static volatile BOOL g_isRunning = FALSE;

/* ============================================================================
 * Path Management
 * ============================================================================ */

static BOOL GetFontsFolderPathInternal(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 20 >= size) return FALSE;
    
    int written = _snwprintf(outPath, size, L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath);
    return (written >= 0 && written < (int)size);
}

static BOOL GetAnimationsFolderPathInternal(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 25 >= size) return FALSE;
    
    int written = _snwprintf(outPath, size, L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath);
    return (written >= 0 && written < (int)size);
}

/* ============================================================================
 * Directory Watching
 * ============================================================================ */

static void ProcessDirectoryChange(DirectoryWatcher* watcher) {
    DWORD bytesReturned = 0;
    
    if (!GetOverlappedResult(watcher->hDirectory, &watcher->overlapped, &bytesReturned, FALSE)) {
        return;
    }
    
    if (bytesReturned == 0 || bytesReturned > WATCH_BUFFER_SIZE) {
        return;
    }
    
    BYTE* bufferEnd = watcher->buffer + bytesReturned;
    FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)watcher->buffer;
    
    while ((BYTE*)fni < bufferEnd) {
        // Verify structure fits in buffer
        if ((BYTE*)fni + sizeof(FILE_NOTIFY_INFORMATION) > bufferEnd) {
            WriteLog(LOG_LEVEL_WARNING, "File notification structure exceeds buffer");
            break;
        }
        
        // Only trigger refresh if file has relevant extension
        wchar_t fileName[MAX_PATH] = {0};
        size_t fileNameLen = fni->FileNameLength / sizeof(wchar_t);
        
        // Bounds check
        if (fileNameLen >= MAX_PATH) {
            fileNameLen = MAX_PATH - 1;
        }
        
        // Verify filename is within buffer
        if ((BYTE*)fni->FileName + fni->FileNameLength > bufferEnd) {
            WriteLog(LOG_LEVEL_WARNING, "Filename exceeds buffer");
            break;
        }
        
        wcsncpy(fileName, fni->FileName, fileNameLen);
        fileName[fileNameLen] = L'\0';
        
        wchar_t* ext = wcsrchr(fileName, L'.');
        BOOL isRelevant = FALSE;
        
        if (ext) {
            if (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0 ||
                _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".bmp") == 0) {
                isRelevant = TRUE;
            }
        } else {
            /* No extension - assume it's a directory change or a file without extension.
             * Trigger refresh to be safe (refresh is debounced/async anyway). */
            isRelevant = TRUE;
        }
        
        if (isRelevant) {
            WriteLog(LOG_LEVEL_INFO, "Resource folder change detected: %ls in %s folder",
                    fileName, watcher->dirName);
            
            // Trigger cache refresh (debounced by ResourceCache_RequestRefresh)
            ResourceCache_RequestRefresh();
            break; // Only refresh once per batch of changes
        }
        
        // Move to next entry
        if (fni->NextEntryOffset == 0) {
            break;
        }
        
        // Verify next offset is valid
        BYTE* nextEntry = (BYTE*)fni + fni->NextEntryOffset;
        if (nextEntry >= bufferEnd) {
            break;
        }
        
        fni = (FILE_NOTIFY_INFORMATION*)nextEntry;
    }
}

static BOOL InitWatchingDirectory(DirectoryWatcher* watcher) {
    // Create event only once during initialization
    memset(&watcher->overlapped, 0, sizeof(OVERLAPPED));
    watcher->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    if (watcher->overlapped.hEvent == NULL) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create event for %s watcher", watcher->dirName);
        return FALSE;
    }
    
    BOOL result = ReadDirectoryChangesW(
        watcher->hDirectory,
        watcher->buffer,
        WATCH_BUFFER_SIZE,
        TRUE, // Watch subtree
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        NULL,
        &watcher->overlapped,
        NULL
    );
    
    if (!result) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to start watching %s folder: %lu",
                watcher->dirName, GetLastError());
        CloseHandle(watcher->overlapped.hEvent);
        watcher->overlapped.hEvent = NULL;
        return FALSE;
    }
    
    return TRUE;
}

static BOOL RestartWatchingDirectory(DirectoryWatcher* watcher) {
    // Reuse existing event, just restart the watch
    if (!watcher->hDirectory || !watcher->overlapped.hEvent) {
        return FALSE;
    }
    
    ResetEvent(watcher->overlapped.hEvent);
    
    BOOL result = ReadDirectoryChangesW(
        watcher->hDirectory,
        watcher->buffer,
        WATCH_BUFFER_SIZE,
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        NULL,
        &watcher->overlapped,
        NULL
    );
    
    if (!result) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to restart watching %s folder: %lu",
                watcher->dirName, GetLastError());
        return FALSE;
    }
    
    return TRUE;
}

static DWORD WINAPI WatcherThread(LPVOID lpParam) {
    (void)lpParam;
    
    WriteLog(LOG_LEVEL_INFO, "Resource watcher thread started");
    
    // Wait a bit for initial cache scan to complete
    // This prevents double-scanning on startup
    Sleep(500);
    
    if (g_shutdownRequested) {
        WriteLog(LOG_LEVEL_INFO, "Watcher thread aborting before start");
        return 0;
    }
    
    // Build events array dynamically, only include valid handles
    HANDLE events[2];
    int eventCount = 0;
    int fontEventIndex = -1;
    int animEventIndex = -1;
    
    if (g_fontWatcher.hDirectory && g_fontWatcher.overlapped.hEvent) {
        events[eventCount] = g_fontWatcher.overlapped.hEvent;
        fontEventIndex = eventCount;
        eventCount++;
    }
    
    if (g_animWatcher.hDirectory && g_animWatcher.overlapped.hEvent) {
        events[eventCount] = g_animWatcher.overlapped.hEvent;
        animEventIndex = eventCount;
        eventCount++;
    }
    
    if (eventCount == 0) {
        WriteLog(LOG_LEVEL_ERROR, "No valid events to watch");
        return 1;
    }
    
    while (!g_shutdownRequested) {
        DWORD waitResult = WaitForMultipleObjects(eventCount, events, FALSE, 1000);
        
        if (g_shutdownRequested) {
            break;
        }
        
        if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + eventCount) {
            int index = waitResult - WAIT_OBJECT_0;
            
            if (index == fontEventIndex) {
                // Font folder changed
                ProcessDirectoryChange(&g_fontWatcher);
                RestartWatchingDirectory(&g_fontWatcher);
            }
            else if (index == animEventIndex) {
                // Animation folder changed
                ProcessDirectoryChange(&g_animWatcher);
                RestartWatchingDirectory(&g_animWatcher);
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            // Normal timeout, continue
            continue;
        }
        else {
            // Error or unexpected result
            if (!g_shutdownRequested) {
                WriteLog(LOG_LEVEL_WARNING, "Watcher WaitForMultipleObjects returned %lu", waitResult);
            }
        }
    }
    
    WriteLog(LOG_LEVEL_INFO, "Resource watcher thread exiting");
    return 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

BOOL ResourceWatcher_Start(void) {
    if (g_isRunning) {
        WriteLog(LOG_LEVEL_WARNING, "Resource watcher already running");
        return TRUE;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Starting resource watcher");
    
    // Open fonts directory
    if (!GetFontsFolderPathInternal(g_fontWatcher.dirPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to get fonts folder path");
        return FALSE;
    }
    
    g_fontWatcher.dirName = "fonts";
    g_fontWatcher.hDirectory = CreateFileW(
        g_fontWatcher.dirPath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (g_fontWatcher.hDirectory == INVALID_HANDLE_VALUE) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to open fonts folder for watching: %lu", GetLastError());
        g_fontWatcher.hDirectory = NULL;
    }
    
    // Open animations directory
    if (!GetAnimationsFolderPathInternal(g_animWatcher.dirPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to get animations folder path");
        if (g_fontWatcher.hDirectory) {
            CloseHandle(g_fontWatcher.hDirectory);
            g_fontWatcher.hDirectory = NULL;
        }
        return FALSE;
    }
    
    g_animWatcher.dirName = "animations";
    g_animWatcher.hDirectory = CreateFileW(
        g_animWatcher.dirPath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (g_animWatcher.hDirectory == INVALID_HANDLE_VALUE) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to open animations folder for watching: %lu", GetLastError());
        g_animWatcher.hDirectory = NULL;
    }
    
    // Need at least one valid directory
    if (!g_fontWatcher.hDirectory && !g_animWatcher.hDirectory) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to open any resource folders for watching");
        return FALSE;
    }
    
    // Start watching
    BOOL fontOk = TRUE;
    BOOL animOk = TRUE;
    
    if (g_fontWatcher.hDirectory) {
        fontOk = InitWatchingDirectory(&g_fontWatcher);
        if (!fontOk) {
            CloseHandle(g_fontWatcher.hDirectory);
            g_fontWatcher.hDirectory = NULL;
        }
    }
    
    if (g_animWatcher.hDirectory) {
        animOk = InitWatchingDirectory(&g_animWatcher);
        if (!animOk) {
            CloseHandle(g_animWatcher.hDirectory);
            g_animWatcher.hDirectory = NULL;
        }
    }
    
    if (!fontOk && !animOk) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to start watching any folders");
        return FALSE;
    }
    
    // Create watcher thread
    g_shutdownRequested = FALSE;
    g_fontWatcher.hThread = CreateThread(NULL, 0, WatcherThread, NULL, 0, NULL);
    
    if (g_fontWatcher.hThread == NULL) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create watcher thread");
        
        if (g_fontWatcher.hDirectory) {
            if (g_fontWatcher.overlapped.hEvent) {
                CloseHandle(g_fontWatcher.overlapped.hEvent);
                g_fontWatcher.overlapped.hEvent = NULL;
            }
            CloseHandle(g_fontWatcher.hDirectory);
            g_fontWatcher.hDirectory = NULL;
        }
        
        if (g_animWatcher.hDirectory) {
            if (g_animWatcher.overlapped.hEvent) {
                CloseHandle(g_animWatcher.overlapped.hEvent);
                g_animWatcher.overlapped.hEvent = NULL;
            }
            CloseHandle(g_animWatcher.hDirectory);
            g_animWatcher.hDirectory = NULL;
        }
        
        return FALSE;
    }
    
    SetThreadPriority(g_fontWatcher.hThread, THREAD_PRIORITY_BELOW_NORMAL);
    g_isRunning = TRUE;
    
    WriteLog(LOG_LEVEL_INFO, "Resource watcher started successfully");
    return TRUE;
}

void ResourceWatcher_Stop(void) {
    if (!g_isRunning) {
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Stopping resource watcher");
    
    g_shutdownRequested = TRUE;
    
    // Wait for thread to exit
    if (g_fontWatcher.hThread) {
        DWORD waitResult = WaitForSingleObject(g_fontWatcher.hThread, 3000);
        if (waitResult == WAIT_TIMEOUT) {
            WriteLog(LOG_LEVEL_WARNING, "Watcher thread did not exit within 3 seconds");
            // Still close handle - thread will be leaked but safer than hanging
        }
        CloseHandle(g_fontWatcher.hThread);
        g_fontWatcher.hThread = NULL;
    }
    
    // Close handles
    if (g_fontWatcher.hDirectory) {
        CancelIo(g_fontWatcher.hDirectory);
        if (g_fontWatcher.overlapped.hEvent) {
            CloseHandle(g_fontWatcher.overlapped.hEvent);
            g_fontWatcher.overlapped.hEvent = NULL;
        }
        CloseHandle(g_fontWatcher.hDirectory);
        g_fontWatcher.hDirectory = NULL;
    }
    
    if (g_animWatcher.hDirectory) {
        CancelIo(g_animWatcher.hDirectory);
        if (g_animWatcher.overlapped.hEvent) {
            CloseHandle(g_animWatcher.overlapped.hEvent);
            g_animWatcher.overlapped.hEvent = NULL;
        }
        CloseHandle(g_animWatcher.hDirectory);
        g_animWatcher.hDirectory = NULL;
    }
    
    g_isRunning = FALSE;
    
    WriteLog(LOG_LEVEL_INFO, "Resource watcher stopped");
}

BOOL ResourceWatcher_IsRunning(void) {
    return g_isRunning;
}
