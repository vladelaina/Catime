/**
 * @file window_config_handlers.c
 * @brief Configuration reload handlers implementation (simplified)
 * 
 * Handles WM_APP_*_CHANGED messages to reload configuration from INI file.
 * Each handler directly reads and applies configuration changes.
 */

#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_hotkeys.h"
#include "config.h"
#include "timer/timer.h"
#include "window.h"
#include "color/color.h"
#include "color/color_parser.h"
#include "tray/tray_animation_core.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];
extern int time_options[];
extern int time_options_count;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static BOOL LoadAndCompareString(const char* section, const char* key, 
                                  char* target, size_t size, const char* def) {
    char temp[512];
    ReadConfigStr(section, key, def, temp, sizeof(temp));
    if (strcmp(temp, target) != 0) {
        strncpy_s(target, size, temp, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadAndCompareInt(const char* section, const char* key, int* target, int def) {
    int temp = ReadConfigInt(section, key, def);
    if (temp != *target) {
        *target = temp;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadAndCompareBool(const char* section, const char* key, BOOL* target, BOOL def) {
    BOOL temp = ReadConfigBool(section, key, def);
    if (temp != *target) {
        *target = temp;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadAndCompareFloat(const char* section, const char* key, float* target) {
    char buffer[32];
    ReadConfigStr(section, key, "", buffer, sizeof(buffer));
    if (buffer[0]) {
        float temp = (float)atof(buffer);
        if (temp > 0.0f && fabsf(temp - *target) > 0.0001f) {
            *target = temp;
            return TRUE;
        }
    }
    return FALSE;
}

/* ============================================================================
 * Handler Implementations
 * ============================================================================ */

LRESULT HandleAppDisplayChanged(HWND hwnd) {
    BOOL changed = FALSE;
    
    /* Text color */
    changed |= LoadAndCompareString(CFG_SECTION_DISPLAY, CFG_KEY_TEXT_COLOR, 
                                    CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), 
                                    CLOCK_TEXT_COLOR);
    
    /* Font size */
    changed |= LoadAndCompareInt(CFG_SECTION_DISPLAY, CFG_KEY_BASE_FONT_SIZE, 
                                 &CLOCK_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE);
    
    /* Window settings (only if not in edit mode) */
    if (!CLOCK_EDIT_MODE) {
        int configPosX = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_X, CLOCK_WINDOW_POS_X);
        int posY = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_Y, CLOCK_WINDOW_POS_Y);
        int posX = configPosX;
        
        /* Skip position handling for special values (-2, -1) during hot-reload.
         * These values require window size to be finalized first. */
        BOOL skipPositionUpdate = (configPosX == -2 || configPosX == -1);
        if (skipPositionUpdate) {
            CLOCK_WINDOW_POS_Y = posY;
            posX = CLOCK_WINDOW_POS_X;
        }
        
        char scaleStr[16];
        ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_SCALE, "1.62", scaleStr, sizeof(scaleStr));
        float newScale = (float)atof(scaleStr);
        
        char pluginScaleStr[16];
        ReadConfigStr(CFG_SECTION_DISPLAY, "PLUGIN_SCALE", "1.0", pluginScaleStr, sizeof(pluginScaleStr));
        float newPluginScale = (float)atof(pluginScaleStr);
        
        BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
        int newOpacity = ReadConfigInt(CFG_SECTION_DISPLAY, "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY);
        
        BOOL posChanged = !skipPositionUpdate && ((posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y));
        BOOL scaleChanged = (newScale > 0.0f && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f);
        
        extern float CLOCK_FONT_SCALE_FACTOR;
        extern float PLUGIN_FONT_SCALE_FACTOR;
        
        if (scaleChanged) {
            CLOCK_WINDOW_SCALE = newScale;
            CLOCK_FONT_SCALE_FACTOR = newScale;
            changed = TRUE;
        }
        
        if (newPluginScale > 0.0f && fabsf(newPluginScale - PLUGIN_FONT_SCALE_FACTOR) > 0.0001f) {
            PLUGIN_FONT_SCALE_FACTOR = newPluginScale;
            changed = TRUE;
        }
        
        if (posChanged || scaleChanged) {
            SetWindowPos(hwnd, NULL, posX, posY,
                        (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
                        (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
                        SWP_NOZORDER | SWP_NOACTIVATE);
            CLOCK_WINDOW_POS_X = posX;
            CLOCK_WINDOW_POS_Y = posY;
            changed = TRUE;
        }
        
        if (newTopmost != CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmostTransient(hwnd, newTopmost);
            changed = TRUE;
        }
        
        if (newOpacity != CLOCK_WINDOW_OPACITY) {
            if (newOpacity < 0) newOpacity = 0;
            if (newOpacity > 100) newOpacity = 100;
            CLOCK_WINDOW_OPACITY = newOpacity;
            SetBlurBehind(hwnd, TRUE);
            changed = TRUE;
        }
    }
    
    if (changed) {
        ResetTimerWithInterval(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    return 0;
}

LRESULT HandleAppTimerChanged(HWND hwnd) {
    BOOL changed = FALSE;
    
    /* Basic timer settings */
    changed |= LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR, 
                                  &CLOCK_USE_24HOUR, CLOCK_USE_24HOUR);
    changed |= LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS, 
                                  &CLOCK_SHOW_SECONDS, CLOCK_SHOW_SECONDS);
    
    /* Time format */
    char formatBuf[32];
    ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_TIME_FORMAT, "DEFAULT", formatBuf, sizeof(formatBuf));
    TimeFormatType newFormat = TimeFormatType_FromStr(formatBuf);
    if (newFormat != g_AppConfig.display.time_format.format) {
        g_AppConfig.display.time_format.format = newFormat;
        changed = TRUE;
    }
    
    /* Milliseconds display */
    BOOL newShowMs = ReadConfigBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_MILLISECONDS, FALSE);
    if (newShowMs != g_AppConfig.display.time_format.show_milliseconds) {
        g_AppConfig.display.time_format.show_milliseconds = newShowMs;
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
        changed = TRUE;
    }
    
    /* Timeout settings */
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_TEXT, 
                         CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), "0");
    
    /* Timeout action (preserve one-time actions) */
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
        char actionBuf[32];
        ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_ACTION, "MESSAGE", actionBuf, sizeof(actionBuf));
        TimeoutActionType newAction = TimeoutActionType_FromStr(actionBuf);
        if (newAction != CLOCK_TIMEOUT_ACTION) {
            CLOCK_TIMEOUT_ACTION = newAction;
        }
    }
    
    /* Timeout file and website */
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_FILE, 
                         CLOCK_TIMEOUT_FILE_PATH, sizeof(CLOCK_TIMEOUT_FILE_PATH), "");
    
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_WEBSITE, 
                         CLOCK_TIMEOUT_WEBSITE_URL, sizeof(CLOCK_TIMEOUT_WEBSITE_URL), "");
    
    /* Default start time */
    LoadAndCompareInt(CFG_SECTION_TIMER, CFG_KEY_DEFAULT_START_TIME, 
                      &g_AppConfig.timer.default_start_time, g_AppConfig.timer.default_start_time);
    
    /* Time options */
    char optionsBuf[256];
    ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_TIME_OPTIONS, "1500,600,300", optionsBuf, sizeof(optionsBuf));
    int newArr[MAX_TIME_OPTIONS] = {0}, newCnt = 0;
    char* tok = strtok(optionsBuf, ",");
    while (tok && newCnt < MAX_TIME_OPTIONS) {
        while (*tok == ' ') tok++;
        newArr[newCnt++] = atoi(tok);
        tok = strtok(NULL, ",");
    }
    if (newCnt != time_options_count || memcmp(newArr, time_options, newCnt * sizeof(int)) != 0) {
        time_options_count = newCnt;
        memcpy(time_options, newArr, newCnt * sizeof(int));
    }
    
    /* Startup mode */
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_STARTUP_MODE, 
                         CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE), CLOCK_STARTUP_MODE);
    
    if (changed) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    return 0;
}

