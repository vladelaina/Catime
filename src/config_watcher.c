/**
 * @file config_watcher.c
 * @brief Watch config.ini changes via ReadDirectoryChangesW and notify UI
 * @version 2.0 - Refactored for better modularity and maintainability
 */

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#include "../include/config_watcher.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/tray_animation.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Buffer size for directory change notifications */
#define WATCH_BUFFER_SIZE 8192

/** @brief Debounce delay to avoid processing rapid file changes */
#define DEBOUNCE_DELAY_MS 200

/** @brief Number of events to monitor (stop event + notification event) */
#define WATCH_EVENT_COUNT 2

/** @brief Custom messages for animation config hot-reload */
#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif

/* ============================================================================
 * Global state
 * ============================================================================ */

static HANDLE g_watcherThread = NULL;
static HANDLE g_stopEvent = NULL;
static HWND g_targetHwnd = NULL;

/* ============================================================================
 * Helper functions
 * ============================================================================ */

/**
 * @brief Extract directory path from full file path
 * @param filePath Full path to file
 * @param dirPath Output buffer for directory path
 * @param dirPathSize Size of output buffer
 */
static void ExtractDirectoryPath(const char* filePath, char* dirPath, size_t dirPathSize) {
    strncpy(dirPath, filePath, dirPathSize - 1);
    dirPath[dirPathSize - 1] = '\0';
    
    char* lastSlash = strrchr(dirPath, '\\');
    if (!lastSlash) lastSlash = strrchr(dirPath, '/');
    
    if (lastSlash) {
        *lastSlash = '\0';
    } else {
        strncpy(dirPath, ".\\", dirPathSize - 1);
        dirPath[dirPathSize - 1] = '\0';
    }
}

/**
 * @brief Extract filename from full path (supports both slash types)
 * @param path Full path
 * @return Pointer to filename within path
 */
static const wchar_t* GetFileNameFromPath(const wchar_t* path) {
    const wchar_t* name = wcsrchr(path, L'\\');
    if (!name) name = wcsrchr(path, L'/');
    return name ? (name + 1) : path;
}

/**
 * @brief Check if target file was changed in notification buffer
 * @param buffer Notification buffer from ReadDirectoryChangesW
 * @param bytes Number of bytes in buffer
 * @param targetFileName Filename to check for
 * @return TRUE if target file was changed, FALSE otherwise
 */
static BOOL IsTargetFileChanged(const BYTE* buffer, DWORD bytes, const wchar_t* targetFileName) {
    const BYTE* ptr = buffer;
    
    while (ptr < buffer + bytes) {
        const FILE_NOTIFY_INFORMATION* pinfo = (const FILE_NOTIFY_INFORMATION*)ptr;
        
        if (pinfo->FileNameLength > 0) {
            size_t cch = pinfo->FileNameLength / sizeof(WCHAR);
            WCHAR nameBuf[MAX_PATH];
            size_t copy = (cch >= MAX_PATH) ? (MAX_PATH - 1) : cch;
            wcsncpy(nameBuf, pinfo->FileName, copy);
            nameBuf[copy] = L'\0';
            
            if (_wcsicmp(nameBuf, targetFileName) == 0) {
                return TRUE;
            }
        }
        
        if (pinfo->NextEntryOffset == 0) break;
        ptr += pinfo->NextEntryOffset;
    }
    
    return FALSE;
}

/**
 * @brief Notify main window of all config-related changes
 * @param hwnd Target window handle
 */
static void NotifyConfigChanges(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    
    static const UINT configChangeMessages[] = {
        WM_APP_ANIM_SPEED_CHANGED,
        WM_APP_ANIM_PATH_CHANGED,
        WM_APP_DISPLAY_CHANGED,
        WM_APP_TIMER_CHANGED,
        WM_APP_POMODORO_CHANGED,
        WM_APP_NOTIFICATION_CHANGED,
        WM_APP_HOTKEYS_CHANGED,
        WM_APP_RECENTFILES_CHANGED,
        WM_APP_COLORS_CHANGED
    };
    
    for (size_t i = 0; i < sizeof(configChangeMessages) / sizeof(UINT); i++) {
        PostMessage(hwnd, configChangeMessages[i], 0, 0);
    }
}

