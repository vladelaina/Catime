/**
 * @file config_writer.c
 * @brief Configuration writer implementation
 */

#include "config/config_writer.h"
#include "config.h"
#include "language.h"
#include "timer/timer.h"
#include "window.h"
#include "font.h"
#include "color/color.h"
#include "tray/tray_animation_core.h"
#include "utils/string_safe.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Helper: Check if color is a gradient (contains underscore)
 * ============================================================================ */

static BOOL IsGradientColor(const char* color) {
    return color && strchr(color, '_') != NULL;
}

/* ============================================================================
 * Helper: Enum to string mappings
 * ============================================================================ */

static const char* NotificationTypeToString(NotificationType type) {
    switch (type) {
        case NOTIFICATION_TYPE_CATIME: return "CATIME";
        case NOTIFICATION_TYPE_SYSTEM_MODAL: return "SYSTEM_MODAL";
        case NOTIFICATION_TYPE_OS: return "OS";
        default: return "CATIME";
    }
}

static const char* TimeFormatTypeToString(TimeFormatType format) {
    switch (format) {
        case TIME_FORMAT_DEFAULT: return "DEFAULT";
        case TIME_FORMAT_ZERO_PADDED: return "ZERO_PADDED";
        case TIME_FORMAT_FULL_PADDED: return "FULL_PADDED";
        default: return "DEFAULT";
    }
}

static const char* TimeoutActionTypeToString(TimeoutActionType action) {
    /* One-time actions: never persist to config file */
    if (action == TIMEOUT_ACTION_SHUTDOWN ||
        action == TIMEOUT_ACTION_RESTART ||
        action == TIMEOUT_ACTION_SLEEP) {
        return "MESSAGE";
    }
    
    switch (action) {
        case TIMEOUT_ACTION_MESSAGE: return "MESSAGE";
        case TIMEOUT_ACTION_LOCK: return "LOCK";
        case TIMEOUT_ACTION_OPEN_FILE: return "OPEN_FILE";
        case TIMEOUT_ACTION_SHOW_TIME: return "SHOW_TIME";
        case TIMEOUT_ACTION_COUNT_UP: return "COUNT_UP";
        case TIMEOUT_ACTION_OPEN_WEBSITE: return "OPEN_WEBSITE";
        default: return "MESSAGE";
    }
}

