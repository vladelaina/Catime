/**
 * @file window_config_handlers.c
 * @brief Configuration reload handlers implementation
 */

#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_hotkeys.h"
#include "config.h"
#include "timer/timer.h"
#include "window.h"
#include "color/color.h"
#include "tray/tray_animation_core.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

extern char CLOCK_TEXT_COLOR[10];
extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

/* ============================================================================
 * Configuration Reload Framework
 * ============================================================================ */

typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_CUSTOM
} ConfigItemType;

typedef struct {
    ConfigItemType type;
    const char* section;
    const char* key;
    void* target;
    size_t targetSize;
    const void* defaultValue;
    BOOL (*customLoader)(const char* section, const char* key, void* target, const void* def);
    BOOL triggerRedraw;
} ConfigItem;

static BOOL LoadConfigString(const char* section, const char* key, void* target, size_t size, const char* def) {
    char temp[512];
    ReadConfigStr(section, key, def, temp, sizeof(temp));
    if (strcmp(temp, (char*)target) != 0) {
        strncpy_s(target, size, temp, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadConfigInt(const char* section, const char* key, void* target, int def) {
    int temp = ReadConfigInt(section, key, def);
    if (temp != *(int*)target) {
        *(int*)target = temp;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadConfigBool(const char* section, const char* key, void* target, BOOL def) {
    BOOL temp = ReadConfigBool(section, key, def);
    if (temp != *(BOOL*)target) {
        *(BOOL*)target = temp;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadConfigFloat(const char* section, const char* key, void* target, float def) {
    char buffer[32];
    ReadConfigStr(section, key, "", buffer, sizeof(buffer));
    if (buffer[0]) {
        float temp = (float)atof(buffer);
        if (temp > 0.0f && fabsf(temp - *(float*)target) > 0.0001f) {
            *(float*)target = temp;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL ReloadConfigItems(HWND hwnd, const ConfigItem* items, size_t count) {
    BOOL anyChanged = FALSE;
    BOOL needsRedraw = FALSE;
    
    for (size_t i = 0; i < count; i++) {
        const ConfigItem* item = &items[i];
        BOOL changed = FALSE;
        
        if (item->customLoader) {
            changed = item->customLoader(item->section, item->key, item->target, item->defaultValue);
        } else {
            switch (item->type) {
                case CONFIG_TYPE_STRING:
                    changed = LoadConfigString(item->section, item->key, item->target, 
                                              item->targetSize, (const char*)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_INT:
                    changed = LoadConfigInt(item->section, item->key, item->target, 
                                           (int)(intptr_t)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_BOOL:
                    changed = LoadConfigBool(item->section, item->key, item->target, 
                                            (BOOL)(intptr_t)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_FLOAT:
                    changed = LoadConfigFloat(item->section, item->key, item->target, 
                                             *(float*)&item->defaultValue);
                    break;
                    
                default:
                    break;
            }
        }
        
        if (changed) {
            anyChanged = TRUE;
            if (item->triggerRedraw) {
                needsRedraw = TRUE;
            }
        }
    }
    
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    return anyChanged;
}

#define CFG_STR(sec, key, var, def) \
    {CONFIG_TYPE_STRING, sec, key, (void*)var, sizeof(var), (void*)def, NULL, TRUE}

#define CFG_STR_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_STRING, sec, key, (void*)var, sizeof(var), (void*)def, NULL, FALSE}

#define CFG_INT(sec, key, var, def) \
    {CONFIG_TYPE_INT, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, TRUE}

#define CFG_INT_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_INT, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, FALSE}

#define CFG_BOOL(sec, key, var, def) \
    {CONFIG_TYPE_BOOL, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, TRUE}

#define CFG_BOOL_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_BOOL, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, FALSE}

#define CFG_CUSTOM(sec, key, var, def, loader, redraw) \
    {CONFIG_TYPE_CUSTOM, sec, key, (void*)var, 0, (void*)def, loader, redraw}

/* ============================================================================
 * Custom Loaders
 * ============================================================================ */

static BOOL LoadDisplayWindowSettings(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    if (CLOCK_EDIT_MODE) return FALSE;
    
    int posX = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_X, CLOCK_WINDOW_POS_X);
    int posY = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_Y, CLOCK_WINDOW_POS_Y);
    char scaleStr[16];
    ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_SCALE, "1.62", scaleStr, sizeof(scaleStr));
    float newScale = (float)atof(scaleStr);
    BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
    int newOpacity = ReadConfigInt(CFG_SECTION_DISPLAY, "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY);

    BOOL changed = FALSE;
    BOOL posChanged = (posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y);
    BOOL scaleChanged = (newScale > 0.0f && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f);
    BOOL opacityChanged = (newOpacity != CLOCK_WINDOW_OPACITY);
    
    if (scaleChanged) {
        extern float CLOCK_FONT_SCALE_FACTOR;
        CLOCK_WINDOW_SCALE = newScale;
        CLOCK_FONT_SCALE_FACTOR = newScale;
        changed = TRUE;
    }
    
    if (posChanged || scaleChanged) {
        HWND hwnd = *(HWND*)target;
        SetWindowPos(hwnd, NULL, posX, posY,
                    (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
                    (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
                    SWP_NOZORDER | SWP_NOACTIVATE);
        CLOCK_WINDOW_POS_X = posX;
        CLOCK_WINDOW_POS_Y = posY;
        changed = TRUE;
    }
    
    if (newTopmost != CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(*(HWND*)target, newTopmost);
        changed = TRUE;
    }

    if (opacityChanged) {
        if (newOpacity < 1) newOpacity = 1;
        if (newOpacity > 100) newOpacity = 100;
        CLOCK_WINDOW_OPACITY = newOpacity;

        HWND hwnd = *(HWND*)target;
        BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);

        /* Use appropriate flags based on edit mode */
        if (CLOCK_EDIT_MODE) {
            SetLayeredWindowAttributes(hwnd, 0, alphaValue, LWA_ALPHA);
        } else {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), alphaValue, LWA_COLORKEY | LWA_ALPHA);
        }

        changed = TRUE;
    }

    return changed;
}

static const char* TimeFormatTypeToStr(TimeFormatType val) { 
    switch(val) {
        case TIME_FORMAT_DEFAULT: return "DEFAULT";
        case TIME_FORMAT_ZERO_PADDED: return "ZERO_PADDED";
        case TIME_FORMAT_FULL_PADDED: return "FULL_PADDED";
        default: return STR_DEFAULT;
    }
}

static TimeFormatType TimeFormatTypeFromStr(const char* str) { 
    if (!str) return TIME_FORMAT_DEFAULT;
    if (strcmp(str, "DEFAULT") == 0) return TIME_FORMAT_DEFAULT;
    if (strcmp(str, "ZERO_PADDED") == 0) return TIME_FORMAT_ZERO_PADDED;
    if (strcmp(str, "FULL_PADDED") == 0) return TIME_FORMAT_FULL_PADDED;
    return TIME_FORMAT_DEFAULT;
}

static const char* TimeoutActionTypeToStr(TimeoutActionType val) {
    switch(val) {
        case TIMEOUT_ACTION_MESSAGE: return STR_MESSAGE;
        case TIMEOUT_ACTION_LOCK: return "LOCK";
        case TIMEOUT_ACTION_OPEN_FILE: return "OPEN_FILE";
        case TIMEOUT_ACTION_SHOW_TIME: return "SHOW_TIME";
        case TIMEOUT_ACTION_COUNT_UP: return "COUNT_UP";
        case TIMEOUT_ACTION_OPEN_WEBSITE: return "OPEN_WEBSITE";
        case TIMEOUT_ACTION_SLEEP: return "SLEEP";
        case TIMEOUT_ACTION_SHUTDOWN: return "SHUTDOWN";
        case TIMEOUT_ACTION_RESTART: return "RESTART";
        default: return STR_MESSAGE;
    }
}

static TimeoutActionType TimeoutActionTypeFromStr(const char* str) {
    if (!str) return TIMEOUT_ACTION_MESSAGE;
    if (strcmp(str, STR_MESSAGE) == 0) return TIMEOUT_ACTION_MESSAGE;
    if (strcmp(str, "LOCK") == 0) return TIMEOUT_ACTION_LOCK;
    if (strcmp(str, "OPEN_FILE") == 0) return TIMEOUT_ACTION_OPEN_FILE;
    if (strcmp(str, "SHOW_TIME") == 0) return TIMEOUT_ACTION_SHOW_TIME;
    if (strcmp(str, "COUNT_UP") == 0) return TIMEOUT_ACTION_COUNT_UP;
    if (strcmp(str, "OPEN_WEBSITE") == 0) return TIMEOUT_ACTION_OPEN_WEBSITE;
    if (strcmp(str, "SLEEP") == 0) return TIMEOUT_ACTION_SLEEP;
    if (strcmp(str, "SHUTDOWN") == 0) return TIMEOUT_ACTION_SHUTDOWN;
    if (strcmp(str, "RESTART") == 0) return TIMEOUT_ACTION_RESTART;
    return TIMEOUT_ACTION_MESSAGE;
}

static BOOL LoadTimeFormatType(const char* sec, const char* key, void* target, const void* def) {
    char buf[32];
    ReadConfigStr(sec, key, (const char*)def, buf, sizeof(buf));
    TimeFormatType newVal = TimeFormatTypeFromStr(buf);
    if (newVal != *(TimeFormatType*)target) {
        *(TimeFormatType*)target = newVal;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadTimeoutActionType(const char* sec, const char* key, void* target, const void* def) {
    TimeoutActionType* currentAction = (TimeoutActionType*)target;
    
    /* Preserve one-time actions: don't override them from config reload */
    if (*currentAction == TIMEOUT_ACTION_SHUTDOWN ||
        *currentAction == TIMEOUT_ACTION_RESTART ||
        *currentAction == TIMEOUT_ACTION_SLEEP) {
        return FALSE;
    }
    
    char buf[32];
    ReadConfigStr(sec, key, (const char*)def, buf, sizeof(buf));
    TimeoutActionType newVal = TimeoutActionTypeFromStr(buf);
    if (newVal != *currentAction) {
        *currentAction = newVal;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadShowMilliseconds(const char* section, const char* key, void* target, const void* def) {
    BOOL temp = ReadConfigBool(section, key, (BOOL)(intptr_t)def);
    if (temp != *(BOOL*)target) {
        *(BOOL*)target = temp;
        HWND hwnd = GetActiveWindow();
        if (hwnd) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadTimeoutWebsite(const char* section, const char* key, void* target, const void* def) {
    char buffer[MAX_PATH];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    WideString ws = ToWide(buffer);
    if (!ws.valid && buffer[0]) ws.buf[0] = L'\0';
    if (wcscmp(ws.buf, (wchar_t*)target) != 0) {
        wcsncpy_s((wchar_t*)target, MAX_PATH, ws.buf, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadTimeOptions(const char* sec, const char* key, void* target, const void* def) {
    (void)target;
    extern int time_options[];
    extern int time_options_count;
    
    char buf[256];
    ReadConfigStr(sec, key, (const char*)def, buf, sizeof(buf));
    int newArr[MAX_TIME_OPTIONS] = {0}, newCnt = 0;
    char* tok = strtok(buf, ",");
    while (tok && newCnt < MAX_TIME_OPTIONS) {
        while (*tok == ' ') tok++;
        newArr[newCnt++] = atoi(tok);
        tok = strtok(NULL, ",");
    }
    BOOL changed = (newCnt != time_options_count);
    if (!changed) {
        for (int i = 0; i < newCnt; i++) {
            if (newArr[i] != time_options[i]) {
                changed = TRUE;
                break;
            }
        }
    }
    if (changed) {
        time_options_count = newCnt;
        memcpy(time_options, newArr, newCnt * sizeof(int));
    }
    return changed;
}

static BOOL LoadPomodoroOptions(const char* section, const char* key, void* target, const void* def) {
    (void)target;
    
    char buf[128];
    ReadConfigStr(section, key, (const char*)def, buf, sizeof(buf));
    int tmp[3] = {g_AppConfig.pomodoro.work_time, g_AppConfig.pomodoro.short_break, g_AppConfig.pomodoro.long_break}, cnt = 0;
    char* tok = strtok(buf, ",");
    while (tok && cnt < 3) {
        while (*tok == ' ') tok++;
        tmp[cnt++] = atoi(tok);
        tok = strtok(NULL, ",");
    }
    
    BOOL changed = FALSE;
    if (cnt > 0 && tmp[0] != g_AppConfig.pomodoro.work_time) { g_AppConfig.pomodoro.work_time = tmp[0]; changed = TRUE; }
    if (cnt > 1 && tmp[1] != g_AppConfig.pomodoro.short_break) { g_AppConfig.pomodoro.short_break = tmp[1]; changed = TRUE; }
    if (cnt > 2 && tmp[2] != g_AppConfig.pomodoro.long_break) { g_AppConfig.pomodoro.long_break = tmp[2]; changed = TRUE; }
    
    return changed;
}

static BOOL LoadPomodoroLoopCount(const char* section, const char* key, void* target, const void* def) {
    int temp = ReadConfigInt(section, key, (int)(intptr_t)def);
    if (temp < 1) temp = 1;
    if (temp != *(int*)target) {
        *(int*)target = temp;
        return TRUE;
    }
    return FALSE;
}

static BOOL LoadNotificationSettings(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    ReadNotificationMessagesConfig();
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
    ReadNotificationTypeConfig();
    ReadNotificationSoundConfig();
    ReadNotificationVolumeConfig();
    ReadNotificationDisabledConfig();
    ReadNotificationWindowConfig();
    
    return FALSE;
}

static BOOL LoadHotkeys(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)def;
    RegisterGlobalHotkeys(*(HWND*)target);
    return FALSE;
}

static BOOL LoadRecentFilesConfig(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    extern void LoadRecentFiles(void);
    LoadRecentFiles();
    
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    extern char CLOCK_TIMEOUT_FILE_PATH[];
    
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
    
    return FALSE;
}

static BOOL LoadColorOptions(const char* section, const char* key, void* target, const void* def) {
    (void)target;
    char buffer[1024];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    
    ClearColorOptions();
    char* tok = strtok(buffer, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        AddColorOption(tok);
        tok = strtok(NULL, ",");
    }
    
    ReadPercentIconColorsConfig();
    TrayAnimation_UpdatePercentIconIfNeeded();
    
    return TRUE;
}

static BOOL LoadAnimSpeed(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    ReloadAnimationSpeedFromConfig();
    TrayAnimation_RecomputeTimerDelay();
    return FALSE;
}

static BOOL LoadAnimPath(const char* section, const char* key, void* target, const void* def) {
    char buffer[MAX_PATH];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    ApplyAnimationPathValueNoPersist(buffer);
    return FALSE;
}

/* ============================================================================
 * Handler Implementations
 * ============================================================================ */

LRESULT HandleAppDisplayChanged(HWND hwnd) {
    ConfigItem items[] = {
        CFG_STR(CFG_SECTION_DISPLAY, CFG_KEY_TEXT_COLOR, CLOCK_TEXT_COLOR, CLOCK_TEXT_COLOR),
        CFG_INT(CFG_SECTION_DISPLAY, CFG_KEY_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_DISPLAY, NULL, (void*)&hwnd, 0, NULL, LoadDisplayWindowSettings, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppTimerChanged(HWND hwnd) {
    ConfigItem items[] = {
        CFG_BOOL(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR, CLOCK_USE_24HOUR, CLOCK_USE_24HOUR),
        CFG_BOOL(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS, CLOCK_SHOW_SECONDS, CLOCK_SHOW_SECONDS),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIME_FORMAT, (void*)&g_AppConfig.display.time_format.format, 0, "DEFAULT", LoadTimeFormatType, TRUE},
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_SHOW_MILLISECONDS, (void*)&g_AppConfig.display.time_format.show_milliseconds, 0, (void*)FALSE, LoadShowMilliseconds, TRUE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_TEXT, CLOCK_TIMEOUT_TEXT, "0"),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_ACTION, (void*)&CLOCK_TIMEOUT_ACTION, 0, "MESSAGE", LoadTimeoutActionType, FALSE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_FILE, CLOCK_TIMEOUT_FILE_PATH, ""),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_WEBSITE, (void*)CLOCK_TIMEOUT_WEBSITE_URL, 0, "", LoadTimeoutWebsite, FALSE},
        CFG_INT_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_DEFAULT_START_TIME, g_AppConfig.timer.default_start_time, g_AppConfig.timer.default_start_time),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIME_OPTIONS, (void*)time_options, 0, "1500,600,300", LoadTimeOptions, FALSE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_STARTUP_MODE, CLOCK_STARTUP_MODE, CLOCK_STARTUP_MODE)
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppPomodoroChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_POMODORO, CFG_KEY_POMODORO_OPTIONS, (void*)g_AppConfig.pomodoro.times, 0, "1500,300,1500,600", LoadPomodoroOptions, FALSE},
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_POMODORO, CFG_KEY_POMODORO_LOOP_COUNT, (void*)&g_AppConfig.pomodoro.loop_count, 0, (void*)1, LoadPomodoroLoopCount, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppNotificationChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadNotificationSettings, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppHotkeysChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, (void*)&hwnd, 0, NULL, LoadHotkeys, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppRecentFilesChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadRecentFilesConfig, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppColorsChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_COLORS, "COLOR_OPTIONS", NULL, 0,
         DEFAULT_COLOR_OPTIONS_INI,
         LoadColorOptions, TRUE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppAnimSpeedChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadAnimSpeed, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

LRESULT HandleAppAnimPathChanged(HWND hwnd) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, "Animation", "ANIMATION_PATH", NULL, 0, "__logo__", LoadAnimPath, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, sizeof(items) / sizeof(items[0]));
    return 0;
}

