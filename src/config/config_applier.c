/**
 * @file config_applier.c
 * @brief Configuration applier implementation
 */

#include "config/config_applier.h"
#include "config.h"
#include "config/config_defaults.h"
#include "config/config_plugin_security.h"
#include "language.h"
#include "window.h"
#include "font.h"
#include "color/color.h"
#include "drawing/drawing_effect.h"
#include "text_effect.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

/* Global text effect */
extern TextEffectType CLOCK_TEXT_EFFECT;

/* Force apply flag - used during reset to bypass position preservation */
BOOL g_ForceApplyConfig = FALSE;

/* ============================================================================
 * Helper: Language enum mapping
 * ============================================================================ */

static int LanguageNameToEnum(const char* langName) {
    if (!langName) return APP_LANG_ENGLISH;
    return GetLanguageFromConfigKey(langName);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

static void ApplyDefaultPomodoroTimes(void) {
    static const int defaultTimes[] = {
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_SHORT_BREAK,
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_LONG_BREAK
    };

    int count = (int)_countof(defaultTimes);
    if (count > (int)_countof(g_AppConfig.pomodoro.times)) {
        count = (int)_countof(g_AppConfig.pomodoro.times);
    }

    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    memcpy(g_AppConfig.pomodoro.times, defaultTimes, (size_t)count * sizeof(defaultTimes[0]));
    g_AppConfig.pomodoro.times_count = count;
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
}

static int ClampPomodoroLoopCountForApply(int loopCount) {
    if (loopCount < MIN_POMODORO_LOOP_COUNT) {
        return MIN_POMODORO_LOOP_COUNT;
    }
    if (loopCount > MAX_POMODORO_LOOP_COUNT) {
        return MAX_POMODORO_LOOP_COUNT;
    }
    return loopCount;
}

static void ApplyDefaultQuickCountdownOptions(void) {
    static const int defaultOptions[] = {
        DEFAULT_QUICK_COUNTDOWN_1,
        DEFAULT_QUICK_COUNTDOWN_2,
        DEFAULT_QUICK_COUNTDOWN_3
    };

    int count = (int)_countof(defaultOptions);
    if (count > MAX_TIME_OPTIONS) {
        count = MAX_TIME_OPTIONS;
    }

    ZeroMemory(time_options, sizeof(time_options));
    for (int i = 0; i < count; i++) {
        time_options[i] = defaultOptions[i];
    }
    time_options_count = count;
}

static void ApplyQuickCountdownOptions(const ConfigSnapshot* snapshot) {
    int optionsCount = snapshot->timeOptionsCount;
    if (optionsCount <= 0 || optionsCount > MAX_TIME_OPTIONS) {
        LOG_WARNING("Invalid quick countdown options count %d while applying config, using defaults",
                    optionsCount);
        ApplyDefaultQuickCountdownOptions();
        return;
    }

    for (int i = 0; i < optionsCount; i++) {
        if (snapshot->timeOptions[i] <= 0 ||
            snapshot->timeOptions[i] > MAX_TIME_OPTION_SECONDS) {
            LOG_WARNING("Invalid quick countdown preset %d while applying config, using defaults",
                        snapshot->timeOptions[i]);
            ApplyDefaultQuickCountdownOptions();
            return;
        }
    }

    ZeroMemory(time_options, sizeof(time_options));
    for (int i = 0; i < optionsCount; i++) {
        time_options[i] = snapshot->timeOptions[i];
    }
    time_options_count = optionsCount;
}

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
    
    strncpy(FONT_FILE_NAME, snapshot->fontFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    strncpy(FONT_RUNTIME_FILE_NAME, snapshot->fontFileName,
            sizeof(FONT_RUNTIME_FILE_NAME) - 1);
    FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, snapshot->fontInternalName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    /* Apply non-position settings first */
    CLOCK_WINDOW_SCALE = snapshot->windowScale;
    CLOCK_FONT_SCALE_FACTOR = snapshot->windowScale;
    PLUGIN_FONT_SCALE_FACTOR = snapshot->pluginScale;
    CLOCK_WINDOW_TOPMOST = snapshot->windowTopmost;
    CLOCK_WINDOW_EFFECTIVE_TOPMOST = snapshot->windowTopmost;
    CLOCK_WINDOW_OPACITY = snapshot->windowOpacity;

    g_AppConfig.display.move_step_small = snapshot->moveStepSmall;
    g_AppConfig.display.move_step_large = snapshot->moveStepLarge;
    g_AppConfig.display.opacity_step_normal = snapshot->opacityStepNormal;
    g_AppConfig.display.opacity_step_fast = snapshot->opacityStepFast;
    g_AppConfig.display.scale_step_normal = snapshot->scaleStepNormal;
    g_AppConfig.display.scale_step_fast = snapshot->scaleStepFast;
    TextEffectType previousTextEffect = CLOCK_TEXT_EFFECT;
    CLOCK_TEXT_EFFECT = (TextEffectType)snapshot->textEffect;
    g_AppConfig.display.text_effect = snapshot->textEffect;

    if (TextEffect_UsesSharedEffectBuffer(previousTextEffect) &&
        !TextEffect_UsesSharedEffectBuffer(CLOCK_TEXT_EFFECT)) {
        CleanupDrawingEffects();
    }

    HWND hwnd = FindCurrentProcessMainWindow();
    if (hwnd) {
        if (CLOCK_IS_DRAGGING) {
            RECT currentRect;
            if (GetWindowRect(hwnd, &currentRect)) {
                CLOCK_WINDOW_POS_X = currentRect.left;
                CLOCK_WINDOW_POS_Y = currentRect.top;
            }
        } else if (g_ForceApplyConfig) {
            /* Force apply mode: always use config values (used during reset) */
            CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
            
            /* For special position values (-2, -1), don't set position here.
             * RecalculateWindowSize will handle it with correct window dimensions. */
            if (snapshot->windowPosX != -2 && snapshot->windowPosX != -1) {
                CLOCK_WINDOW_POS_X = snapshot->windowPosX;
                SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                            0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        } else {
            /* Normal mode: preserve position if significantly different */
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);
            long long deltaX = llabs((long long)currentRect.left -
                                     snapshot->windowPosX);
            long long deltaY = llabs((long long)currentRect.top -
                                     snapshot->windowPosY);
            
            if (deltaX > 10 || deltaY > 10) {
                CLOCK_WINDOW_POS_X = currentRect.left;
                CLOCK_WINDOW_POS_Y = currentRect.top;
            } else {
                CLOCK_WINDOW_POS_X = snapshot->windowPosX;
                CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
                SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                            0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }

        BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), alphaValue, LWA_COLORKEY | LWA_ALPHA);
        if (!CLOCK_IS_DRAGGING) {
            RefreshWindowTopmostState(hwnd);
        }

        if (!CLOCK_IS_DRAGGING) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
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
    
    strncpy(CLOCK_TIMEOUT_WEBSITE_URL, snapshot->timeoutWebsiteUrl, MAX_PATH - 1);
    CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = '\0';
    
    ApplyQuickCountdownOptions(snapshot);
    
    /* Startup mode */
    strncpy(CLOCK_STARTUP_MODE, snapshot->startupMode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
}

void ApplyPomodoroSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;

    int loopCount = ClampPomodoroLoopCountForApply(snapshot->pomodoroLoopCount);
    int timesCount = snapshot->pomodoroTimesCount;
    if (timesCount <= 0 || timesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        LOG_WARNING("Invalid Pomodoro times count %d while applying config, using defaults",
                    timesCount);
        ApplyDefaultPomodoroTimes();
        g_AppConfig.pomodoro.loop_count = loopCount;
        return;
    }

    for (int i = 0; i < timesCount; i++) {
        if (snapshot->pomodoroTimes[i] <= 0 ||
            snapshot->pomodoroTimes[i] > MAX_POMODORO_OPTION_SECONDS) {
            LOG_WARNING("Invalid Pomodoro interval %d while applying config, using defaults",
                        snapshot->pomodoroTimes[i]);
            ApplyDefaultPomodoroTimes();
            g_AppConfig.pomodoro.loop_count = loopCount;
            return;
        }
    }

    g_AppConfig.pomodoro.times_count = timesCount;
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    for (int i = 0; i < timesCount; i++) {
        g_AppConfig.pomodoro.times[i] = snapshot->pomodoroTimes[i];
    }
    
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    if (timesCount > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    if (timesCount > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    
    g_AppConfig.pomodoro.loop_count = loopCount;
}

void ApplyNotificationSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    strncpy(g_AppConfig.notification.messages.timeout_message, snapshot->timeoutMessage,
           sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    
    g_AppConfig.notification.display.timeout_ms = snapshot->notificationTimeoutMs;
    g_AppConfig.notification.display.max_opacity = snapshot->notificationMaxOpacity;
    g_AppConfig.notification.display.corner_radius = snapshot->notificationCornerRadius;
    g_AppConfig.notification.display.font_size = snapshot->notificationFontSize;
    g_AppConfig.notification.display.type = snapshot->notificationType;
    g_AppConfig.notification.display.disabled = snapshot->notificationDisabled;
    g_AppConfig.notification.display.window_x = snapshot->notificationWindowX;
    g_AppConfig.notification.display.window_y = snapshot->notificationWindowY;
    g_AppConfig.notification.display.window_width = snapshot->notificationWindowWidth;
    g_AppConfig.notification.display.window_height = snapshot->notificationWindowHeight;
    
    char resolvedSoundPath[MAX_PATH] = {0};
    const char* soundPathToApply = snapshot->notificationSoundFile;
    if (ExpandEffectiveLocalAppDataPath(snapshot->notificationSoundFile,
                                        resolvedSoundPath,
                                        sizeof(resolvedSoundPath))) {
        soundPathToApply = resolvedSoundPath;
    }
    strncpy(g_AppConfig.notification.sound.sound_file, soundPathToApply, MAX_PATH - 1);
    g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    
    g_AppConfig.notification.sound.volume = snapshot->notificationSoundVolume;
}

void ApplyColorSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;

    if (!ReplaceColorOptionsFromConfigValue(snapshot->colorOptions) &&
        !ReplaceColorOptionsFromConfigValue(DEFAULT_COLOR_OPTIONS_INI)) {
        LOG_WARNING("Failed to apply color options from config and defaults");
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

    int recentFilesCount = snapshot->recentFilesCount;
    if (recentFilesCount < 0) {
        recentFilesCount = 0;
    }
    if (recentFilesCount > MAX_RECENT_FILES) {
        recentFilesCount = MAX_RECENT_FILES;
    }

    ZeroMemory(g_AppConfig.recent_files.files, sizeof(g_AppConfig.recent_files.files));
    g_AppConfig.recent_files.count = recentFilesCount;
    for (int i = 0; i < recentFilesCount; i++) {
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
}