static const char* LanguageToString(AppLanguage lang) {
    switch (lang) {
        case APP_LANG_CHINESE_SIMP: return "Chinese_Simplified";
        case APP_LANG_CHINESE_TRAD: return "Chinese_Traditional";
        case APP_LANG_ENGLISH: return "English";
        case APP_LANG_SPANISH: return "Spanish";
        case APP_LANG_FRENCH: return "French";
        case APP_LANG_GERMAN: return "German";
        case APP_LANG_RUSSIAN: return "Russian";
        case APP_LANG_PORTUGUESE: return "Portuguese";
        case APP_LANG_JAPANESE: return "Japanese";
        case APP_LANG_KOREAN: return "Korean";
        default: return "English";
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

BOOL CollectCurrentConfig(ConfigWriteItem* items, int* count) {
    if (!items || !count) return FALSE;
    
    int idx = 0;
    
    /* General section */
    safe_strncpy(items[idx].section, INI_SECTION_GENERAL, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CONFIG_VERSION", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CATIME_VERSION, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_GENERAL, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "LANGUAGE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, LanguageToString(GetCurrentLanguage()), sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_GENERAL, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "SHORTCUT_CHECK_DONE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, IsShortcutCheckDone() ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;
    
    /* Display section */
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TEXT_COLOR", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_TEXT_COLOR, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_BASE_FONT_SIZE", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", CLOCK_BASE_FONT_SIZE);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "FONT_FILE_NAME", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, FONT_FILE_NAME, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_WINDOW_POS_X", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", CLOCK_WINDOW_POS_X);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_WINDOW_POS_Y", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", CLOCK_WINDOW_POS_Y);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "WINDOW_SCALE", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%.2f", CLOCK_WINDOW_SCALE);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "WINDOW_TOPMOST", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "WINDOW_OPACITY", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", CLOCK_WINDOW_OPACITY);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "MOVE_STEP_SMALL", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.move_step_small);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "MOVE_STEP_LARGE", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.move_step_large);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "OPACITY_STEP_NORMAL", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.opacity_step_normal);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "OPACITY_STEP_FAST", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.opacity_step_fast);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "SCALE_STEP_NORMAL", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.scale_step_normal);
    idx++;

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "SCALE_STEP_FAST", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.display.scale_step_fast);
    idx++;

    /* Timer section */
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_DEFAULT_START_TIME", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.timer.default_start_time);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_USE_24HOUR", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_USE_24HOUR ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_SHOW_SECONDS", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIME_FORMAT", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, TimeFormatTypeToString(g_AppConfig.display.time_format.format), sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_SHOW_MILLISECONDS", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, g_AppConfig.display.time_format.show_milliseconds ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIMEOUT_TEXT", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_TIMEOUT_TEXT, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIMEOUT_ACTION", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, TimeoutActionTypeToString(CLOCK_TIMEOUT_ACTION), sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIMEOUT_FILE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_TIMEOUT_FILE_PATH, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIMEOUT_WEBSITE", sizeof(items[idx].key));
    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, 
                       items[idx].value, sizeof(items[idx].value), NULL, NULL);
    idx++;
    
    /* Time options */
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIME_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", time_options[i]);
        if (i > 0) safe_strncat(items[idx].value, ",", sizeof(items[idx].value));
        safe_strncat(items[idx].value, buffer, sizeof(items[idx].value));
    }
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "STARTUP_MODE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, CLOCK_STARTUP_MODE, sizeof(items[idx].value));
    idx++;
    
    /* Pomodoro section */
    safe_strncpy(items[idx].section, INI_SECTION_POMODORO, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "POMODORO_TIME_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    for (int i = 0; i < g_AppConfig.pomodoro.times_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", g_AppConfig.pomodoro.times[i]);
        if (i > 0) safe_strncat(items[idx].value, ",", sizeof(items[idx].value));
        safe_strncat(items[idx].value, buffer, sizeof(items[idx].value));
    }
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_POMODORO, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "POMODORO_LOOP_COUNT", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.pomodoro.loop_count);
    idx++;
    
    /* Notification section */
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIMEOUT_MESSAGE_TEXT", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, g_AppConfig.notification.messages.timeout_message, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_TIMEOUT_MS", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.timeout_ms);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_MAX_OPACITY", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.max_opacity);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_TYPE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, NotificationTypeToString(g_AppConfig.notification.display.type), sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_SOUND_FILE", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, g_AppConfig.notification.sound.sound_file, sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_SOUND_VOLUME", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.sound.volume);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_DISABLED", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, g_AppConfig.notification.display.disabled ? "TRUE" : "FALSE", sizeof(items[idx].value));
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_WINDOW_X", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.window_x);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_WINDOW_Y", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.window_y);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_WINDOW_WIDTH", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.window_width);
    idx++;
    
    safe_strncpy(items[idx].section, INI_SECTION_NOTIFICATION, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "NOTIFICATION_WINDOW_HEIGHT", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%d", g_AppConfig.notification.display.window_height);
    idx++;
    
    /* Hotkeys */
    WORD hotkeys[12];
    ReadConfigHotkeys(&hotkeys[0], &hotkeys[1], &hotkeys[2], &hotkeys[3],
                     &hotkeys[4], &hotkeys[5], &hotkeys[6], &hotkeys[7],
                     &hotkeys[8], &hotkeys[9], &hotkeys[10]);
    ReadCustomCountdownHotkey(&hotkeys[11]);
    
    const char* hotkeyNames[] = {
        "HOTKEY_SHOW_TIME", "HOTKEY_COUNT_UP", "HOTKEY_COUNTDOWN",
        "HOTKEY_QUICK_COUNTDOWN1", "HOTKEY_QUICK_COUNTDOWN2", "HOTKEY_QUICK_COUNTDOWN3",
        "HOTKEY_POMODORO", "HOTKEY_TOGGLE_VISIBILITY", "HOTKEY_EDIT_MODE",
        "HOTKEY_PAUSE_RESUME", "HOTKEY_RESTART_TIMER", "HOTKEY_CUSTOM_COUNTDOWN"
    };
    
    for (int i = 0; i < 12; i++) {
        safe_strncpy(items[idx].section, INI_SECTION_HOTKEYS, sizeof(items[idx].section));
        safe_strncpy(items[idx].key, hotkeyNames[i], sizeof(items[idx].key));
        HotkeyToString(hotkeys[i], items[idx].value, sizeof(items[idx].value));
        idx++;
    }
    
    /* Recent files */
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        safe_strncpy(items[idx].section, INI_SECTION_RECENTFILES, sizeof(items[idx].section));
        snprintf(items[idx].key, sizeof(items[idx].key), "CLOCK_RECENT_FILE_%d", i + 1);
        if (i < g_AppConfig.recent_files.count) {
            safe_strncpy(items[idx].value, g_AppConfig.recent_files.files[i].path, sizeof(items[idx].value));
        } else {
            safe_strncpy(items[idx].value, "", sizeof(items[idx].value));
        }
        idx++;
    }
    
    /* Colors - write single colors first, then gradients */
    safe_strncpy(items[idx].section, INI_SECTION_COLORS, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "COLOR_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    BOOL firstColor = TRUE;
    /* First pass: single colors */
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (!IsGradientColor(COLOR_OPTIONS[i].hexColor)) {
            if (!firstColor) safe_strncat(items[idx].value, ",", sizeof(items[idx].value));
            safe_strncat(items[idx].value, COLOR_OPTIONS[i].hexColor, sizeof(items[idx].value));
            firstColor = FALSE;
        }
    }
    /* Second pass: gradient colors */
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (IsGradientColor(COLOR_OPTIONS[i].hexColor)) {
            if (!firstColor) safe_strncat(items[idx].value, ",", sizeof(items[idx].value));
            safe_strncat(items[idx].value, COLOR_OPTIONS[i].hexColor, sizeof(items[idx].value));
            firstColor = FALSE;
        }
    }
    idx++;
    
    *count = idx;
    return TRUE;
}

