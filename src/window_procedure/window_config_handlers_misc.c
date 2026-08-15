/**
 * @file window_config_handlers_misc.c
 * @brief Reloads recent files, hotkeys, and shared monitor settings.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "window_procedure/window_hotkeys.h"
#include "config.h"
#include "log.h"
#include "taskbar_monitor.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_utils.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char recentFiles[MAX_RECENT_FILES][MAX_PATH];
    TimeoutActionType timeoutAction;
    char timeoutFile[MAX_PATH];
} HotReloadRecentFilesConfig;

static BOOL g_hotReloadRecentFilesConfigValid = FALSE;
static HotReloadRecentFilesConfig g_lastHotReloadRecentFilesConfig = {0};

static void ReadHotReloadRecentFilesConfig(HotReloadRecentFilesConfig* config) {
    if (!config) return;

    ZeroMemory(config, sizeof(*config));
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        if (!WindowConfigInternal_ReadStringExact(INI_SECTION_RECENTFILES, key, "",
                                      config->recentFiles[i],
                                      sizeof(config->recentFiles[i]))) {
            config->recentFiles[i][0] = '\0';
        }
    }

    config->timeoutAction = CLOCK_TIMEOUT_ACTION;
    strncpy_s(config->timeoutFile, sizeof(config->timeoutFile),
              CLOCK_TIMEOUT_FILE_PATH, _TRUNCATE);
}

static BOOL HotReloadRecentFilesChanged(const HotReloadRecentFilesConfig* config) {
    if (!config || !g_hotReloadRecentFilesConfigValid) {
        return TRUE;
    }

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        if (strcmp(config->recentFiles[i],
                   g_lastHotReloadRecentFilesConfig.recentFiles[i]) != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL HotReloadRecentValidationInputChanged(const HotReloadRecentFilesConfig* config) {
    return !config ||
           !g_hotReloadRecentFilesConfigValid ||
           config->timeoutAction != g_lastHotReloadRecentFilesConfig.timeoutAction ||
           strcmp(config->timeoutFile, g_lastHotReloadRecentFilesConfig.timeoutFile) != 0;
}

static void RememberHotReloadRecentFilesConfig(HotReloadRecentFilesConfig* config) {
    if (!config) return;

    config->timeoutAction = CLOCK_TIMEOUT_ACTION;
    strncpy_s(config->timeoutFile, sizeof(config->timeoutFile),
              CLOCK_TIMEOUT_FILE_PATH, _TRUNCATE);
    g_lastHotReloadRecentFilesConfig = *config;
    g_hotReloadRecentFilesConfigValid = TRUE;
}

LRESULT HandleAppHotkeysChanged(HWND hwnd) {
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

LRESULT HandleAppRecentFilesChanged(HWND hwnd) {
    (void)hwnd;

    HotReloadRecentFilesConfig recentConfig = {0};
    ReadHotReloadRecentFilesConfig(&recentConfig);

    BOOL recentFilesChanged = HotReloadRecentFilesChanged(&recentConfig);
    BOOL validationInputChanged = HotReloadRecentValidationInputChanged(&recentConfig);
    if (!recentFilesChanged && !validationInputChanged) {
        return 0;
    }

    if (recentFilesChanged) {
        extern void LoadRecentFiles(void);
        LoadRecentFiles();
    }

    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE) {
        int recentFilesCount = g_AppConfig.recent_files.count;
        if (recentFilesCount < 0) recentFilesCount = 0;
        if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;

        if (CLOCK_TIMEOUT_FILE_PATH[0] == '\0' && recentFilesCount > 0 &&
            !WriteConfigTimeoutFile(g_AppConfig.recent_files.files[0].path)) {
            LOG_WARNING("Failed to save hot-reloaded timeout file: %s",
                        g_AppConfig.recent_files.files[0].path);
        }
    }

    RememberHotReloadRecentFilesConfig(&recentConfig);
    return 0;
}

LRESULT HandleAppColorsChanged(HWND hwnd) {
    (void)hwnd;
    /* Active colors and palette choices are process-local after startup. */
    return 0;
}

LRESULT HandleAppAnimSpeedChanged(HWND hwnd) {
    (void)hwnd;
    /* Animation speed is runtime-local after startup. Menu commands update
     * this process immediately and persist the latest choice for new ones. */
    return 0;
}

LRESULT HandleAppAnimPathChanged(HWND hwnd) {
    (void)hwnd;
    /* The taskbar monitor is one shared surface, so its options remain
     * synchronized even though each process keeps its own tray animation. */
    BOOL taskbarMonitorEnabled = ReadConfigBool(
        "Animation", "TASKBAR_MONITOR_ENABLED", FALSE);
    BOOL taskbarMonitorCpuMemory = taskbarMonitorEnabled && ReadConfigBool(
        "Animation", "TASKBAR_MONITOR_CPU_MEMORY", TRUE);
    BOOL taskbarMonitorNetwork = taskbarMonitorEnabled && ReadConfigBool(
        "Animation", "TASKBAR_MONITOR_NETWORK", TRUE);
    if (taskbarMonitorCpuMemory != TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_CPU_MEMORY) ||
        taskbarMonitorNetwork != TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_NETWORK)) {
        TaskbarMonitor_ApplyConfig(
            taskbarMonitorEnabled,
            taskbarMonitorCpuMemory,
            taskbarMonitorNetwork);
        RefreshTrayBackgroundWorkState();
    }
    return 0;
}
