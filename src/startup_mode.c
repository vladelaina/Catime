/**
 * @file startup_mode.c
 * @brief Data-driven application of the configured startup timer mode
 */
#include "startup_internal.h"

#include "config.h"
#include "log.h"
#include "timer/main_timer.h"
#include "timer/timer.h"
#include "timer/timer_events.h"

#include <string.h>

#define STARTUP_MODE_MAX_LEN 20
#define MODE_NAME_COUNT_UP "COUNT_UP"
#define MODE_NAME_SHOW_TIME "SHOW_TIME"
#define MODE_NAME_NO_DISPLAY "NO_DISPLAY"
#define MODE_NAME_DEFAULT "DEFAULT"

typedef struct {
    const char* name;
    BOOL showCurrentTime;
    BOOL countUp;
    BOOL enableTimer;
    int totalTime;
} StartupModeConfig;

static const StartupModeConfig STARTUP_MODES[] = {
    {MODE_NAME_COUNT_UP, FALSE, TRUE, TRUE, 0},
    {MODE_NAME_SHOW_TIME, TRUE, FALSE, TRUE, 0},
    {MODE_NAME_NO_DISPLAY, FALSE, FALSE, FALSE, 0},
    {MODE_NAME_DEFAULT, FALSE, FALSE, TRUE, -1},
};

static const StartupModeConfig* FindMode(const char* name) {
    size_t count = sizeof(STARTUP_MODES) / sizeof(STARTUP_MODES[0]);
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(name, STARTUP_MODES[i].name) == 0) {
            return &STARTUP_MODES[i];
        }
    }
    return NULL;
}

static const StartupModeConfig* GetDefaultMode(void) {
    const StartupModeConfig* mode = FindMode(MODE_NAME_DEFAULT);
    return mode ? mode : &STARTUP_MODES[0];
}

static BOOL ReadConfiguredMode(char modeName[STARTUP_MODE_MAX_LEN]) {
    char configPath[MAX_PATH] = {0};

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') return FALSE;
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "", modeName,
                  STARTUP_MODE_MAX_LEN, configPath);
    return modeName[0] != '\0';
}

static void ApplyMode(HWND hwnd, const StartupModeConfig* mode) {
    int64_t now;

    CLOCK_SHOW_CURRENT_TIME = mode->showCurrentTime;
    CLOCK_COUNT_UP = mode->countUp;
    CLOCK_TOTAL_TIME = mode->totalTime == -1
        ? g_AppConfig.timer.default_start_time
        : mode->totalTime;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    now = GetAbsoluteTimeMs();
    if (mode->countUp) {
        g_start_time = now;
    } else if (CLOCK_TOTAL_TIME > 0) {
        g_target_end_time = now + ((int64_t)CLOCK_TOTAL_TIME * 1000);
    }
    ResetMillisecondAccumulator();

    if (mode->enableTimer) {
        MainTimer_Start(hwnd, GetTimerInterval());
    } else {
        MainTimer_Stop();
    }
}

void StartupMode_ApplyConfigured(HWND hwnd) {
    char modeName[STARTUP_MODE_MAX_LEN] = {0};
    const StartupModeConfig* mode = NULL;

    if (ReadConfiguredMode(modeName)) {
        mode = FindMode(modeName);
        if (!mode) {
            LOG_WARNING("Unknown startup mode '%s', using default", modeName);
        }
    }
    ApplyMode(hwnd, mode ? mode : GetDefaultMode());
}
