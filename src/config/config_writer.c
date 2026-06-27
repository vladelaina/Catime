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
#include "text_effect.h"
#include "utils/string_safe.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Helper: Check if color is a gradient (contains underscore)
 * ============================================================================ */

static BOOL IsGradientColor(const char* color) {
    return color && strchr(color, '_') != NULL;
}

static BOOL AppendConfigListToken(char* dest, size_t destSize,
                                  const char* token, BOOL* isFirst,
                                  const char* listName) {
    if (!dest || destSize == 0 || !token || !isFirst) {
        return FALSE;
    }

    size_t destLen = strnlen(dest, destSize);
    if (destLen >= destSize) {
        return FALSE;
    }

    size_t sepLen = *isFirst ? 0 : 1;
    size_t tokenLen = strlen(token);
    size_t remaining = destSize - destLen - 1;
    if (sepLen > remaining || tokenLen > remaining - sepLen) {
        LOG_WARNING("%s too long to write completely; dropping remaining entries",
                    listName ? listName : "Config list");
        return FALSE;
    }

    if (sepLen) {
        dest[destLen++] = ',';
    }
    memcpy(dest + destLen, token, tokenLen);
    dest[destLen + tokenLen] = '\0';
    *isFirst = FALSE;
    return TRUE;
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
    return GetLanguageConfigKey(lang);
}

static BOOL EnsureConfigItemCapacity(int idx, int itemCapacity, int needed,
                                     const char* context) {
    if (idx < 0 || itemCapacity < 0 || needed < 0 ||
        idx > itemCapacity - needed) {
        LOG_ERROR("Config write item capacity exceeded while collecting %s (idx=%d needed=%d capacity=%d)",
                  context ? context : "configuration",
                  idx, needed, itemCapacity);
        return FALSE;
    }

    return TRUE;
}

