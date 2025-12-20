/**
 * @file config_recovery.c
 * @brief Configuration validation and automatic recovery implementation
 */

#include "config/config_recovery.h"
#include "config/config_defaults.h"
#include "config.h"
#include "log.h"
#include "window.h"
#include "color/gradient.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

BOOL ValidateFontConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    /* Validate font path - check for truncation or invalid path */
    if (strlen(snapshot->fontFileName) >= sizeof(snapshot->fontFileName) - 1) {
        LOG_WARNING("Font path appears truncated, resetting to default");
        strncpy(snapshot->fontFileName, FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                sizeof(snapshot->fontFileName) - 1);
        snapshot->fontFileName[sizeof(snapshot->fontFileName) - 1] = '\0';
        modified = TRUE;
    }
    
    /* Check if font path has valid extension */
    const char* fontExt = strrchr(snapshot->fontFileName, '.');
    if (!fontExt || (strcasecmp(fontExt, ".ttf") != 0 && 
                     strcasecmp(fontExt, ".otf") != 0 &&
                     strcasecmp(fontExt, ".ttc") != 0)) {
        LOG_WARNING("Font path missing valid extension, resetting to default");
        strncpy(snapshot->fontFileName, FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                sizeof(snapshot->fontFileName) - 1);
        snapshot->fontFileName[sizeof(snapshot->fontFileName) - 1] = '\0';
        modified = TRUE;
    }
    
    /* Validate font size */
    if (snapshot->baseFontSize < 8 || snapshot->baseFontSize > 500) {
        LOG_WARNING("Invalid font size %d, resetting to default %d",
                   snapshot->baseFontSize, DEFAULT_FONT_SIZE);
        snapshot->baseFontSize = DEFAULT_FONT_SIZE;
        modified = TRUE;
    }
    
    return modified;
}

BOOL ValidateColorConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    /* Check if it's a valid gradient name or custom gradient format */
    GradientType gradType = GetGradientTypeByName(snapshot->textColor);
    if (gradType != GRADIENT_NONE) {
        return modified; /* Valid gradient name or custom gradient, no modification needed */
    }

    /* Validate text color format (must be #RRGGBB) */
    if (snapshot->textColor[0] != '#' || strlen(snapshot->textColor) != 7) {
        LOG_WARNING("Invalid text color format '%s', resetting to default",
                   snapshot->textColor);
        strncpy(snapshot->textColor, DEFAULT_TEXT_COLOR,
                sizeof(snapshot->textColor) - 1);
        snapshot->textColor[sizeof(snapshot->textColor) - 1] = '\0';
        modified = TRUE;
    } else {
        /* Validate hex digits */
        BOOL validHex = TRUE;
        for (int i = 1; i < 7; i++) {
            char c = snapshot->textColor[i];
            if (!((c >= '0' && c <= '9') || 
                  (c >= 'A' && c <= 'F') || 
                  (c >= 'a' && c <= 'f'))) {
                validHex = FALSE;
                break;
            }
        }
        if (!validHex) {
            LOG_WARNING("Invalid hex color '%s', resetting to default",
                       snapshot->textColor);
            strncpy(snapshot->textColor, DEFAULT_TEXT_COLOR,
                    sizeof(snapshot->textColor) - 1);
            snapshot->textColor[sizeof(snapshot->textColor) - 1] = '\0';
            modified = TRUE;
        }
    }
    
    /* Avoid pure black which causes transparency issues */
    if (strcasecmp(snapshot->textColor, "#000000") == 0) {
        LOG_INFO("Pure black color detected, adjusting to #000001");
        strncpy(snapshot->textColor, "#000001", sizeof(snapshot->textColor) - 1);
        modified = TRUE;
    }
    
    return modified;
}

