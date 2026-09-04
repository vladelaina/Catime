#include "config.h"
#include "config/config_defaults.h"
#include "multi_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
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
    char localAppData[MAX_PATH] = {0};
    if (GetEffectiveLocalAppDataPath(localAppData, sizeof(localAppData))) {
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

/*
 * A --new-window timer is an independent timer process.  It inherits the
 * persisted notification defaults at startup, but changes made from that
 * window must stay local so another running timer cannot overwrite them.
 */
static BOOL NotificationSettingsAreProcessLocal(void) {
    return MultiWindow_IsSecondary();
}

BOOL WriteConfigNotificationMessages(const char* timeoutMessage) {
    if (!timeoutMessage) timeoutMessage = "";
    if (NotificationSettingsAreProcessLocal()) {
        strncpy_s(g_AppConfig.notification.messages.timeout_message,
                  sizeof(g_AppConfig.notification.messages.timeout_message),
                  timeoutMessage, _TRUNCATE);
        return TRUE;
    }
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    BOOL runtimeMatches =
        strcmp(g_AppConfig.notification.messages.timeout_message, timeoutMessage) == 0;
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeoutMessage);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }
    if (!configMatches &&
        !WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT",
                        timeoutMessage, config_path)) {
        return FALSE;
    }
    strncpy(g_AppConfig.notification.messages.timeout_message, timeoutMessage, sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    return TRUE;
}
BOOL WriteConfigNotificationTimeout(int timeoutMs) {
    if (timeoutMs < 0) timeoutMs = 0;
    if (NotificationSettingsAreProcessLocal()) {
        g_AppConfig.notification.display.timeout_ms = timeoutMs;
        return TRUE;
    }
    char timeoutStr[32];
    if (snprintf(timeoutStr, sizeof(timeoutStr), "%d", timeoutMs) < 0) {
        return FALSE;
    }
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    BOOL runtimeMatches = (g_AppConfig.notification.display.timeout_ms == timeoutMs);
    BOOL configMatches =
        NotificationIniValueMatches(config_path, "NOTIFICATION_TIMEOUT_MS", timeoutStr);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }
    if (!configMatches &&
        !UpdateConfigIntAtomic(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeoutMs)) {
        return FALSE;
    }
    g_AppConfig.notification.display.timeout_ms = timeoutMs;
    return TRUE;
}
BOOL WriteConfigNotificationOpacity(int opacity) {
    if (opacity < MIN_VISIBLE_OPACITY) opacity = MIN_VISIBLE_OPACITY;
    if (opacity > 100) opacity = 100;
    if (NotificationSettingsAreProcessLocal()) {
        g_AppConfig.notification.display.max_opacity = opacity;
        return TRUE;
    }
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
void WriteConfigNotificationType(NotificationType type) {
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME;
    }
    if (NotificationSettingsAreProcessLocal()) {
        g_AppConfig.notification.display.type = type;
        return;
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
void WriteConfigNotificationDisabled(BOOL disabled) {
    disabled = disabled ? TRUE : FALSE;
    if (NotificationSettingsAreProcessLocal()) {
        g_AppConfig.notification.display.disabled = disabled;
        return;
    }
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
void WriteConfigNotificationSound(const char* soundFile) {
    if (!soundFile) return;
    char clean_path[MAX_PATH] = {0};
    char to_write[MAX_PATH] = {0};
    BuildNotificationSoundConfigValue(soundFile, clean_path, sizeof(clean_path),
                                      to_write, sizeof(to_write));
    if (NotificationSettingsAreProcessLocal()) {
        strncpy_s(g_AppConfig.notification.sound.sound_file,
                  sizeof(g_AppConfig.notification.sound.sound_file),
                  clean_path, _TRUNCATE);
        return;
    }
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
