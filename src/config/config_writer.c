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
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>

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
    /* Security: Filter dangerous actions */
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
    strcpy(items[idx].section, INI_SECTION_GENERAL);
    strcpy(items[idx].key, "CONFIG_VERSION");
    strcpy(items[idx].value, CATIME_VERSION);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_GENERAL);
    strcpy(items[idx].key, "LANGUAGE");
    strcpy(items[idx].value, LanguageToString(GetCurrentLanguage()));
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_GENERAL);
    strcpy(items[idx].key, "SHORTCUT_CHECK_DONE");
    strcpy(items[idx].value, IsShortcutCheckDone() ? "TRUE" : "FALSE");
    idx++;
    
    /* Display section */
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "CLOCK_TEXT_COLOR");
    strcpy(items[idx].value, CLOCK_TEXT_COLOR);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "CLOCK_BASE_FONT_SIZE");
    sprintf(items[idx].value, "%d", CLOCK_BASE_FONT_SIZE);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "FONT_FILE_NAME");
    strcpy(items[idx].value, FONT_FILE_NAME);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "CLOCK_WINDOW_POS_X");
    sprintf(items[idx].value, "%d", CLOCK_WINDOW_POS_X);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "CLOCK_WINDOW_POS_Y");
    sprintf(items[idx].value, "%d", CLOCK_WINDOW_POS_Y);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "WINDOW_SCALE");
    sprintf(items[idx].value, "%.2f", CLOCK_WINDOW_SCALE);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_DISPLAY);
    strcpy(items[idx].key, "WINDOW_TOPMOST");
    strcpy(items[idx].value, CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE");
    idx++;
    
    /* Timer section */
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_DEFAULT_START_TIME");
    sprintf(items[idx].value, "%d", g_AppConfig.timer.default_start_time);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_USE_24HOUR");
    strcpy(items[idx].value, CLOCK_USE_24HOUR ? "TRUE" : "FALSE");
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_SHOW_SECONDS");
    strcpy(items[idx].value, CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE");
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIME_FORMAT");
    strcpy(items[idx].value, TimeFormatTypeToString(g_AppConfig.display.time_format.format));
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_SHOW_MILLISECONDS");
    strcpy(items[idx].value, g_AppConfig.display.time_format.show_milliseconds ? "TRUE" : "FALSE");
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIMEOUT_TEXT");
    strcpy(items[idx].value, CLOCK_TIMEOUT_TEXT);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIMEOUT_ACTION");
    strcpy(items[idx].value, TimeoutActionTypeToString(CLOCK_TIMEOUT_ACTION));
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIMEOUT_FILE");
    strcpy(items[idx].value, CLOCK_TIMEOUT_FILE_PATH);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIMEOUT_WEBSITE");
    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, 
                       items[idx].value, sizeof(items[idx].value), NULL, NULL);
    idx++;
    
    /* Time options */
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "CLOCK_TIME_OPTIONS");
    items[idx].value[0] = '\0';
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        sprintf(buffer, "%d", time_options[i]);
        if (i > 0) strcat(items[idx].value, ",");
        strcat(items[idx].value, buffer);
    }
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_TIMER);
    strcpy(items[idx].key, "STARTUP_MODE");
    strcpy(items[idx].value, CLOCK_STARTUP_MODE);
    idx++;
    
    /* Pomodoro section */
    strcpy(items[idx].section, INI_SECTION_POMODORO);
    strcpy(items[idx].key, "POMODORO_TIME_OPTIONS");
    items[idx].value[0] = '\0';
    for (int i = 0; i < g_AppConfig.pomodoro.times_count; i++) {
        char buffer[16];
        sprintf(buffer, "%d", g_AppConfig.pomodoro.times[i]);
        if (i > 0) strcat(items[idx].value, ",");
        strcat(items[idx].value, buffer);
    }
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_POMODORO);
    strcpy(items[idx].key, "POMODORO_LOOP_COUNT");
    sprintf(items[idx].value, "%d", g_AppConfig.pomodoro.loop_count);
    idx++;
    
    /* Notification section */
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "CLOCK_TIMEOUT_MESSAGE_TEXT");
    strcpy(items[idx].value, g_AppConfig.notification.messages.timeout_message);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "POMODORO_TIMEOUT_MESSAGE_TEXT");
    strcpy(items[idx].value, g_AppConfig.notification.messages.pomodoro_message);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "POMODORO_CYCLE_COMPLETE_TEXT");
    strcpy(items[idx].value, g_AppConfig.notification.messages.cycle_complete_message);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_TIMEOUT_MS");
    sprintf(items[idx].value, "%d", g_AppConfig.notification.display.timeout_ms);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_MAX_OPACITY");
    sprintf(items[idx].value, "%d", g_AppConfig.notification.display.max_opacity);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_TYPE");
    strcpy(items[idx].value, NotificationTypeToString(g_AppConfig.notification.display.type));
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_SOUND_FILE");
    strcpy(items[idx].value, g_AppConfig.notification.sound.sound_file);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_SOUND_VOLUME");
    sprintf(items[idx].value, "%d", g_AppConfig.notification.sound.volume);
    idx++;
    
    strcpy(items[idx].section, INI_SECTION_NOTIFICATION);
    strcpy(items[idx].key, "NOTIFICATION_DISABLED");
    strcpy(items[idx].value, g_AppConfig.notification.display.disabled ? "TRUE" : "FALSE");
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
        strcpy(items[idx].section, INI_SECTION_HOTKEYS);
        strcpy(items[idx].key, hotkeyNames[i]);
        HotkeyToString(hotkeys[i], items[idx].value, sizeof(items[idx].value));
        idx++;
    }
    
    /* Recent files */
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        strcpy(items[idx].section, INI_SECTION_RECENTFILES);
        sprintf(items[idx].key, "CLOCK_RECENT_FILE_%d", i + 1);
        if (i < g_AppConfig.recent_files.count) {
            strcpy(items[idx].value, g_AppConfig.recent_files.files[i].path);
        } else {
            strcpy(items[idx].value, "");
        }
        idx++;
    }
    
    /* Colors */
    strcpy(items[idx].section, INI_SECTION_COLORS);
    strcpy(items[idx].key, "COLOR_OPTIONS");
    items[idx].value[0] = '\0';
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) strcat(items[idx].value, ",");
        strcat(items[idx].value, COLOR_OPTIONS[i].hexColor);
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
        snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", anim);
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

