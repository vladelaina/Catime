/**
 * @file config_notification.c
 * @brief Notification configuration management
 */
#include "config.h"
#include "config/config_defaults.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/** All notification config now in g_AppConfig.notification */

/** Enum-string mapping for notification types */
typedef struct {
    int value;
    const char* str;
} EnumStrMap;

static const EnumStrMap NOTIFICATION_TYPE_MAP[] = {
    {NOTIFICATION_TYPE_CATIME,       "CATIME"},
    {NOTIFICATION_TYPE_SYSTEM_MODAL, "SYSTEM_MODAL"},
    {NOTIFICATION_TYPE_OS,           "OS"},
    {-1, NULL}
};

static const char* EnumToString(const EnumStrMap* map, int value, const char* defaultVal) {
    if (!map) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (map[i].value == value) {
            return map[i].str;
        }
    }
    return defaultVal;
}

static void BuildNotificationSoundConfigValue(const char* sound_file,
                                              char* clean_path, size_t clean_size,
                                              char* config_value, size_t config_size) {
    if (!clean_path || clean_size == 0 || !config_value || config_size == 0) return;

    clean_path[0] = '\0';
    config_value[0] = '\0';

    if (!sound_file) {
        return;
    }

    const char* src = sound_file;
    char* dst = clean_path;
    size_t remaining = clean_size - 1;
    while (*src && remaining > 0) {
        if (*src != '=') {
            *dst++ = *src;
            remaining--;
        }
        src++;
    }
    *dst = '\0';

    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData) {
        size_t localLen = strlen(localAppData);
        if (_strnicmp(clean_path, localAppData, localLen) == 0) {
            const char* rest = clean_path + localLen;
            if (*rest == '\\') rest++;
            int written = snprintf(config_value, config_size, "%%LOCALAPPDATA%%\\%s", rest);
            if (written >= 0 && (size_t)written < config_size) {
                return;
            }
            config_value[0] = '\0';
        }
    }

    strncpy(config_value, clean_path, config_size - 1);
    config_value[config_size - 1] = '\0';
}

static BOOL NotificationIniValueMatches(const char* config_path, const char* key,
                                        const char* expected) {
    char current[2048] = {0};
    if (!ReadIniStringExact(INI_SECTION_NOTIFICATION, key, "", current,
                            sizeof(current), config_path)) {
        return FALSE;
    }
    return strcmp(current, expected ? expected : "") == 0;
}


/**
 * @brief Update notification message texts in config
 */
BOOL WriteConfigNotificationMessages(const char* timeout_msg) {
    if (!timeout_msg) timeout_msg = "";

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches =
        strcmp(g_AppConfig.notification.messages.timeout_message, timeout_msg) == 0;
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT",
                        timeout_msg, config_path)) {
        return FALSE;
    }

    strncpy(g_AppConfig.notification.messages.timeout_message, timeout_msg, sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    return TRUE;
}


/**
 * @brief Write notification timeout setting to config file
 */
BOOL WriteConfigNotificationTimeout(int timeout_ms) {
    if (timeout_ms < 0) timeout_ms = 0;

    char timeoutStr[32];
    if (snprintf(timeoutStr, sizeof(timeoutStr), "%d", timeout_ms) < 0) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (g_AppConfig.notification.display.timeout_ms == timeout_ms);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_TIMEOUT_MS", timeoutStr);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeout_ms)) {
        return FALSE;
    }

    g_AppConfig.notification.display.timeout_ms = timeout_ms;
    return TRUE;
}


/**
 * @brief Write notification opacity setting to config file
 */
BOOL WriteConfigNotificationOpacity(int opacity) {
    if (opacity < 1) opacity = 1;
    if (opacity > 100) opacity = 100;

    char opacityStr[32];
    if (snprintf(opacityStr, sizeof(opacityStr), "%d", opacity) < 0) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (g_AppConfig.notification.display.max_opacity == opacity);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_MAX_OPACITY", opacityStr);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", opacity)) {
        return FALSE;
    }

    g_AppConfig.notification.display.max_opacity = opacity;
    return TRUE;
}


/**
 * @brief Write notification type setting to config file
 */
void WriteConfigNotificationType(NotificationType type) {
    /** Validate notification type range */
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME;
    }

    const char* typeStr = EnumToString(NOTIFICATION_TYPE_MAP, type, "CATIME");

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (g_AppConfig.notification.display.type == type);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_TYPE", typeStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    if (!configMatches &&
        !UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr)) {
        return;
    }
    
    g_AppConfig.notification.display.type = type;
}


/**
 * @brief Write notification disabled setting to config file
 */
void WriteConfigNotificationDisabled(BOOL disabled) {
    disabled = disabled ? TRUE : FALSE;
    const char* disabledStr = disabled ? "TRUE" : "FALSE";

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (g_AppConfig.notification.display.disabled == disabled);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_DISABLED", disabledStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    if (!configMatches &&
        !UpdateConfigBoolAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", disabled)) {
        return;
    }

    g_AppConfig.notification.display.disabled = disabled;
}


/**
 * @brief Write notification sound file path to config
 */
void WriteConfigNotificationSound(const char* sound_file) {
    if (!sound_file) return;

    char clean_path[MAX_PATH] = {0};
    char to_write[MAX_PATH] = {0};
    BuildNotificationSoundConfigValue(sound_file, clean_path, sizeof(clean_path),
                                      to_write, sizeof(to_write));

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches =
        strcmp(g_AppConfig.notification.sound.sound_file, clean_path) == 0;
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_SOUND_FILE", to_write);
    if (runtimeMatches && configMatches) {
        return;
    }

    if (!configMatches &&
        !UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION,
                                    "NOTIFICATION_SOUND_FILE", to_write)) {
        return;
    }

    strncpy(g_AppConfig.notification.sound.sound_file, clean_path, sizeof(g_AppConfig.notification.sound.sound_file) - 1);
    g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
}

