/**
 * @file config_watcher.c
 * @brief Watch config.ini changes via ReadDirectoryChangesW and notify UI
 */

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#include "../include/config_watcher.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/tray_animation.h"

/** Custom messages for animation config hot-reload */
#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif

static HANDLE g_watcherThread = NULL;
static HANDLE g_stopEvent = NULL;
static HWND g_targetHwnd = NULL;

static DWORD WINAPI WatcherThreadProc(LPVOID lpParam) {
    (void)lpParam;

    char iniPath[MAX_PATH] = {0};
    GetConfigPath(iniPath, sizeof(iniPath));

    char dirPath[MAX_PATH] = {0};
    strncpy(dirPath, iniPath, sizeof(dirPath) - 1);
    char *lastSlash = strrchr(dirPath, '\\');
    if (!lastSlash) lastSlash = strrchr(dirPath, '/');
    if (lastSlash) {
        *lastSlash = '\0';
    } else {
        strncpy(dirPath, ".\\", sizeof(dirPath) - 1);
        dirPath[sizeof(dirPath) - 1] = '\0';
    }

    wchar_t wDir[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, dirPath, -1, wDir, MAX_PATH);

    HANDLE hDir = CreateFileW(wDir, FILE_LIST_DIRECTORY,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                               NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        return 0;
    }

    BYTE buffer[8192];
    DWORD bytesReturned = 0;
    OVERLAPPED ov = {0};
    HANDLE hEvents[2];
    hEvents[0] = g_stopEvent;
    hEvents[1] = CreateEventW(NULL, TRUE, FALSE, NULL);
    ov.hEvent = hEvents[1];

    for (;;) {
        ResetEvent(hEvents[1]);
        if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                                   &bytesReturned, &ov, NULL)) {
            break;
        }
        DWORD wait = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            break; /** stop event */
        }

        DWORD bytes = 0;
        if (!GetOverlappedResult(hDir, &ov, &bytes, FALSE)) {
            continue;
        }

        /** Only react to changes on the config.ini file */
        wchar_t wIni[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, iniPath, -1, wIni, MAX_PATH);
        const wchar_t* wFileName = wcsrchr(wIni, L'\\');
        if (!wFileName) wFileName = wcsrchr(wIni, L'/');
        wFileName = wFileName ? (wFileName + 1) : wIni;

        BOOL match = FALSE;
        BYTE* ptr = buffer;
        while (ptr < buffer + bytes) {
            FILE_NOTIFY_INFORMATION* pinfo = (FILE_NOTIFY_INFORMATION*)ptr;
            if (pinfo->FileNameLength > 0) {
                size_t cch = pinfo->FileNameLength / sizeof(WCHAR);
                WCHAR nameBuf[MAX_PATH];
                size_t copy = (cch >= MAX_PATH) ? (MAX_PATH - 1) : cch;
                wcsncpy(nameBuf, pinfo->FileName, copy);
                nameBuf[copy] = L'\0';
                if (_wcsicmp(nameBuf, wFileName) == 0) {
                    match = TRUE;
                }
            }
            if (pinfo->NextEntryOffset == 0) break;
            ptr += pinfo->NextEntryOffset;
        }

        if (match) {
            /** Debounce small bursts */
            Sleep(200);
            if (g_targetHwnd && IsWindow(g_targetHwnd)) {
                PostMessage(g_targetHwnd, WM_APP_ANIM_SPEED_CHANGED, 0, 0);
                PostMessage(g_targetHwnd, WM_APP_ANIM_PATH_CHANGED, 0, 0);
                PostMessage(g_targetHwnd, WM_APP_DISPLAY_CHANGED, 0, 0);
                /** Notify timer section change as well */
                PostMessage(g_targetHwnd, WM_APP_TIMER_CHANGED, 0, 0);
            }
        }
    }

    CloseHandle(hDir);
    CloseHandle(hEvents[1]);
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