/**
 * @brief Setup directory monitoring handle
 * @param iniPath Full path to INI file
 * @param outDir Output buffer for directory path (wide char)
 * @param outDirSize Size of output buffer
 * @return Directory handle or INVALID_HANDLE_VALUE on failure
 */
static HANDLE SetupDirectoryWatch(const char* iniPath, wchar_t* outDir, size_t outDirSize) {
    char dirPath[MAX_PATH];
    ExtractDirectoryPath(iniPath, dirPath, sizeof(dirPath));
    
    MultiByteToWideChar(CP_UTF8, 0, dirPath, -1, outDir, (int)outDirSize);
    
    return CreateFileW(outDir, FILE_LIST_DIRECTORY,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL, OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                      NULL);
}

/**
 * @brief Initialize overlapped I/O events for directory watching
 * @param hEvents Output array for event handles
 * @param pOv Pointer to OVERLAPPED structure to initialize
 */
static void InitializeWatchEvents(HANDLE* hEvents, OVERLAPPED* pOv) {
    hEvents[0] = g_stopEvent;
    hEvents[1] = CreateEventW(NULL, TRUE, FALSE, NULL);
    memset(pOv, 0, sizeof(OVERLAPPED));
    pOv->hEvent = hEvents[1];
}

/**
 * @brief Cleanup watch events
 * @param hEvents Array of event handles
 */
static void CleanupWatchEvents(HANDLE* hEvents) {
    if (hEvents[1]) {
        CloseHandle(hEvents[1]);
        hEvents[1] = NULL;
    }
}

/* ============================================================================
 * Main watcher thread
 * ============================================================================ */

/**
 * @brief Directory watcher thread procedure
 * @param lpParam Unused thread parameter
 * @return Thread exit code
 */
static DWORD WINAPI WatcherThreadProc(LPVOID lpParam) {
    (void)lpParam;
    
    // Get config file path
    char iniPath[MAX_PATH] = {0};
    GetConfigPath(iniPath, sizeof(iniPath));
    
    // Setup directory monitoring
    wchar_t wDir[MAX_PATH] = {0};
    HANDLE hDir = SetupDirectoryWatch(iniPath, wDir, MAX_PATH);
    if (hDir == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    // Initialize watch events
    BYTE buffer[WATCH_BUFFER_SIZE];
    DWORD bytesReturned = 0;
    OVERLAPPED ov = {0};
    HANDLE hEvents[WATCH_EVENT_COUNT];
    InitializeWatchEvents(hEvents, &ov);
    
    // Get target filename for comparison
    wchar_t wIni[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, iniPath, -1, wIni, MAX_PATH);
    const wchar_t* wFileName = GetFileNameFromPath(wIni);
    
    // Main watch loop
    for (;;) {
        ResetEvent(hEvents[1]);
        
        if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | 
                                   FILE_NOTIFY_CHANGE_LAST_WRITE | 
                                   FILE_NOTIFY_CHANGE_SIZE,
                                   &bytesReturned, &ov, NULL)) {
            break;
        }
        
        DWORD wait = WaitForMultipleObjects(WATCH_EVENT_COUNT, hEvents, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            break; // Stop event signaled
        }
        
        DWORD bytes = 0;
        if (!GetOverlappedResult(hDir, &ov, &bytes, FALSE)) {
            continue;
        }
        
        // Check if our config file was changed
        if (IsTargetFileChanged(buffer, bytes, wFileName)) {
            Sleep(DEBOUNCE_DELAY_MS);
            NotifyConfigChanges(g_targetHwnd);
        }
    }
    
    // Cleanup
    CloseHandle(hDir);
    CleanupWatchEvents(hEvents);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Start config file watcher thread
 * @param hwnd Window handle to receive change notifications
 */
void ConfigWatcher_Start(HWND hwnd) {
    if (g_watcherThread) return;
    
    g_targetHwnd = hwnd;
    g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_watcherThread = CreateThread(NULL, 0, WatcherThreadProc, NULL, 0, NULL);
}

/**
 * @brief Stop config file watcher thread and cleanup resources
 */
void ConfigWatcher_Stop(void) {
    if (!g_watcherThread) return;
    
    SetEvent(g_stopEvent);
    WaitForSingleObject(g_watcherThread, INFINITE);
    CloseHandle(g_watcherThread);
    g_watcherThread = NULL;
    
    CloseHandle(g_stopEvent);
    g_stopEvent = NULL;
    
    g_targetHwnd = NULL;
}