static BOOL ConfigIniValueMatches(const char* config_path, const char* section,
                                  const char* key, const char* expected) {
    char current[2048] = {0};
    if (!config_path || !section || !key) {
        return FALSE;
    }

    ReadIniString(section, key, "", current, sizeof(current), config_path);
    return strcmp(current, expected ? expected : "") == 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

BOOL CollectCurrentConfig(ConfigWriteItem* items, int itemCapacity, int* count) {
    if (!items || itemCapacity <= 0 || !count) return FALSE;
    
    int idx = 0;
    *count = 0;
    
    /* General section */
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 3, "General")) return FALSE;
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
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 17, "Display")) return FALSE;
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
    safe_strncpy(items[idx].key, "PLUGIN_SCALE", sizeof(items[idx].key));
    snprintf(items[idx].value, sizeof(items[idx].value), "%.2f", PLUGIN_FONT_SCALE_FACTOR);
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

    safe_strncpy(items[idx].section, INI_SECTION_DISPLAY, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "TEXT_EFFECT", sizeof(items[idx].key));
    safe_strncpy(items[idx].value, TextEffect_ToConfigString(CLOCK_TEXT_EFFECT), sizeof(items[idx].value));
    idx++;

    /* Timer section */
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 11, "Timer")) return FALSE;
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
    safe_strncpy(items[idx].value, CLOCK_TIMEOUT_WEBSITE_URL, sizeof(items[idx].value));
    idx++;
    
    /* Time options */
    safe_strncpy(items[idx].section, INI_SECTION_TIMER, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "CLOCK_TIME_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    int timeOptionsCount = time_options_count;
    if (timeOptionsCount < 0) timeOptionsCount = 0;
    if (timeOptionsCount > MAX_TIME_OPTIONS) timeOptionsCount = MAX_TIME_OPTIONS;
    for (int i = 0; i < timeOptionsCount; i++) {
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
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 2, "Pomodoro")) return FALSE;
    safe_strncpy(items[idx].section, INI_SECTION_POMODORO, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "POMODORO_TIME_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    int pomodoroTimesCount = g_AppConfig.pomodoro.times_count;
    if (pomodoroTimesCount < 0) pomodoroTimesCount = 0;
    if (pomodoroTimesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        pomodoroTimesCount = (int)_countof(g_AppConfig.pomodoro.times);
    }
    for (int i = 0; i < pomodoroTimesCount; i++) {
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
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 11, "Notification")) return FALSE;
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
    WORD hotkeys[14];
    ReadConfigHotkeys(&hotkeys[0], &hotkeys[1], &hotkeys[2], &hotkeys[3],
                     &hotkeys[4], &hotkeys[5], &hotkeys[6], &hotkeys[7],
                     &hotkeys[8], &hotkeys[9], &hotkeys[10], &hotkeys[12], &hotkeys[13]);
    ReadCustomCountdownHotkey(&hotkeys[11]);
    
    const char* hotkeyNames[] = {
        "HOTKEY_SHOW_TIME", "HOTKEY_COUNT_UP", "HOTKEY_COUNTDOWN",
        "HOTKEY_QUICK_COUNTDOWN1", "HOTKEY_QUICK_COUNTDOWN2", "HOTKEY_QUICK_COUNTDOWN3",
        "HOTKEY_POMODORO", "HOTKEY_TOGGLE_VISIBILITY", "HOTKEY_EDIT_MODE",
        "HOTKEY_PAUSE_RESUME", "HOTKEY_RESTART_TIMER", "HOTKEY_CUSTOM_COUNTDOWN",
        "HOTKEY_TOGGLE_MILLISECONDS", "HOTKEY_TOPMOST"
    };
    
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 14, "Hotkeys")) return FALSE;
    for (int i = 0; i < 14; i++) {
        safe_strncpy(items[idx].section, INI_SECTION_HOTKEYS, sizeof(items[idx].section));
        safe_strncpy(items[idx].key, hotkeyNames[i], sizeof(items[idx].key));
        HotkeyToString(hotkeys[i], items[idx].value, sizeof(items[idx].value));
        idx++;
    }
    
    /* Recent files */
    if (!EnsureConfigItemCapacity(idx, itemCapacity, MAX_RECENT_FILES, "Recent files")) return FALSE;
    int recentFilesCount = g_AppConfig.recent_files.count;
    if (recentFilesCount < 0) recentFilesCount = 0;
    if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        safe_strncpy(items[idx].section, INI_SECTION_RECENTFILES, sizeof(items[idx].section));
        snprintf(items[idx].key, sizeof(items[idx].key), "CLOCK_RECENT_FILE_%d", i + 1);
        if (i < recentFilesCount) {
            safe_strncpy(items[idx].value, g_AppConfig.recent_files.files[i].path, sizeof(items[idx].value));
        } else {
            safe_strncpy(items[idx].value, "", sizeof(items[idx].value));
        }
        idx++;
    }
    
    /* Colors - write single colors first, then gradients */
    if (!EnsureConfigItemCapacity(idx, itemCapacity, 1, "Colors")) return FALSE;
    safe_strncpy(items[idx].section, INI_SECTION_COLORS, sizeof(items[idx].section));
    safe_strncpy(items[idx].key, "COLOR_OPTIONS", sizeof(items[idx].key));
    items[idx].value[0] = '\0';
    BOOL firstColor = TRUE;
    BOOL colorOptionsFull = TRUE;
    /* First pass: single colors */
    for (size_t i = 0; colorOptionsFull && i < COLOR_OPTIONS_COUNT; i++) {
        if (!IsGradientColor(COLOR_OPTIONS[i].hexColor)) {
            colorOptionsFull = AppendConfigListToken(
                items[idx].value, sizeof(items[idx].value),
                COLOR_OPTIONS[i].hexColor, &firstColor, "COLOR_OPTIONS");
        }
    }
    /* Second pass: gradient colors */
    for (size_t i = 0; colorOptionsFull && i < COLOR_OPTIONS_COUNT; i++) {
        if (IsGradientColor(COLOR_OPTIONS[i].hexColor)) {
            colorOptionsFull = AppendConfigListToken(
                items[idx].value, sizeof(items[idx].value),
                COLOR_OPTIONS[i].hexColor, &firstColor, "COLOR_OPTIONS");
        }
    }
    if (!colorOptionsFull || firstColor) {
        LOG_ERROR("Failed to collect COLOR_OPTIONS for full config write");
        return FALSE;
    }
    idx++;
    
    *count = idx;
    return TRUE;
}

