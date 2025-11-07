/**
 * @file config_watcher.c
 * @brief Live config reload via ReadDirectoryChangesW
 * 
 * Event-driven (ReadDirectoryChangesW) vs polling: Lower CPU usage.
 * 200ms debounce batches rapid editor writes (temp file + rename + metadata).
 */

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#include "config/config_watcher.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "tray/tray_animation_core.h"

/* 200ms debounce batches rapid editor writes (mentioned in file header) */
#define WATCH_BUFFER_SIZE 8192
#define DEBOUNCE_DELAY_MS 200
#define WATCH_EVENT_COUNT 2

#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif

static HANDLE g_watcherThread = NULL;
static HANDLE g_stopEvent = NULL;
static HWND g_targetHwnd = NULL;

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

static const wchar_t* GetFileNameFromPath(const wchar_t* path) {
    const wchar_t* name = wcsrchr(path, L'\\');
    if (!name) name = wcsrchr(path, L'/');
    return name ? (name + 1) : path;
}

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

static void InitializeWatchEvents(HANDLE* hEvents, OVERLAPPED* pOv) {
    hEvents[0] = g_stopEvent;
    hEvents[1] = CreateEventW(NULL, TRUE, FALSE, NULL);
    memset(pOv, 0, sizeof(OVERLAPPED));
    pOv->hEvent = hEvents[1];
}

static void CleanupWatchEvents(HANDLE* hEvents) {
    if (hEvents[1]) {
        CloseHandle(hEvents[1]);
        hEvents[1] = NULL;
    }
}

/** Background thread required (ReadDirectoryChangesW blocks) */
static DWORD WINAPI WatcherThreadProc(LPVOID lpParam) {
    (void)lpParam;
    
    char iniPath[MAX_PATH] = {0};
    GetConfigPath(iniPath, sizeof(iniPath));
    
    wchar_t wDir[MAX_PATH] = {0};
    HANDLE hDir = SetupDirectoryWatch(iniPath, wDir, MAX_PATH);
    if (hDir == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    BYTE buffer[WATCH_BUFFER_SIZE];
    DWORD bytesReturned = 0;
    OVERLAPPED ov = {0};
    HANDLE hEvents[WATCH_EVENT_COUNT];
    InitializeWatchEvents(hEvents, &ov);
    
    wchar_t wIni[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, iniPath, -1, wIni, MAX_PATH);
    const wchar_t* wFileName = GetFileNameFromPath(wIni);
    
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
            break;
        }
        
        DWORD bytes = 0;
        if (!GetOverlappedResult(hDir, &ov, &bytes, FALSE)) {
            continue;
        }
        
        if (IsTargetFileChanged(buffer, bytes, wFileName)) {
            Sleep(DEBOUNCE_DELAY_MS);
            NotifyConfigChanges(g_targetHwnd);
        }
    }
    
    CloseHandle(hDir);
    CleanupWatchEvents(hEvents);
    return 0;
}

void ConfigWatcher_Start(HWND hwnd) {
    if (g_watcherThread) return;
    
    g_targetHwnd = hwnd;
    g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_watcherThread = CreateThread(NULL, 0, WatcherThreadProc, NULL, 0, NULL);
}

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
