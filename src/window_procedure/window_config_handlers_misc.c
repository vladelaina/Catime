/**
 * @file window_config_handlers_misc.c
 * @brief Reloads colors, recent files, hotkeys, and tray animation settings.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "window_procedure/window_hotkeys.h"
#include "color/color.h"
#include "color/gradient.h"
#include "config.h"
#include "config/config_defaults.h"
#include "log.h"
#include "taskbar_monitor.h"
#include "tray/tray.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_utils.h"

#include <stdio.h>
#include <string.h>

#define COLOR_OPTIONS_CONFIG_BUFFER 2048
#define PERCENT_ICON_COLOR_CONFIG_BUFFER 32

typedef struct {
    BOOL colorOptionsValid;
    char colorOptions[COLOR_OPTIONS_CONFIG_BUFFER];
    char percentIconTextColor[PERCENT_ICON_COLOR_CONFIG_BUFFER];
    char percentIconBgColor[PERCENT_ICON_COLOR_CONFIG_BUFFER];
} HotReloadColorConfig;

static BOOL g_hotReloadColorConfigValid = FALSE;
static HotReloadColorConfig g_lastHotReloadColorConfig = {0};

typedef struct {
    char recentFiles[MAX_RECENT_FILES][MAX_PATH];
    TimeoutActionType timeoutAction;
    char timeoutFile[MAX_PATH];
} HotReloadRecentFilesConfig;

static BOOL g_hotReloadRecentFilesConfigValid = FALSE;
static HotReloadRecentFilesConfig g_lastHotReloadRecentFilesConfig = {0};

static void ReadHotReloadColorConfig(HotReloadColorConfig* config) {
    if (!config) return;

    config->colorOptionsValid = ReadIniStringExact(
        CFG_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI,
        config->colorOptions, sizeof(config->colorOptions), GetCachedConfigPath());
    if (!config->colorOptionsValid) {
        config->colorOptions[0] = '\0';
        LOG_WARNING("Hot reload ignored COLOR_OPTIONS because the config value is too long");
    }
    ReadConfigStr("Animation", "PERCENT_ICON_TEXT_COLOR", "auto",
                  config->percentIconTextColor, sizeof(config->percentIconTextColor));
    ReadConfigStr("Animation", "PERCENT_ICON_BG_COLOR", "transparent",
                  config->percentIconBgColor, sizeof(config->percentIconBgColor));
}

static BOOL HotReloadColorOptionsChanged(const HotReloadColorConfig* config) {
    if (!config || !config->colorOptionsValid) return FALSE;
    return !g_hotReloadColorConfigValid ||
           strcmp(config->colorOptions, g_lastHotReloadColorConfig.colorOptions) != 0;
}

static BOOL HotReloadPercentIconColorsChanged(const HotReloadColorConfig* config) {
    return !g_hotReloadColorConfigValid ||
           strcmp(config->percentIconTextColor,
                  g_lastHotReloadColorConfig.percentIconTextColor) != 0 ||
           strcmp(config->percentIconBgColor,
                  g_lastHotReloadColorConfig.percentIconBgColor) != 0;
}

static void RememberHotReloadColorConfig(const HotReloadColorConfig* config) {
    if (!config) return;

    if (config->colorOptionsValid) {
        g_lastHotReloadColorConfig.colorOptionsValid = TRUE;
        memcpy(g_lastHotReloadColorConfig.colorOptions,
               config->colorOptions,
               sizeof(g_lastHotReloadColorConfig.colorOptions));
    } else if (!g_hotReloadColorConfigValid) {
        g_lastHotReloadColorConfig.colorOptionsValid = FALSE;
        g_lastHotReloadColorConfig.colorOptions[0] = '\0';
    }
    memcpy(g_lastHotReloadColorConfig.percentIconTextColor,
           config->percentIconTextColor,
           sizeof(g_lastHotReloadColorConfig.percentIconTextColor));
    memcpy(g_lastHotReloadColorConfig.percentIconBgColor,
           config->percentIconBgColor,
           sizeof(g_lastHotReloadColorConfig.percentIconBgColor));
    g_hotReloadColorConfigValid = TRUE;
}

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
    HotReloadColorConfig colorConfig = {0};
    ReadHotReloadColorConfig(&colorConfig);

    BOOL colorOptionsChanged = HotReloadColorOptionsChanged(&colorConfig);
    BOOL percentIconColorsChanged = HotReloadPercentIconColorsChanged(&colorConfig);
    if (!colorOptionsChanged && !percentIconColorsChanged) {
        return 0;
    }

    if (colorOptionsChanged) {
        if (ReplaceColorOptionsFromConfigValue(colorConfig.colorOptions)) {
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            LOG_WARNING("Hot reload ignored invalid COLOR_OPTIONS; keeping current palette");
        }
    }

    if (percentIconColorsChanged) {
        ReadPercentIconColorsConfig();
        TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
    }

    RememberHotReloadColorConfig(&colorConfig);
    return 0;
}

LRESULT HandleAppAnimSpeedChanged(HWND hwnd) {
    (void)hwnd;
    ReloadAnimationSpeedFromConfig();
    TrayAnimation_RecomputeTimerDelay();
    return 0;
}

LRESULT HandleAppAnimPathChanged(HWND hwnd) {
    (void)hwnd;
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
    char buffer[MAX_PATH] = {0};
    if (!ReadIniStringExact("Animation", "ANIMATION_PATH", "__logo__",
                            buffer, sizeof(buffer), GetCachedConfigPath())) {
        LOG_WARNING("Hot reload ignored ANIMATION_PATH because the config value is too long");
        return 0;
    }
    ApplyAnimationPathValueNoPersist(buffer);
    return 0;
}
