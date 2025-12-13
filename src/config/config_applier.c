/**
 * @file config_applier.c
 * @brief Configuration applier implementation
 */

#include "config/config_applier.h"
#include "config.h"
#include "config/config_plugin_security.h"
#include "language.h"
#include "timer/timer.h"
#include "window.h"
#include "font.h"
#include "color/color.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Global text effect */
extern TextEffectType CLOCK_TEXT_EFFECT;

/* ============================================================================
 * Helper: Language enum mapping
 * ============================================================================ */

static int LanguageNameToEnum(const char* langName) {
    if (!langName) return APP_LANG_ENGLISH;
    
    if (strcmp(langName, "Chinese_Simplified") == 0) return APP_LANG_CHINESE_SIMP;
    if (strcmp(langName, "Chinese_Traditional") == 0) return APP_LANG_CHINESE_TRAD;
    if (strcmp(langName, "English") == 0) return APP_LANG_ENGLISH;
    if (strcmp(langName, "Spanish") == 0) return APP_LANG_SPANISH;
    if (strcmp(langName, "French") == 0) return APP_LANG_FRENCH;
    if (strcmp(langName, "German") == 0) return APP_LANG_GERMAN;
    if (strcmp(langName, "Russian") == 0) return APP_LANG_RUSSIAN;
    if (strcmp(langName, "Portuguese") == 0) return APP_LANG_PORTUGUESE;
    if (strcmp(langName, "Japanese") == 0) return APP_LANG_JAPANESE;
    if (strcmp(langName, "Korean") == 0) return APP_LANG_KOREAN;
    
    return APP_LANG_ENGLISH;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void ApplyGeneralSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    g_AppConfig.font_license.accepted = snapshot->fontLicenseAccepted;
    strncpy(g_AppConfig.font_license.version_accepted, snapshot->fontLicenseVersion,
           sizeof(g_AppConfig.font_license.version_accepted) - 1);
    g_AppConfig.font_license.version_accepted[sizeof(g_AppConfig.font_license.version_accepted) - 1] = '\0';
    
    /* Load plugin trust state */
    LoadPluginTrustFromConfig();
}

void ApplyDisplaySettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    /* Colors and fonts */
    strncpy(CLOCK_TEXT_COLOR, snapshot->textColor, sizeof(CLOCK_TEXT_COLOR) - 1);
    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
    
    CLOCK_BASE_FONT_SIZE = snapshot->baseFontSize;
    
    LOG_INFO("ApplyDisplaySettings: Setting FONT_FILE_NAME from snapshot");
    LOG_INFO("ApplyDisplaySettings:   From snapshot: '%s'", snapshot->fontFileName);
    strncpy(FONT_FILE_NAME, snapshot->fontFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    LOG_INFO("ApplyDisplaySettings:   Now FONT_FILE_NAME = '%s'", FONT_FILE_NAME);
    
    strncpy(FONT_INTERNAL_NAME, snapshot->fontInternalName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    LOG_INFO("ApplyDisplaySettings:   FONT_INTERNAL_NAME = '%s'", FONT_INTERNAL_NAME);
    
    /* Update font cache to mark correct current font */
    extern void FontCache_UpdateCurrent(const char* fontRelativePath);
    char relativePath[MAX_PATH];
    const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
    size_t prefixLen = strlen(prefix);
    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        strncpy(relativePath, FONT_FILE_NAME + prefixLen, sizeof(relativePath) - 1);
        relativePath[sizeof(relativePath) - 1] = '\0';
        FontCache_UpdateCurrent(relativePath);
        LOG_INFO("ApplyDisplaySettings:   Updated font cache with relative path: '%s'", relativePath);
    } else {
        /* System font - clear current font marker in cache */
        FontCache_UpdateCurrent("");
        LOG_INFO("ApplyDisplaySettings:   Cleared font cache marker (system font)");
    }
    
    /* Apply non-position settings first */
    CLOCK_WINDOW_SCALE = snapshot->windowScale;
    CLOCK_FONT_SCALE_FACTOR = snapshot->windowScale;
    PLUGIN_FONT_SCALE_FACTOR = snapshot->pluginScale;
    CLOCK_WINDOW_TOPMOST = snapshot->windowTopmost;
    CLOCK_WINDOW_OPACITY = snapshot->windowOpacity;

    g_AppConfig.display.move_step_small = snapshot->moveStepSmall;
    g_AppConfig.display.move_step_large = snapshot->moveStepLarge;
    g_AppConfig.display.opacity_step_normal = snapshot->opacityStepNormal;
    g_AppConfig.display.opacity_step_fast = snapshot->opacityStepFast;
    g_AppConfig.display.scale_step_normal = snapshot->scaleStepNormal;
    g_AppConfig.display.scale_step_fast = snapshot->scaleStepFast;
    CLOCK_TEXT_EFFECT = (TextEffectType)snapshot->textEffect;
    g_AppConfig.display.text_effect = snapshot->textEffect;

    HWND hwnd = FindWindowW(L"CatimeWindowClass", L"Catime");
    if (hwnd) {
        /* Get current window position before updating globals */
        RECT currentRect;
        GetWindowRect(hwnd, &currentRect);
        
        /* Compare current position with config snapshot */
        int deltaX = abs(currentRect.left - snapshot->windowPosX);
        int deltaY = abs(currentRect.top - snapshot->windowPosY);
        
        if (deltaX > 10 || deltaY > 10) {
            /* Significant difference: preserve current position (likely just saved) */
            CLOCK_WINDOW_POS_X = currentRect.left;
            CLOCK_WINDOW_POS_Y = currentRect.top;
        } else {
            /* Minor difference: apply config position */
            CLOCK_WINDOW_POS_X = snapshot->windowPosX;
            CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
            SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), alphaValue, LWA_COLORKEY | LWA_ALPHA);

        /* Ensure animation timer is running if effects are active */
        /* Use adaptive interval based on window size to prevent mouse lag */
        if (CLOCK_LIQUID_EFFECT || CLOCK_HOLOGRAPHIC_EFFECT || 
            CLOCK_NEON_EFFECT || CLOCK_GLOW_EFFECT || CLOCK_GLASS_EFFECT) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int pixels = rect.right * rect.bottom;
            
            /* Holographic effect needs more aggressive throttling */
            UINT interval;
            if (CLOCK_HOLOGRAPHIC_EFFECT) {
                interval = (pixels < 30000) ? 50 : 
                           (pixels < 100000) ? 80 : 
                           (pixels < 300000) ? 120 : 200;
            } else {
                interval = (pixels < 50000) ? 33 : 
                           (pixels < 200000) ? 50 : 
                           (pixels < 500000) ? 80 : 120;
            }
            SetTimer(hwnd, TIMER_ID_RENDER_ANIMATION, interval, NULL); 
        } else {
            KillTimer(hwnd, TIMER_ID_RENDER_ANIMATION);
        }

        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        /* No window: just update global variables */
        CLOCK_WINDOW_POS_X = snapshot->windowPosX;
        CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
    }
}

void ApplyTimerSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    g_AppConfig.timer.default_start_time = snapshot->defaultStartTime;
    CLOCK_TOTAL_TIME = snapshot->defaultStartTime;
    CLOCK_USE_24HOUR = snapshot->use24Hour;
    CLOCK_SHOW_SECONDS = snapshot->showSeconds;
    g_AppConfig.display.time_format.format = snapshot->timeFormat;
    g_AppConfig.display.time_format.show_milliseconds = snapshot->showMilliseconds;
    
    /* Timeout action - preserve one-time actions in memory */
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
        CLOCK_TIMEOUT_ACTION = snapshot->timeoutAction;
    }
    strncpy(CLOCK_TIMEOUT_TEXT, snapshot->timeoutText, sizeof(CLOCK_TIMEOUT_TEXT) - 1);
    CLOCK_TIMEOUT_TEXT[sizeof(CLOCK_TIMEOUT_TEXT) - 1] = '\0';
    
    strncpy(CLOCK_TIMEOUT_FILE_PATH, snapshot->timeoutFilePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    
    wcsncpy(CLOCK_TIMEOUT_WEBSITE_URL, snapshot->timeoutWebsiteUrl, MAX_PATH - 1);
    CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = L'\0';
    
    /* Quick countdown presets */
    time_options_count = snapshot->timeOptionsCount;
    for (int i = 0; i < snapshot->timeOptionsCount && i < MAX_TIME_OPTIONS; i++) {
        time_options[i] = snapshot->timeOptions[i];
    }
    
    /* Startup mode */
    strncpy(CLOCK_STARTUP_MODE, snapshot->startupMode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
}

void ApplyPomodoroSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    g_AppConfig.pomodoro.times_count = snapshot->pomodoroTimesCount;
    for (int i = 0; i < snapshot->pomodoroTimesCount && i < 10; i++) {
        g_AppConfig.pomodoro.times[i] = snapshot->pomodoroTimes[i];
    }
    
    if (g_AppConfig.pomodoro.times_count > 0) {
        g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
        if (g_AppConfig.pomodoro.times_count > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
        if (g_AppConfig.pomodoro.times_count > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    }
    
    g_AppConfig.pomodoro.loop_count = snapshot->pomodoroLoopCount;
}

void ApplyNotificationSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    strncpy(g_AppConfig.notification.messages.timeout_message, snapshot->timeoutMessage,
           sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    
    g_AppConfig.notification.display.timeout_ms = snapshot->notificationTimeoutMs;
    g_AppConfig.notification.display.max_opacity = snapshot->notificationMaxOpacity;
    g_AppConfig.notification.display.type = snapshot->notificationType;
    g_AppConfig.notification.display.disabled = snapshot->notificationDisabled;
    g_AppConfig.notification.display.window_x = snapshot->notificationWindowX;
    g_AppConfig.notification.display.window_y = snapshot->notificationWindowY;
    g_AppConfig.notification.display.window_width = snapshot->notificationWindowWidth;
    g_AppConfig.notification.display.window_height = snapshot->notificationWindowHeight;
    
    /* Copy sound file path and expand %LOCALAPPDATA% placeholder if needed */
    strncpy(g_AppConfig.notification.sound.sound_file, snapshot->notificationSoundFile, MAX_PATH - 1);
    g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    
    /* Normalize %LOCALAPPDATA% placeholder to absolute path */
    if (g_AppConfig.notification.sound.sound_file[0] != '\0') {
        const char* varToken = "%LOCALAPPDATA%";
        size_t tokenLen = strlen(varToken);
        if (_strnicmp(g_AppConfig.notification.sound.sound_file, varToken, tokenLen) == 0) {
            const char* localAppData = getenv("LOCALAPPDATA");
            if (localAppData && localAppData[0] != '\0') {
                char resolved[MAX_PATH] = {0};
                snprintf(resolved, sizeof(resolved), "%s%s",
                         localAppData,
                         g_AppConfig.notification.sound.sound_file + tokenLen);
                strncpy(g_AppConfig.notification.sound.sound_file, resolved, MAX_PATH - 1);
                g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
            }
        }
    }
    
    g_AppConfig.notification.sound.volume = snapshot->notificationSoundVolume;
}

void ApplyColorSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    /* Free existing color options */
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (COLOR_OPTIONS && COLOR_OPTIONS[i].hexColor) {
            free(COLOR_OPTIONS[i].hexColor);
        }
    }
    if (COLOR_OPTIONS) {
        free(COLOR_OPTIONS);
        COLOR_OPTIONS = NULL;
    }
    COLOR_OPTIONS_COUNT = 0;
    
    /* Parse new color options */
    char colorOptionsCopy[2048];
    strncpy(colorOptionsCopy, snapshot->colorOptions, sizeof(colorOptionsCopy) - 1);
    colorOptionsCopy[sizeof(colorOptionsCopy) - 1] = '\0';
    
    char* token = strtok(colorOptionsCopy, ",");
    while (token) {
        COLOR_OPTIONS = realloc(COLOR_OPTIONS, sizeof(PredefinedColor) * (COLOR_OPTIONS_COUNT + 1));
        if (COLOR_OPTIONS) {
            COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = strdup(token);
            COLOR_OPTIONS_COUNT++;
        }
        token = strtok(NULL, ",");
    }
}

void ApplyHotkeySettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    /* Note: Actual hotkey registration/unregistration is handled by hotkey module */
    /* This just updates the global variables */
    
    /* Hotkeys are typically stored in the hotkey module's own state,
       not in global variables. This function would need to call hotkey
       registration functions if that's required. For now, we assume
       the hotkeys are applied elsewhere when needed. */
}

void ApplyRecentFilesSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    g_AppConfig.recent_files.count = snapshot->recentFilesCount;
    for (int i = 0; i < snapshot->recentFilesCount && i < MAX_RECENT_FILES; i++) {
        memcpy(&g_AppConfig.recent_files.files[i], &snapshot->recentFiles[i], sizeof(RecentFile));
    }
    
    /* Note: Recent files menu is dynamically built when tray menu is shown,
       no need to update it here */
}

void ApplyConfigSnapshot(const ConfigSnapshot* snapshot) {
    if (!snapshot) {
        LOG_WARNING("ApplyConfigSnapshot called with NULL snapshot");
        return;
    }
    
    LOG_INFO("Applying configuration snapshot");
    
    /* Apply in logical order - validation has already been done */
    ApplyGeneralSettings(snapshot);
    ApplyDisplaySettings(snapshot);
    ApplyTimerSettings(snapshot);
    ApplyPomodoroSettings(snapshot);
    ApplyNotificationSettings(snapshot);
    ApplyColorSettings(snapshot);
    ApplyHotkeySettings(snapshot);
    ApplyRecentFilesSettings(snapshot);
    
    /* Apply language (triggers UI update) */
    int languageEnum = LanguageNameToEnum(snapshot->language);
    if (languageEnum < 0 || languageEnum >= APP_LANG_COUNT) {
        LOG_WARNING("Invalid language enum %d, using English", languageEnum);
        languageEnum = APP_LANG_ENGLISH;
    }
    SetLanguage((AppLanguage)languageEnum);
    
    /* Load animation speed settings */
    ReloadAnimationSpeedFromConfig();
    
    /* Update timestamp for config reload detection */
    g_AppConfig.last_config_time = time(NULL);
    
    LOG_INFO("Configuration snapshot applied successfully");
}

