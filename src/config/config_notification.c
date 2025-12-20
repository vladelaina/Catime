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

static int StringToEnum(const EnumStrMap* map, const char* str, int defaultVal) {
    if (!map || !str) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (strcmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}

/**
 * @brief Update notification message texts in config
 */
void WriteConfigNotificationMessages(const char* timeout_msg) {
    strncpy(g_AppConfig.notification.messages.timeout_message, timeout_msg, sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg, config_path);
}


/**
 * @brief Write notification timeout setting to config file
 */
void WriteConfigNotificationTimeout(int timeout_ms) {
    g_AppConfig.notification.display.timeout_ms = timeout_ms;
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeout_ms);
}


/**
 * @brief Write notification opacity setting to config file
 */
void WriteConfigNotificationOpacity(int opacity) {
    g_AppConfig.notification.display.max_opacity = opacity;
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", opacity);
}


/**
 * @brief Write notification type setting to config file
 */
void WriteConfigNotificationType(NotificationType type) {
    /** Validate notification type range */
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME;
    }
    
    g_AppConfig.notification.display.type = type;
    const char* typeStr = EnumToString(NOTIFICATION_TYPE_MAP, type, "CATIME");
    UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr);
}


/**
 * @brief Write notification disabled setting to config file
 */
void WriteConfigNotificationDisabled(BOOL disabled) {
    g_AppConfig.notification.display.disabled = disabled;
    UpdateConfigBoolAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", disabled);
}


/**
 * @brief Write notification sound file path to config
 */
void WriteConfigNotificationSound(const char* sound_file) {
    if (!sound_file) return;
    
    /** Clean path by removing equals signs */
    char clean_path[MAX_PATH] = {0};
    const char* src = sound_file;
    char* dst = clean_path;
    
    while (*src && (dst - clean_path) < (MAX_PATH - 1)) {
        if (*src != '=') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    /** If path is under %LOCALAPPDATA%\Catime\resources\audio, store with placeholder */
    char to_write[MAX_PATH] = {0};
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData && _strnicmp(clean_path, localAppData, strlen(localAppData)) == 0) {
        const char* rest = clean_path + strlen(localAppData);
        if (*rest == '\\') rest++;
        snprintf(to_write, sizeof(to_write), "%%LOCALAPPDATA%%\\%s", rest);
    } else {
        strncpy(to_write, clean_path, sizeof(to_write) - 1);
    }

    strncpy(g_AppConfig.notification.sound.sound_file, clean_path, sizeof(g_AppConfig.notification.sound.sound_file) - 1);
    g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
    
    UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", to_write);
}


void WriteConfigNotificationVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    g_AppConfig.notification.sound.volume = volume;
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", volume);
}

void WriteConfigNotificationWindow(int x, int y, int width, int height) {
    g_AppConfig.notification.display.window_x = x;
    g_AppConfig.notification.display.window_y = y;
    g_AppConfig.notification.display.window_width = width;
    g_AppConfig.notification.display.window_height = height;
    
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", x);
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", y);
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", width);
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", height);
}

