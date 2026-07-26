/**
 * @file window_config_handlers_pomodoro.c
 * @brief Reloads Pomodoro configuration.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "log.h"
#include "window_procedure/window_utils.h"

#include <string.h>

LRESULT HandleAppPomodoroChanged(HWND hwnd) {
    (void)hwnd;

    /* Pomodoro time options */
    char buf[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL optionsComplete = ReadIniStringExact(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_OPTIONS,
                                              DEFAULT_POMODORO_OPTIONS_INI, buf,
                                              sizeof(buf), GetCachedConfigPath());
    int tmp[MAX_POMODORO_TIMES] = {0};
    int cnt = 0;
    if (!optionsComplete) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Pomodoro intervals config is too long during reload; keeping current intervals");
    } else if (WindowConfigInternal_ParsePomodoroTimeOptions(buf, tmp, &cnt) &&
               cnt <= (int)_countof(g_AppConfig.pomodoro.times) &&
               (cnt != g_AppConfig.pomodoro.times_count ||
                memcmp(tmp, g_AppConfig.pomodoro.times, (size_t)cnt * sizeof(tmp[0])) != 0)) {
        g_AppConfig.pomodoro.times_count = cnt;
        ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
        memcpy(g_AppConfig.pomodoro.times, tmp, (size_t)cnt * sizeof(tmp[0]));

        g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
        if (cnt > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
        if (cnt > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    }

    /* Loop count */
    g_AppConfig.pomodoro.loop_count = WindowConfigInternal_ClampInt(
        CFG_KEY_POMODORO_LOOP_COUNT,
        ReadConfigInt(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_LOOP_COUNT,
                      DEFAULT_POMODORO_LOOP_COUNT),
        MIN_POMODORO_LOOP_COUNT, MAX_POMODORO_LOOP_COUNT);

    return 0;
}