BOOL WriteConfigItems(const char* config_path, const ConfigWriteItem* items, int count) {
    if (!config_path || !items || count <= 0) return FALSE;
    
    for (int i = 0; i < count; i++) {
        WriteIniString(items[i].section, items[i].key, items[i].value, config_path);
    }
    
    return TRUE;
}

void WriteConfig(const char* config_path) {
    if (!config_path) return;
    
    ConfigWriteItem items[150];
    int count = 0;
    
    if (!CollectCurrentConfig(items, &count)) {
        return;
    }
    
    /* Write all items */
    WriteConfigItems(config_path, items, count);
    
    /* Preserve FIRST_RUN flag */
    char currentFirstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", 
                 currentFirstRun, sizeof(currentFirstRun), config_path);
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", currentFirstRun, config_path);
    
    /* Write animation settings */
    const char* anim = GetCurrentAnimationName();
    if (anim && anim[0] != '\0') {
        char animPath[MAX_PATH];
        if (_stricmp(anim, "__logo__") == 0 || _stricmp(anim, "__cpu__") == 0 || _stricmp(anim, "__mem__") == 0) {
            snprintf(animPath, sizeof(animPath), "%s", anim);
        } else {
            snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", anim);
        }
        WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);
    }
    
    WriteAnimationSpeedToConfig(config_path);
}

void WriteConfigSection(const char* config_path, const char* section) {
    /* Selective section update - collect only items from specified section */
    ConfigWriteItem allItems[150];
    int allCount = 0;
    
    if (!CollectCurrentConfig(allItems, &allCount)) {
        return;
    }
    
    /* Filter items by section */
    for (int i = 0; i < allCount; i++) {
        if (strcmp(allItems[i].section, section) == 0) {
            WriteIniString(allItems[i].section, allItems[i].key, 
                          allItems[i].value, config_path);
        }
    }
}

void WriteConfigWindowOpacity(int opacity) {
    if (opacity < 0) opacity = 0;
    if (opacity > 100) opacity = 100;

    CLOCK_WINDOW_OPACITY = opacity;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY", opacity, config_path);
}

void WriteConfigMoveSteps(int small_step, int large_step) {
    if (small_step < 1) small_step = 1;
    if (small_step > 500) small_step = 500;
    if (large_step < 1) large_step = 1;
    if (large_step > 500) large_step = 500;

    g_AppConfig.display.move_step_small = small_step;
    g_AppConfig.display.move_step_large = large_step;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniInt(INI_SECTION_DISPLAY, "MOVE_STEP_SMALL", small_step, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "MOVE_STEP_LARGE", large_step, config_path);
}

void WriteConfigScaleSteps(int normal_step, int fast_step) {
    if (normal_step < 1) normal_step = 1;
    if (normal_step > 100) normal_step = 100;
    if (fast_step < 1) fast_step = 1;
    if (fast_step > 100) fast_step = 100;

    g_AppConfig.display.scale_step_normal = normal_step;
    g_AppConfig.display.scale_step_fast = fast_step;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniInt(INI_SECTION_DISPLAY, "SCALE_STEP_NORMAL", normal_step, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "SCALE_STEP_FAST", fast_step, config_path);
}

void FlushConfigToDisk(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /* Write all config items to disk */
    ConfigWriteItem items[150];
    int count = 0;
    if (CollectCurrentConfig(items, &count)) {
        WriteConfigItems(config_path, items, count);
    }
}

