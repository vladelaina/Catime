/**
 * @file window_config_handlers_notification.c
 * @brief Reloads notification configuration.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "notification.h"
#include "multi_window.h"
#include "window_procedure/window_utils.h"

#include <string.h>

LRESULT HandleAppNotificationChanged(HWND hwnd) {
    (void)hwnd;

    /* Secondary timer processes keep their notification preferences local. */
    if (MultiWindow_IsSecondary()) return 0;

    /* Reload notification settings from INI file for hot-reload support */
    int timeoutMs = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000);
    if (timeoutMs < 0 || timeoutMs > 60000) {
        timeoutMs = 3000;
    }
    g_AppConfig.notification.display.timeout_ms = timeoutMs;

    g_AppConfig.notification.display.max_opacity = WindowConfigInternal_ClampInt(
        "NOTIFICATION_MAX_OPACITY",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY",
                      DEFAULT_NOTIFICATION_MAX_OPACITY),
        MIN_VISIBLE_OPACITY, MAX_OPACITY);

    g_AppConfig.notification.display.corner_radius = WindowConfigInternal_ClampInt(
        "NOTIFICATION_CORNER_RADIUS",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_CORNER_RADIUS",
                      DEFAULT_NOTIFICATION_CORNER_RADIUS),
        MIN_NOTIFICATION_CORNER_RADIUS, MAX_NOTIFICATION_CORNER_RADIUS);

    g_AppConfig.notification.display.font_size = WindowConfigInternal_ClampInt(
        "NOTIFICATION_FONT_SIZE",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_FONT_SIZE",
                      DEFAULT_NOTIFICATION_FONT_SIZE),
        MIN_NOTIFICATION_FONT_SIZE, MAX_NOTIFICATION_FONT_SIZE);

    char typeBuf[32];
    ReadConfigStr(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", typeBuf, sizeof(typeBuf));
    if (_stricmp(typeBuf, "SYSTEM_MODAL") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (_stricmp(typeBuf, "OS") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_OS;
    } else {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_CATIME;
    }

    g_AppConfig.notification.display.disabled = ReadConfigBool(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE);

    g_AppConfig.notification.display.window_x = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", -1);
    g_AppConfig.notification.display.window_y = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", -1);
    g_AppConfig.notification.display.window_width = WindowConfigInternal_ClampNotificationWidth(
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", 0));
    g_AppConfig.notification.display.window_height = WindowConfigInternal_ClampNotificationHeight(
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", 0));

    char timeoutMessage[sizeof(g_AppConfig.notification.messages.timeout_message)] = {0};
    if (WindowConfigInternal_ReadStringExact(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT",
                                 DEFAULT_TIMEOUT_MESSAGE, timeoutMessage,
                                 sizeof(timeoutMessage))) {
        strncpy_s(g_AppConfig.notification.messages.timeout_message,
                  sizeof(g_AppConfig.notification.messages.timeout_message),
                  timeoutMessage, _TRUNCATE);
    }
    g_AppConfig.notification.messages.use_for_pomodoro = ReadConfigBool(
        INI_SECTION_NOTIFICATION, NOTIFICATION_USE_FOR_POMODORO_KEY, FALSE);

    char soundBuf[MAX_PATH] = {0};
    BOOL soundConfigComplete = WindowConfigInternal_ReadStringExact(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE",
        "", soundBuf, sizeof(soundBuf));
    if (soundConfigComplete && soundBuf[0] != '\0') {
        char resolved[MAX_PATH] = {0};
        if (ExpandEffectiveLocalAppDataPath(soundBuf, resolved, sizeof(resolved))) {
            strncpy(g_AppConfig.notification.sound.sound_file, resolved, MAX_PATH - 1);
        } else {
            strncpy(g_AppConfig.notification.sound.sound_file, soundBuf, MAX_PATH - 1);
        }
        g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    } else if (soundConfigComplete) {
        g_AppConfig.notification.sound.sound_file[0] = '\0';
    }

    int volume = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100);
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_AppConfig.notification.sound.volume = volume;

    return 0;
}