BOOL WriteConfigItems(const char* config_path, const ConfigWriteItem* items, int count) {
    if (!config_path || !items || count <= 0) return FALSE;

    IniKeyValue stackUpdates[CONFIG_WRITE_ITEM_CAPACITY];
    IniKeyValue* updates = stackUpdates;
    if (count > CONFIG_WRITE_ITEM_CAPACITY) {
        updates = (IniKeyValue*)calloc((size_t)count, sizeof(IniKeyValue));
        if (!updates) {
            return FALSE;
        }
    }

    for (int i = 0; i < count; i++) {
        updates[i].section = items[i].section;
        updates[i].key = items[i].key;
        updates[i].value = items[i].value;
    }

    BOOL result = WriteIniMultipleAtomic(config_path, updates, (size_t)count);
    if (updates != stackUpdates) {
        free(updates);
    }

    return result;
}

BOOL WriteConfig(const char* config_path) {
    if (!config_path) return FALSE;

    ConfigWriteItem* items = (ConfigWriteItem*)calloc(CONFIG_WRITE_ITEM_CAPACITY,
                                                      sizeof(ConfigWriteItem));
    if (!items) {
        return FALSE;
    }

    int count = 0;

    if (!CollectCurrentConfig(items, CONFIG_WRITE_ITEM_CAPACITY, &count)) {
        free(items);
        return FALSE;
    }

    /* Preserve existing animation config; only add current animation when the key is missing. */
    char existingAnimPath[MAX_PATH] = {0};
    BOOL existingAnimPathComplete = ReadIniStringExact(
        "Animation", "ANIMATION_PATH", "", existingAnimPath, sizeof(existingAnimPath), config_path);
    if (!existingAnimPathComplete) {
        LOG_WARNING("Replacing ANIMATION_PATH during full config write because the existing value is too long");
    }

    if ((!existingAnimPathComplete || existingAnimPath[0] == '\0') &&
        count < CONFIG_WRITE_ITEM_CAPACITY) {
        const char* anim = GetCurrentAnimationName();
        if (anim && anim[0] != '\0') {
            char animPath[MAX_PATH];
            int animPathLen = 0;
            if (_stricmp(anim, "__logo__") == 0 || _stricmp(anim, "__cpu__") == 0 ||
                _stricmp(anim, "__mem__") == 0 || _stricmp(anim, "__battery__") == 0 ||
                _stricmp(anim, "__capslock__") == 0 || _stricmp(anim, "__none__") == 0) {
                animPathLen = snprintf(animPath, sizeof(animPath), "%s", anim);
            } else {
                animPathLen = snprintf(animPath, sizeof(animPath),
                                       "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", anim);
            }
            if (animPathLen >= 0 && animPathLen < (int)sizeof(animPath)) {
                safe_strncpy(items[count].section, "Animation", sizeof(items[count].section));
                safe_strncpy(items[count].key, "ANIMATION_PATH", sizeof(items[count].key));
                safe_strncpy(items[count].value, animPath, sizeof(items[count].value));
                count++;
            }
        }
    }

    if (!CollectAnimationSpeedConfigItems(items, CONFIG_WRITE_ITEM_CAPACITY, &count)) {
        LOG_ERROR("Failed to collect animation speed config during full config write");
        free(items);
        return FALSE;
    }

    BOOL result = WriteConfigItems(config_path, items, count);
    if (!result) {
        LOG_ERROR("Failed to write complete config: %s", config_path);
    }
    free(items);
    return result;
}

