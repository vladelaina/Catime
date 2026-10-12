#include "config/config_applier.h"
#include "config.h"
#include "config/config_defaults.h"
#include "pomodoro_phase_action.h"
#include "log.h"
#include <string.h>

static void ApplyDefaultPomodoroTimes(void) {
    static const int defaultTimes[] = {
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_SHORT_BREAK,
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_LONG_BREAK
    };
    int count = (int)_countof(defaultTimes);
    if (count > (int)_countof(g_AppConfig.pomodoro.times)) {
        count = (int)_countof(g_AppConfig.pomodoro.times);
    }
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    memcpy(g_AppConfig.pomodoro.times, defaultTimes, (size_t)count * sizeof(defaultTimes[0]));
    g_AppConfig.pomodoro.times_count = count;
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    strncpy_s(g_AppConfig.pomodoro.work_actions, sizeof(g_AppConfig.pomodoro.work_actions),
              DEFAULT_POMODORO_WORK_ACTIONS, _TRUNCATE);
    strncpy_s(g_AppConfig.pomodoro.short_break_actions,
              sizeof(g_AppConfig.pomodoro.short_break_actions), DEFAULT_POMODORO_SHORT_BREAK_ACTIONS, _TRUNCATE);
    strncpy_s(g_AppConfig.pomodoro.long_break_actions,
              sizeof(g_AppConfig.pomodoro.long_break_actions), DEFAULT_POMODORO_LONG_BREAK_ACTIONS, _TRUNCATE);
}

static int ClampPomodoroLoopCountForApply(int loopCount) {
    if (loopCount < MIN_POMODORO_LOOP_COUNT) return MIN_POMODORO_LOOP_COUNT;
    if (loopCount > MAX_POMODORO_LOOP_COUNT) return MAX_POMODORO_LOOP_COUNT;
    return loopCount;
}

void ApplyPomodoroSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    int loopCount = ClampPomodoroLoopCountForApply(snapshot->pomodoroLoopCount);
    int timesCount = snapshot->pomodoroTimesCount;
    if (timesCount <= 0 || timesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        LOG_WARNING("Invalid Pomodoro times count %d while applying config, using defaults",
                    timesCount);
        ApplyDefaultPomodoroTimes();
        g_AppConfig.pomodoro.loop_count = loopCount;
        return;
    }
    for (int i = 0; i < timesCount; i++) {
        if (snapshot->pomodoroTimes[i] <= 0 ||
            snapshot->pomodoroTimes[i] > MAX_POMODORO_OPTION_SECONDS) {
            LOG_WARNING("Invalid Pomodoro interval %d while applying config, using defaults",
                        snapshot->pomodoroTimes[i]);
            ApplyDefaultPomodoroTimes();
            g_AppConfig.pomodoro.loop_count = loopCount;
            return;
        }
    }
    g_AppConfig.pomodoro.times_count = timesCount;
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    for (int i = 0; i < timesCount; i++) {
        g_AppConfig.pomodoro.times[i] = snapshot->pomodoroTimes[i];
    }
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    if (timesCount > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    if (timesCount > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    PomodoroPhaseAction_NormalizeChain(snapshot->pomodoroWorkActions,
                                       g_AppConfig.pomodoro.work_actions,
                                       sizeof(g_AppConfig.pomodoro.work_actions));
    PomodoroPhaseAction_NormalizeChain(snapshot->pomodoroShortBreakActions,
                                       g_AppConfig.pomodoro.short_break_actions,
                                       sizeof(g_AppConfig.pomodoro.short_break_actions));
    PomodoroPhaseAction_NormalizeChain(snapshot->pomodoroLongBreakActions,
                                       g_AppConfig.pomodoro.long_break_actions,
                                       sizeof(g_AppConfig.pomodoro.long_break_actions));
    g_AppConfig.pomodoro.loop_count = loopCount;
}