BOOL ValidateTimerConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    /* Validate default start time */
    if (snapshot->defaultStartTime < 1 || snapshot->defaultStartTime > 86400) {
        LOG_WARNING("Invalid default start time %d, resetting to %ds (25min)",
                   snapshot->defaultStartTime, DEFAULT_START_TIME_SECONDS);
        snapshot->defaultStartTime = DEFAULT_START_TIME_SECONDS;
        modified = TRUE;
    }
    
    /* Validate time options */
    if (snapshot->timeOptionsCount < 0 || snapshot->timeOptionsCount > MAX_TIME_OPTIONS) {
        LOG_WARNING("Invalid time options count %d, resetting to defaults",
                   snapshot->timeOptionsCount);
        snapshot->timeOptionsCount = DEFAULT_QUICK_COUNTDOWN_COUNT;
        snapshot->timeOptions[0] = DEFAULT_QUICK_COUNTDOWN_1;
        snapshot->timeOptions[1] = DEFAULT_QUICK_COUNTDOWN_2;
        snapshot->timeOptions[2] = DEFAULT_QUICK_COUNTDOWN_3;
        modified = TRUE;
    }
    
    /* Validate startup mode */
    if (strlen(snapshot->startupMode) > 0) {
        if (strcmp(snapshot->startupMode, "COUNTDOWN") != 0 &&
            strcmp(snapshot->startupMode, "COUNT_UP") != 0 &&
            strcmp(snapshot->startupMode, "SHOW_TIME") != 0 &&
            strcmp(snapshot->startupMode, "NO_DISPLAY") != 0 &&
            strcmp(snapshot->startupMode, "POMODORO") != 0) {
            LOG_WARNING("Invalid startup mode '%s', resetting to SHOW_TIME",
                       snapshot->startupMode);
            strncpy(snapshot->startupMode, "SHOW_TIME",
                    sizeof(snapshot->startupMode) - 1);
            snapshot->startupMode[sizeof(snapshot->startupMode) - 1] = '\0';
            modified = TRUE;
        }
    }
    
    return modified;
}

BOOL ValidatePomodoroConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    if (snapshot->pomodoroTimesCount < 0 || snapshot->pomodoroTimesCount > 10) {
        LOG_WARNING("Invalid Pomodoro times count %d, resetting to defaults",
                   snapshot->pomodoroTimesCount);
        snapshot->pomodoroTimesCount = DEFAULT_POMODORO_TIMES_COUNT;
        snapshot->pomodoroTimes[0] = DEFAULT_POMODORO_WORK;
        snapshot->pomodoroTimes[1] = DEFAULT_POMODORO_SHORT_BREAK;
        snapshot->pomodoroTimes[2] = DEFAULT_POMODORO_WORK;
        snapshot->pomodoroTimes[3] = DEFAULT_POMODORO_LONG_BREAK;
        modified = TRUE;
    }
    
    if (snapshot->pomodoroLoopCount < 1) {
        LOG_WARNING("Invalid Pomodoro loop count %d, resetting to %d",
                   snapshot->pomodoroLoopCount, DEFAULT_POMODORO_LOOP_COUNT);
        snapshot->pomodoroLoopCount = DEFAULT_POMODORO_LOOP_COUNT;
        modified = TRUE;
    }
    
    return modified;
}

BOOL ValidateNotificationConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    if (snapshot->notificationTimeoutMs < 0 || snapshot->notificationTimeoutMs > 60000) {
        LOG_WARNING("Invalid notification timeout %d ms, resetting to %dms",
                   snapshot->notificationTimeoutMs, DEFAULT_NOTIFICATION_TIMEOUT_MS);
        snapshot->notificationTimeoutMs = DEFAULT_NOTIFICATION_TIMEOUT_MS;
        modified = TRUE;
    }
    
    if (snapshot->notificationMaxOpacity < MIN_OPACITY) {
        LOG_WARNING("Notification opacity too low (%d), setting to %d",
                   snapshot->notificationMaxOpacity, MIN_OPACITY);
        snapshot->notificationMaxOpacity = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->notificationMaxOpacity > MAX_OPACITY) {
        LOG_WARNING("Notification opacity too high (%d), setting to %d",
                   snapshot->notificationMaxOpacity, MAX_OPACITY);
        snapshot->notificationMaxOpacity = MAX_OPACITY;
        modified = TRUE;
    }

    if (snapshot->notificationSoundVolume < MIN_VOLUME) {
        snapshot->notificationSoundVolume = MIN_VOLUME;
        modified = TRUE;
    }
    if (snapshot->notificationSoundVolume > MAX_VOLUME) {
        snapshot->notificationSoundVolume = MAX_VOLUME;
        modified = TRUE;
    }
    
    return modified;
}

