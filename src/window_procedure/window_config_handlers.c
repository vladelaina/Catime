/**
 * @file window_config_handlers.c
 * @brief Dispatches a complete configuration reload.
 */

#include "window_procedure/window_config_handlers.h"

#include "config/config_watcher.h"

LRESULT HandleAppConfigChanged(HWND hwnd) {
    ConfigWatcher_BeginConfigReloadHandling();
    HandleAppAnimSpeedChanged(hwnd);
    HandleAppAnimPathChanged(hwnd);
    HandleAppDisplayChanged(hwnd);
    HandleAppTimerChanged(hwnd);
    HandleAppPomodoroChanged(hwnd);
    HandleAppNotificationChanged(hwnd);
    HandleAppHotkeysChanged(hwnd);
    HandleAppRecentFilesChanged(hwnd);
    HandleAppColorsChanged(hwnd);
    ConfigWatcher_EndConfigReloadHandling(hwnd);
    return 0;
}
