/**
 * @file config_applier.c
 * @brief Configuration applier implementation
 */

#include "config/config_applier.h"
#include "config.h"
#include "language.h"
#include "timer/timer.h"
#include "window.h"
#include "font.h"
#include "color/color.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    
    /* Legacy numeric format support */
    if (isdigit(langName[0])) {
        int langValue = atoi(langName);
        if (langValue >= 0 && langValue < APP_LANG_COUNT) {
            return langValue;
        }
    }
    
    return APP_LANG_ENGLISH;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void ApplyGeneralSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    FONT_LICENSE_ACCEPTED = snapshot->fontLicenseAccepted;
    strncpy(FONT_LICENSE_VERSION_ACCEPTED, snapshot->fontLicenseVersion,
           sizeof(FONT_LICENSE_VERSION_ACCEPTED) - 1);
    FONT_LICENSE_VERSION_ACCEPTED[sizeof(FONT_LICENSE_VERSION_ACCEPTED) - 1] = '\0';
}

void ApplyDisplaySettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    /* Colors and fonts */
    strncpy(CLOCK_TEXT_COLOR, snapshot->textColor, sizeof(CLOCK_TEXT_COLOR) - 1);
    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
    
    CLOCK_BASE_FONT_SIZE = snapshot->baseFontSize;
    
    strncpy(FONT_FILE_NAME, snapshot->fontFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, snapshot->fontInternalName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    /* Window position and scale */
    CLOCK_WINDOW_POS_X = snapshot->windowPosX;
    CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
    CLOCK_WINDOW_SCALE = snapshot->windowScale;
    CLOCK_FONT_SCALE_FACTOR = snapshot->windowScale;
    CLOCK_WINDOW_TOPMOST = snapshot->windowTopmost;
    
    /* Update window position if window exists */
    HWND hwnd = FindWindowW(L"CatimeWindow", L"Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void ApplyTimerSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    CLOCK_DEFAULT_START_TIME = snapshot->defaultStartTime;
    CLOCK_TOTAL_TIME = snapshot->defaultStartTime;
    CLOCK_USE_24HOUR = snapshot->use24Hour;
    CLOCK_SHOW_SECONDS = snapshot->showSeconds;
    CLOCK_TIME_FORMAT = snapshot->timeFormat;
    CLOCK_SHOW_MILLISECONDS = snapshot->showMilliseconds;
    
    /* Timeout action */
    CLOCK_TIMEOUT_ACTION = snapshot->timeoutAction;
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
    
    POMODORO_TIMES_COUNT = snapshot->pomodoroTimesCount;
    for (int i = 0; i < snapshot->pomodoroTimesCount && i < 10; i++) {
        POMODORO_TIMES[i] = snapshot->pomodoroTimes[i];
    }
    
    if (POMODORO_TIMES_COUNT > 0) {
        POMODORO_WORK_TIME = POMODORO_TIMES[0];
        if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
        if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[3];
    }
    
    POMODORO_LOOP_COUNT = snapshot->pomodoroLoopCount;
}

void ApplyNotificationSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, snapshot->timeoutMessage,
           sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
    CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, snapshot->pomodoroMessage,
           sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
    POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_CYCLE_COMPLETE_TEXT, snapshot->cycleCompleteMessage,
           sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
    POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
    
    NOTIFICATION_TIMEOUT_MS = snapshot->notificationTimeoutMs;
    NOTIFICATION_MAX_OPACITY = snapshot->notificationMaxOpacity;
    NOTIFICATION_TYPE = snapshot->notificationType;
    NOTIFICATION_DISABLED = snapshot->notificationDisabled;
    
    strncpy(NOTIFICATION_SOUND_FILE, snapshot->notificationSoundFile, MAX_PATH - 1);
    NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
    
    NOTIFICATION_SOUND_VOLUME = snapshot->notificationSoundVolume;
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
    char colorOptionsCopy[1024];
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
    
    CLOCK_RECENT_FILES_COUNT = snapshot->recentFilesCount;
    for (int i = 0; i < snapshot->recentFilesCount && i < MAX_RECENT_FILES; i++) {
        memcpy(&CLOCK_RECENT_FILES[i], &snapshot->recentFiles[i], sizeof(RecentFile));
    }
    
    /* Note: Recent files menu is dynamically built when tray menu is shown,
       no need to update it here */
}

void ApplyConfigSnapshot(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    /* Apply in logical order */
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
    SetLanguage((AppLanguage)languageEnum);
    
    /* Load animation speed settings */
    ReloadAnimationSpeedFromConfig();
    
    /* Update timestamp for config reload detection */
    last_config_time = time(NULL);
}