LRESULT HandleAppPomodoroChanged(HWND hwnd) {
    (void)hwnd;
    
    /* Pomodoro time options */
    char buf[128];
    ReadConfigStr(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_OPTIONS, "1500,300,1500,600", buf, sizeof(buf));
    int tmp[3] = {g_AppConfig.pomodoro.work_time, g_AppConfig.pomodoro.short_break, g_AppConfig.pomodoro.long_break};
    int cnt = 0;
    char* tok = strtok(buf, ",");
    while (tok && cnt < 3) {
        while (*tok == ' ') tok++;
        tmp[cnt++] = atoi(tok);
        tok = strtok(NULL, ",");
    }
    if (cnt > 0) g_AppConfig.pomodoro.work_time = tmp[0];
    if (cnt > 1) g_AppConfig.pomodoro.short_break = tmp[1];
    if (cnt > 2) g_AppConfig.pomodoro.long_break = tmp[2];
    
    /* Loop count */
    int loopCount = ReadConfigInt(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_LOOP_COUNT, 1);
    if (loopCount < 1) loopCount = 1;
    g_AppConfig.pomodoro.loop_count = loopCount;
    
    return 0;
}

LRESULT HandleAppNotificationChanged(HWND hwnd) {
    (void)hwnd;
    
    /* Reload notification settings from INI file for hot-reload support */
    g_AppConfig.notification.display.timeout_ms = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000);
    
    int opacity = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95);
    if (opacity >= 1 && opacity <= 100) {
        g_AppConfig.notification.display.max_opacity = opacity;
    }
    
    char typeBuf[32];
    ReadConfigStr(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", typeBuf, sizeof(typeBuf));
    if (strcmp(typeBuf, "SYSTEM_MODAL") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (strcmp(typeBuf, "OS") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_OS;
    } else {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_CATIME;
    }
    
    g_AppConfig.notification.display.disabled = ReadConfigBool(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE);
    
    g_AppConfig.notification.display.window_x = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", -1);
    g_AppConfig.notification.display.window_y = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", -1);
    g_AppConfig.notification.display.window_width = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", 0);
    g_AppConfig.notification.display.window_height = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", 0);
    
    ReadConfigStr(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE,
                  g_AppConfig.notification.messages.timeout_message,
                  sizeof(g_AppConfig.notification.messages.timeout_message));
    
    char soundBuf[MAX_PATH];
    ReadConfigStr(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", soundBuf, sizeof(soundBuf));
    /* Expand %LOCALAPPDATA% placeholder */
    if (soundBuf[0] != '\0') {
        const char* varToken = "%LOCALAPPDATA%";
        size_t tokenLen = strlen(varToken);
        if (_strnicmp(soundBuf, varToken, tokenLen) == 0) {
            const char* localAppData = getenv("LOCALAPPDATA");
            if (localAppData && localAppData[0] != '\0') {
                char resolved[MAX_PATH] = {0};
                snprintf(resolved, sizeof(resolved), "%s%s", localAppData, soundBuf + tokenLen);
                strncpy(g_AppConfig.notification.sound.sound_file, resolved, MAX_PATH - 1);
            } else {
                strncpy(g_AppConfig.notification.sound.sound_file, soundBuf, MAX_PATH - 1);
            }
        } else {
            strncpy(g_AppConfig.notification.sound.sound_file, soundBuf, MAX_PATH - 1);
        }
        g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    } else {
        g_AppConfig.notification.sound.sound_file[0] = '\0';
    }
    
    int volume = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100);
    if (volume >= 0 && volume <= 100) {
        g_AppConfig.notification.sound.volume = volume;
    }
    
    return 0;
}

LRESULT HandleAppHotkeysChanged(HWND hwnd) {
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

LRESULT HandleAppRecentFilesChanged(HWND hwnd) {
    (void)hwnd;
    
    extern void LoadRecentFiles(void);
    LoadRecentFiles();
    
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    extern char CLOCK_TIMEOUT_FILE_PATH[];
    
    /* Validate current timeout file against recent files */
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE) {
        BOOL match = FALSE;
        for (int i = 0; i < g_AppConfig.recent_files.count; ++i) {
            if (strcmp(g_AppConfig.recent_files.files[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0) {
                match = TRUE;
                break;
            }
        }
        
        if (match) {
            WideString ws = ToWide(CLOCK_TIMEOUT_FILE_PATH);
            if (!ws.valid || GetFileAttributesW(ws.buf) == INVALID_FILE_ATTRIBUTES) {
                match = FALSE;
            }
        }
        
        if (!match && g_AppConfig.recent_files.count > 0) {
            WriteConfigTimeoutFile(g_AppConfig.recent_files.files[0].path);
        }
    }
    
    return 0;
}

LRESULT HandleAppColorsChanged(HWND hwnd) {
    char buffer[2048];
    ReadConfigStr(CFG_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI, buffer, sizeof(buffer));
    
    ClearColorOptions();
    char* tok = strtok(buffer, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        AddColorOption(tok);
        tok = strtok(NULL, ",");
    }
    
    ReadPercentIconColorsConfig();
    TrayAnimation_UpdatePercentIconIfNeeded();
    
    InvalidateRect(hwnd, NULL, TRUE);
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
    char buffer[MAX_PATH];
    ReadConfigStr("Animation", "ANIMATION_PATH", "__logo__", buffer, sizeof(buffer));
    ApplyAnimationPathValueNoPersist(buffer);
    return 0;
}