BOOL WriteConfigSection(const char* config_path, const char* section) {
    if (!config_path || !section) {
        return FALSE;
    }

    /* Selective section update - collect only items from specified section */
    ConfigWriteItem* allItems = (ConfigWriteItem*)calloc(CONFIG_WRITE_ITEM_CAPACITY,
                                                         sizeof(ConfigWriteItem));
    if (!allItems) {
        return FALSE;
    }

    int allCount = 0;

    if (!CollectCurrentConfig(allItems, CONFIG_WRITE_ITEM_CAPACITY, &allCount)) {
        free(allItems);
        return FALSE;
    }

    IniKeyValue updates[CONFIG_WRITE_ITEM_CAPACITY];
    size_t updateCount = 0;

    for (int i = 0; i < allCount; i++) {
        if (strcmp(allItems[i].section, section) == 0) {
            updates[updateCount].section = allItems[i].section;
            updates[updateCount].key = allItems[i].key;
            updates[updateCount].value = allItems[i].value;
            updateCount++;
        }
    }

    BOOL result = TRUE;
    if (updateCount > 0) {
        result = WriteIniMultipleAtomic(config_path, updates, updateCount);
        if (!result) {
            LOG_ERROR("Failed to write config section '%s': %s", section, config_path);
        }
    }

    free(allItems);
    return result;
}

void WriteConfigWindowOpacity(int opacity) {
    if (opacity < 0) opacity = 0;
    if (opacity > 100) opacity = 100;

    char opacityStr[32];
    if (snprintf(opacityStr, sizeof(opacityStr), "%d", opacity) < 0) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (CLOCK_WINDOW_OPACITY == opacity);
    BOOL configMatches = ConfigIniValueMatches(config_path, INI_SECTION_DISPLAY,
                                               "WINDOW_OPACITY", opacityStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    if (!configMatches &&
        !WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY", opacity, config_path)) {
        return;
    }

    CLOCK_WINDOW_OPACITY = opacity;
}

void WriteConfigMoveSteps(int small_step, int large_step) {
    if (small_step < 1) small_step = 1;
    if (small_step > 500) small_step = 500;
    if (large_step < 1) large_step = 1;
    if (large_step > 500) large_step = 500;

    char smallStepStr[32];
    char largeStepStr[32];
    if (snprintf(smallStepStr, sizeof(smallStepStr), "%d", small_step) < 0 ||
        snprintf(largeStepStr, sizeof(largeStepStr), "%d", large_step) < 0) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches =
        g_AppConfig.display.move_step_small == small_step &&
        g_AppConfig.display.move_step_large == large_step;
    BOOL configMatches =
        ConfigIniValueMatches(config_path, INI_SECTION_DISPLAY,
                              "MOVE_STEP_SMALL", smallStepStr) &&
        ConfigIniValueMatches(config_path, INI_SECTION_DISPLAY,
                              "MOVE_STEP_LARGE", largeStepStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "MOVE_STEP_SMALL", smallStepStr},
        {INI_SECTION_DISPLAY, "MOVE_STEP_LARGE", largeStepStr},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return;
    }

    g_AppConfig.display.move_step_small = small_step;
    g_AppConfig.display.move_step_large = large_step;
}

void WriteConfigScaleSteps(int normal_step, int fast_step) {
    if (normal_step < 1) normal_step = 1;
    if (normal_step > 100) normal_step = 100;
    if (fast_step < 1) fast_step = 1;
    if (fast_step > 100) fast_step = 100;

    char normalStepStr[32];
    char fastStepStr[32];
    if (snprintf(normalStepStr, sizeof(normalStepStr), "%d", normal_step) < 0 ||
        snprintf(fastStepStr, sizeof(fastStepStr), "%d", fast_step) < 0) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches =
        g_AppConfig.display.scale_step_normal == normal_step &&
        g_AppConfig.display.scale_step_fast == fast_step;
    BOOL configMatches =
        ConfigIniValueMatches(config_path, INI_SECTION_DISPLAY,
                              "SCALE_STEP_NORMAL", normalStepStr) &&
        ConfigIniValueMatches(config_path, INI_SECTION_DISPLAY,
                              "SCALE_STEP_FAST", fastStepStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "SCALE_STEP_NORMAL", normalStepStr},
        {INI_SECTION_DISPLAY, "SCALE_STEP_FAST", fastStepStr},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return;
    }

    g_AppConfig.display.scale_step_normal = normal_step;
    g_AppConfig.display.scale_step_fast = fast_step;
}
