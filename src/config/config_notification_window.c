#include "config.h"
#include "config/config_defaults.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
static BOOL NotificationIniValueMatches(const char* path, const char* key,
                                        const char* expected) {
    char current[2048] = {0};
    if (!ReadIniStringExact(INI_SECTION_NOTIFICATION, key, "", current,
                            sizeof(current), path)) return FALSE;
    return strcmp(current, expected ? expected : "") == 0;
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
