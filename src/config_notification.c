/**
 * @file config_notification.c
 * @brief Notification configuration management
 * 
 * Manages notification settings including messages, type, timeout, opacity, sound, and volume.
 */
#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/** External references to notification globals */
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];
extern int NOTIFICATION_TIMEOUT_MS;
extern int NOTIFICATION_MAX_OPACITY;
extern NotificationType NOTIFICATION_TYPE;
extern BOOL NOTIFICATION_DISABLED;
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];
extern int NOTIFICATION_SOUND_VOLUME;

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
 * @brief Read notification message texts from config
 */
void ReadNotificationMessagesConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE, 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", DEFAULT_POMODORO_MESSAGE, 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", DEFAULT_POMODORO_COMPLETE_MSG,
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
}


/**
 * @brief Update notification message texts in config
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Batch update for better performance */
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", pomodoro_msg, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", cycle_complete_msg, config_path);
}


/**
 * @brief Read notification timeout setting from config file
 */
void ReadNotificationTimeoutConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    NOTIFICATION_TIMEOUT_MS = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
}


/**
 * @brief Write notification timeout setting to config file
 */
void WriteConfigNotificationTimeout(int timeout_ms) {
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeout_ms);
}


/**
 * @brief Read notification opacity setting from config file
 */
void ReadNotificationOpacityConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    int opacity = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    /** Validate opacity range (1-100) */
    if (opacity >= 1 && opacity <= 100) {
        NOTIFICATION_MAX_OPACITY = opacity;
    } else {
        NOTIFICATION_MAX_OPACITY = 95;
    }
}


/**
 * @brief Write notification opacity setting to config file
 */
void WriteConfigNotificationOpacity(int opacity) {
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", opacity);
}


/**
 * @brief Read notification type setting from config file
 */
void ReadNotificationTypeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    char typeStr[32];
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", 
                 typeStr, sizeof(typeStr), config_path);
    
    NOTIFICATION_TYPE = StringToEnum(NOTIFICATION_TYPE_MAP, typeStr, NOTIFICATION_TYPE_CATIME);
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
    UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr);
}


/**
 * @brief Read notification disabled setting from config
 */
void ReadNotificationDisabledConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
}


/**
 * @brief Write notification disabled setting to config file
 */
void WriteConfigNotificationDisabled(BOOL disabled) {
    UpdateConfigBoolAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", disabled);
}


/**
 * @brief Read notification sound file path from config
 */
void ReadNotificationSoundConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    ReadIniString(INI_SECTION_NOTIFICATION,
                 "NOTIFICATION_SOUND_FILE",
                 "",
                 NOTIFICATION_SOUND_FILE,
                 MAX_PATH,
                 config_path);

    /** Normalize %LOCALAPPDATA% placeholder to absolute path */
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        const char* varToken = "%LOCALAPPDATA%";
        size_t tokenLen = strlen(varToken);
        if (_strnicmp(NOTIFICATION_SOUND_FILE, varToken, (int)tokenLen) == 0) {
            const char* localAppData = getenv("LOCALAPPDATA");
            if (localAppData && localAppData[0] != '\0') {
                char resolved[MAX_PATH] = {0};
                snprintf(resolved, sizeof(resolved), "%s%s",
                         localAppData,
                         NOTIFICATION_SOUND_FILE + tokenLen);
                strncpy(NOTIFICATION_SOUND_FILE, resolved, MAX_PATH - 1);
                NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
            }
        }
    }
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

    UpdateConfigKeyValueAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", to_write);
}


void ReadNotificationVolumeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    int volume = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    /** Validate volume range (0-100) */
    if (volume >= 0 && volume <= 100) {
        NOTIFICATION_SOUND_VOLUME = volume;
    } else {
        NOTIFICATION_SOUND_VOLUME = 100;
    }
}


void WriteConfigNotificationVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", volume);
}

