/**
 * @file config_watcher_internal.h
 * @brief Shared implementation details for configuration watcher modules.
 */

#ifndef CATIME_CONFIG_WATCHER_INTERNAL_H
#define CATIME_CONFIG_WATCHER_INTERNAL_H

#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_watcher.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "tray/tray_animation_core.h"
#include "log.h"

#define DEBOUNCE_DELAY_MS 200
#define WATCH_EVENT_COUNT 2
#define WATCHER_STOP_TIMEOUT_MS 2000
#define WATCHER_FINAL_STOP_TIMEOUT_MS 5000
#define WATCH_CHANGE_FILTER \
    (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | \
     FILE_NOTIFY_CHANGE_SIZE)
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif

typedef struct {
    HANDLE stopEvent;
    HWND targetHwnd;
} ConfigWatcherThreadContext;

typedef struct {
    BOOL exists;
    FILETIME lastWriteTime;
    ULONGLONG fileSize;
} ConfigFileSnapshot;

extern HANDLE g_watcherThread;
extern HANDLE g_stopEvent;
extern HWND g_targetHwnd;
extern volatile LONG g_configReloadPending;
extern volatile LONG g_configReloadDirty;
extern volatile LONG g_acceptingChanges;

BOOL ConfigWatcher_IsValidTargetWindow(HWND hwnd);
DWORD WINAPI ConfigWatcher_ThreadProc(LPVOID lpParam);

#endif /* CATIME_CONFIG_WATCHER_INTERNAL_H */