BOOL WriteConfigNotificationSettings(const char* timeout_msg, int timeout_ms,
                                     int opacity, NotificationType type,
                                     int corner_radius, BOOL disabled, const char* sound_file,
                                     int volume) {
    if (!timeout_msg) timeout_msg = "";
    if (timeout_ms < 0) timeout_ms = 0;
    if (opacity < 1) opacity = 1;
    if (opacity > 100) opacity = 100;
    if (corner_radius < MIN_NOTIFICATION_CORNER_RADIUS) {
        corner_radius = MIN_NOTIFICATION_CORNER_RADIUS;
    }
    if (corner_radius > MAX_NOTIFICATION_CORNER_RADIUS) {
        corner_radius = MAX_NOTIFICATION_CORNER_RADIUS;
    }
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME;
    }
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    char timeoutStr[32];
    char opacityStr[32];
    char radiusStr[32];
    char volumeStr[32];
    if (snprintf(timeoutStr, sizeof(timeoutStr), "%d", timeout_ms) < 0 ||
        snprintf(opacityStr, sizeof(opacityStr), "%d", opacity) < 0 ||
        snprintf(radiusStr, sizeof(radiusStr), "%d", corner_radius) < 0 ||
        snprintf(volumeStr, sizeof(volumeStr), "%d", volume) < 0) {
        return FALSE;
    }

    char cleanSoundPath[MAX_PATH] = {0};
    char soundConfigValue[MAX_PATH] = {0};
    BuildNotificationSoundConfigValue(sound_file, cleanSoundPath, sizeof(cleanSoundPath),
                                      soundConfigValue, sizeof(soundConfigValue));

    const char* typeStr = EnumToString(NOTIFICATION_TYPE_MAP, type, "CATIME");
    const char* disabledStr = disabled ? "TRUE" : "FALSE";

    BOOL runtimeMatches =
        strcmp(g_AppConfig.notification.messages.timeout_message, timeout_msg) == 0 &&
        g_AppConfig.notification.display.timeout_ms == timeout_ms &&
        g_AppConfig.notification.display.max_opacity == opacity &&
        g_AppConfig.notification.display.corner_radius == corner_radius &&
        g_AppConfig.notification.display.type == type &&
        g_AppConfig.notification.display.disabled == disabled &&
        strcmp(g_AppConfig.notification.sound.sound_file, cleanSoundPath) == 0 &&
        g_AppConfig.notification.sound.volume == volume;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    if (runtimeMatches &&
        NotificationIniValueMatches(config_path, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_TIMEOUT_MS", timeoutStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_MAX_OPACITY", opacityStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_CORNER_RADIUS", radiusStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_TYPE", typeStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_DISABLED", disabledStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_SOUND_FILE", soundConfigValue) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_SOUND_VOLUME", volumeStr)) {
        return TRUE;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeoutStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", opacityStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_CORNER_RADIUS", radiusStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", disabledStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", soundConfigValue},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", volumeStr},
    };
    if (!WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return FALSE;
    }

    strncpy(g_AppConfig.notification.messages.timeout_message, timeout_msg,
            sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    g_AppConfig.notification.display.timeout_ms = timeout_ms;
    g_AppConfig.notification.display.max_opacity = opacity;
    g_AppConfig.notification.display.corner_radius = corner_radius;
    g_AppConfig.notification.display.type = type;
    g_AppConfig.notification.display.disabled = disabled;
    strncpy(g_AppConfig.notification.sound.sound_file, cleanSoundPath,
            sizeof(g_AppConfig.notification.sound.sound_file) - 1);
    g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
    g_AppConfig.notification.sound.volume = volume;
    return TRUE;
}


void WriteConfigNotificationVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    char volumeStr[32];
    if (snprintf(volumeStr, sizeof(volumeStr), "%d", volume) < 0) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = (g_AppConfig.notification.sound.volume == volume);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_SOUND_VOLUME", volumeStr);
    if (runtimeMatches && configMatches) {
        return;
    }

    if (!configMatches &&
        !UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", volume)) {
        return;
    }

    g_AppConfig.notification.sound.volume = volume;
}

BOOL WriteConfigNotificationWindow(int x, int y, int width, int height) {
    char xStr[32];
    char yStr[32];
    char widthStr[32];
    char heightStr[32];
    if (snprintf(xStr, sizeof(xStr), "%d", x) < 0 ||
        snprintf(yStr, sizeof(yStr), "%d", y) < 0 ||
        snprintf(widthStr, sizeof(widthStr), "%d", width) < 0 ||
        snprintf(heightStr, sizeof(heightStr), "%d", height) < 0) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches =
        g_AppConfig.notification.display.window_x == x &&
        g_AppConfig.notification.display.window_y == y &&
        g_AppConfig.notification.display.window_width == width &&
        g_AppConfig.notification.display.window_height == height;
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_WINDOW_X", xStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_WINDOW_Y", yStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_WINDOW_WIDTH", widthStr) &&
        NotificationIniValueMatches(config_path, "NOTIFICATION_WINDOW_HEIGHT", heightStr);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", xStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", yStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", widthStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", heightStr},
    };
    if (!WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return FALSE;
    }

    g_AppConfig.notification.display.window_x = x;
    g_AppConfig.notification.display.window_y = y;
    g_AppConfig.notification.display.window_width = width;
    g_AppConfig.notification.display.window_height = height;
    return TRUE;
}
