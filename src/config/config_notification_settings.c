#include "config.h"
#include "config/config_defaults.h"
#include "multi_window.h"

#include <stdio.h>
#include <string.h>

static const char* NotificationTypeToString(NotificationType type) {
    switch (type) {
        case NOTIFICATION_TYPE_SYSTEM_MODAL: return "SYSTEM_MODAL";
        case NOTIFICATION_TYPE_OS: return "OS";
        case NOTIFICATION_TYPE_CATIME:
        default: return "CATIME";
    }
}

static void BuildSoundValue(const char* soundFile, char* cleanPath,
                            size_t cleanSize, char* configValue,
                            size_t configSize) {
    cleanPath[0] = '\0';
    configValue[0] = '\0';
    if (!soundFile) return;

    size_t index = 0;
    for (const char* source = soundFile; *source && index + 1 < cleanSize;
         ++source) {
        if (*source != '=') cleanPath[index++] = *source;
    }
    cleanPath[index] = '\0';

    char localAppData[MAX_PATH] = {0};
    if (GetEffectiveLocalAppDataPath(localAppData, sizeof(localAppData))) {
        size_t rootLength = strlen(localAppData);
        if (_strnicmp(cleanPath, localAppData, rootLength) == 0) {
            const char* suffix = cleanPath + rootLength;
            if (*suffix == '\\') ++suffix;
            int written = snprintf(configValue, configSize,
                                   "%%LOCALAPPDATA%%\\%s", suffix);
            if (written >= 0 && (size_t)written < configSize) return;
        }
    }
    strncpy_s(configValue, configSize, cleanPath, _TRUNCATE);
}

BOOL WriteConfigNotificationSettings(const char* timeoutMessage, int timeoutMs,
                                     int opacity, NotificationType type,
                                     int cornerRadius, int fontPercent,
                                     BOOL disabled, const char* soundFile,
                                     int volume, BOOL useForPomodoro) {
    if (!timeoutMessage) timeoutMessage = "";
    if (timeoutMs < 0) timeoutMs = 0;
    if (opacity < MIN_VISIBLE_OPACITY) opacity = MIN_VISIBLE_OPACITY;
    if (opacity > 100) opacity = 100;
    if (cornerRadius < MIN_NOTIFICATION_CORNER_RADIUS) cornerRadius = MIN_NOTIFICATION_CORNER_RADIUS;
    if (cornerRadius > MAX_NOTIFICATION_CORNER_RADIUS) cornerRadius = MAX_NOTIFICATION_CORNER_RADIUS;
    if (fontPercent < MIN_NOTIFICATION_FONT_SIZE) fontPercent = MIN_NOTIFICATION_FONT_SIZE;
    if (fontPercent > MAX_NOTIFICATION_FONT_SIZE) fontPercent = MAX_NOTIFICATION_FONT_SIZE;
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) type = NOTIFICATION_TYPE_CATIME;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    disabled = disabled ? TRUE : FALSE;
    useForPomodoro = useForPomodoro ? TRUE : FALSE;

    char cleanSoundPath[MAX_PATH] = {0};
    char soundConfigValue[MAX_PATH] = {0};
    BuildSoundValue(soundFile, cleanSoundPath, sizeof(cleanSoundPath),
                    soundConfigValue, sizeof(soundConfigValue));
    if (MultiWindow_IsSecondary()) {
        strncpy_s(g_AppConfig.notification.messages.timeout_message,
                  sizeof(g_AppConfig.notification.messages.timeout_message), timeoutMessage, _TRUNCATE);
        g_AppConfig.notification.messages.use_for_pomodoro = useForPomodoro;
        g_AppConfig.notification.display.timeout_ms = timeoutMs;
        g_AppConfig.notification.display.max_opacity = opacity;
        g_AppConfig.notification.display.corner_radius = cornerRadius;
        g_AppConfig.notification.display.font_size = fontPercent;
        g_AppConfig.notification.display.type = type;
        g_AppConfig.notification.display.disabled = disabled;
        strncpy_s(g_AppConfig.notification.sound.sound_file,
                  sizeof(g_AppConfig.notification.sound.sound_file), cleanSoundPath, _TRUNCATE);
        g_AppConfig.notification.sound.volume = volume;
        return TRUE;
    }

    char timeoutStr[32], opacityStr[32], radiusStr[32], fontPercentStr[32], volumeStr[32];
    if (snprintf(timeoutStr, sizeof(timeoutStr), "%d", timeoutMs) < 0 ||
        snprintf(opacityStr, sizeof(opacityStr), "%d", opacity) < 0 ||
        snprintf(radiusStr, sizeof(radiusStr), "%d", cornerRadius) < 0 ||
        snprintf(fontPercentStr, sizeof(fontPercentStr), "%d", fontPercent) < 0 ||
        snprintf(volumeStr, sizeof(volumeStr), "%d", volume) < 0) return FALSE;

    const char* typeStr = NotificationTypeToString(type);
    const char* disabledStr = disabled ? "TRUE" : "FALSE";
    char configPath[MAX_PATH];
    GetConfigPath(configPath, sizeof(configPath));
    const IniKeyValue updates[] = {
        {INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeoutMessage},
        {INI_SECTION_NOTIFICATION, NOTIFICATION_USE_FOR_POMODORO_KEY,
         useForPomodoro ? "TRUE" : "FALSE"},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", timeoutStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", opacityStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_CORNER_RADIUS", radiusStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_FONT_SIZE", fontPercentStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", disabledStr},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", soundConfigValue},
        {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", volumeStr},
    };
    if (!WriteIniMultipleAtomic(configPath, updates, _countof(updates))) return FALSE;

    strncpy_s(g_AppConfig.notification.messages.timeout_message,
              sizeof(g_AppConfig.notification.messages.timeout_message), timeoutMessage, _TRUNCATE);
    g_AppConfig.notification.messages.use_for_pomodoro = useForPomodoro;
    g_AppConfig.notification.display.timeout_ms = timeoutMs;
    g_AppConfig.notification.display.max_opacity = opacity;
    g_AppConfig.notification.display.corner_radius = cornerRadius;
    g_AppConfig.notification.display.font_size = fontPercent;
    g_AppConfig.notification.display.type = type;
    g_AppConfig.notification.display.disabled = disabled;
    strncpy_s(g_AppConfig.notification.sound.sound_file,
              sizeof(g_AppConfig.notification.sound.sound_file), cleanSoundPath, _TRUNCATE);
    g_AppConfig.notification.sound.volume = volume;
    return TRUE;
}