BOOL ValidateWindowConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;
    
    /* Validate window scale */
    if (snapshot->windowScale < MIN_SCALE_FACTOR) {
        LOG_WARNING("Window scale too small (%.2f), setting to %.2f",
                   snapshot->windowScale, MIN_SCALE_FACTOR);
        snapshot->windowScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->windowScale > MAX_SCALE_FACTOR) {
        LOG_WARNING("Window scale too large (%.2f), setting to %.2f",
                   snapshot->windowScale, MAX_SCALE_FACTOR);
        snapshot->windowScale = MAX_SCALE_FACTOR;
        modified = TRUE;
    }
    
    /* Validate plugin scale */
    if (snapshot->pluginScale < MIN_SCALE_FACTOR) {
        LOG_WARNING("Plugin scale too small (%.2f), setting to %.2f",
                   snapshot->pluginScale, MIN_SCALE_FACTOR);
        snapshot->pluginScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->pluginScale > MAX_SCALE_FACTOR) {
        LOG_WARNING("Plugin scale too large (%.2f), setting to %.2f",
                   snapshot->pluginScale, MAX_SCALE_FACTOR);
        snapshot->pluginScale = MAX_SCALE_FACTOR;
        modified = TRUE;
    }
    
    if (snapshot->windowOpacity < 0) {
        snapshot->windowOpacity = 0;
        modified = TRUE;
    }
    if (snapshot->windowOpacity > MAX_OPACITY) {
        snapshot->windowOpacity = MAX_OPACITY;
        modified = TRUE;
    }

    if (snapshot->moveStepSmall < MIN_MOVE_STEP) {
        snapshot->moveStepSmall = MIN_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepSmall > MAX_MOVE_STEP) {
        snapshot->moveStepSmall = MAX_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepLarge < MIN_MOVE_STEP) {
        snapshot->moveStepLarge = MIN_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepLarge > MAX_MOVE_STEP) {
        snapshot->moveStepLarge = MAX_MOVE_STEP;
        modified = TRUE;
    }

    if (snapshot->opacityStepNormal < MIN_OPACITY) {
        snapshot->opacityStepNormal = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->opacityStepNormal > MAX_OPACITY) {
        snapshot->opacityStepNormal = MAX_OPACITY;
        modified = TRUE;
    }

    if (snapshot->opacityStepFast < MIN_OPACITY) {
        snapshot->opacityStepFast = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->opacityStepFast > MAX_OPACITY) {
        snapshot->opacityStepFast = MAX_OPACITY;
        modified = TRUE;
    }
    
    return modified;
}

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path || !*utf8Path) return FALSE;
    
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

BOOL ValidateTimeoutAction(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    
    BOOL modified = FALSE;

    /* One-time actions: prevent these from being persisted in config */
    if (snapshot->timeoutAction == TIMEOUT_ACTION_SHUTDOWN ||
        snapshot->timeoutAction == TIMEOUT_ACTION_RESTART ||
        snapshot->timeoutAction == TIMEOUT_ACTION_SLEEP) {
        LOG_INFO("One-time timeout action detected, resetting to MESSAGE");
        snapshot->timeoutAction = TIMEOUT_ACTION_MESSAGE;
        modified = TRUE;
    }

    /* Validate file paths only for OPEN_FILE action */
    if (snapshot->timeoutAction == TIMEOUT_ACTION_OPEN_FILE) {
        if (strlen(snapshot->timeoutFilePath) == 0 || !FileExistsUtf8(snapshot->timeoutFilePath)) {
            LOG_WARNING("Timeout file path invalid or missing, resetting action to MESSAGE");
            snapshot->timeoutAction = TIMEOUT_ACTION_MESSAGE;
            modified = TRUE;
        }
    }

    /* Validate website URL only for OPEN_WEBSITE action */
    if (snapshot->timeoutAction == TIMEOUT_ACTION_OPEN_WEBSITE) {
        if (wcslen(snapshot->timeoutWebsiteUrl) == 0) {
            LOG_WARNING("Timeout website URL empty, resetting action to MESSAGE");
            snapshot->timeoutAction = TIMEOUT_ACTION_MESSAGE;
            modified = TRUE;
        }
    }
    
    return modified;
}

BOOL ValidateAndRecoverConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;

    BOOL modified = FALSE;
    
    LOG_INFO("Starting configuration validation and recovery");

    /* Validate each category - order matters for dependencies */
    if (ValidateFontConfig(snapshot)) modified = TRUE;
    if (ValidateColorConfig(snapshot)) modified = TRUE;
    if (ValidateTimerConfig(snapshot)) modified = TRUE;
    if (ValidatePomodoroConfig(snapshot)) modified = TRUE;
    if (ValidateNotificationConfig(snapshot)) modified = TRUE;
    if (ValidateWindowConfig(snapshot)) modified = TRUE;
    if (ValidateTimeoutAction(snapshot)) modified = TRUE;

    if (modified) {
        LOG_INFO("Configuration validation completed with auto-corrections applied");
    } else {
        LOG_INFO("Configuration validation completed - all values valid");
    }

    return modified;
}
