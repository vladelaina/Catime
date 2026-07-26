#include "config_misc_internal.h"

static BOOL BuildPomodoroTimesString(
    const int* times, int count, char* buffer, size_t bufferSize) {
    if (!times || count <= 0 || count > MAX_POMODORO_TIMES ||
        !buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = '\0';
    size_t offset = 0;
    for (int i = 0; i < count; ++i) {
        if (times[i] <= 0 || times[i] > MAX_POMODORO_OPTION_SECONDS) {
            return FALSE;
        }
        int written = snprintf(
            buffer + offset, bufferSize - offset, "%s%d",
            i > 0 ? "," : "", times[i]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            buffer[0] = '\0';
            return FALSE;
        }
        offset += (size_t)written;
    }
    return TRUE;
}

static BOOL PomodoroTimesStateMatches(const int* times, int count) {
    if (!times || count <= 0 || count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times) ||
        g_AppConfig.pomodoro.times_count != count) {
        return FALSE;
    }
    for (int i = 0; i < count; ++i) {
        if (g_AppConfig.pomodoro.times[i] != times[i]) {
            return FALSE;
        }
    }
    if (g_AppConfig.pomodoro.work_time != times[0]) return FALSE;
    if (count > 1 && g_AppConfig.pomodoro.short_break != times[1]) return FALSE;
    if (count > 2 && g_AppConfig.pomodoro.long_break != times[2]) return FALSE;
    return TRUE;
}

static void UpdatePomodoroTimesState(const int* times, int count) {
    if (!times || count <= 0 || count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return;
    }
    g_AppConfig.pomodoro.times_count = count;
    ZeroMemory(g_AppConfig.pomodoro.times,
               sizeof(g_AppConfig.pomodoro.times));
    for (int i = 0; i < count; ++i) {
        g_AppConfig.pomodoro.times[i] = times[i];
    }
    g_AppConfig.pomodoro.work_time = times[0];
    if (count > 1) g_AppConfig.pomodoro.short_break = times[1];
    if (count > 2) g_AppConfig.pomodoro.long_break = times[2];
}

static BOOL WritePomodoroTimeOptionsStringIfChanged(
    const int* times, int count) {
    if (!times || count <= 0 || count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return FALSE;
    }

    char timesString[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    if (!BuildPomodoroTimesString(
            times, count, timesString, sizeof(timesString))) {
        return FALSE;
    }

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentValue[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL currentValueComplete = ReadIniStringExact(
        INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "",
        currentValue, sizeof(currentValue), configPath);
    BOOL runtimeMatches = PomodoroTimesStateMatches(times, count);
    BOOL configMatches = currentValueComplete &&
                         strcmp(currentValue, timesString) == 0;
    if (runtimeMatches && configMatches) {
        return TRUE;
    }
    if (!configMatches && !WriteIniString(
            INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS",
            timesString, configPath)) {
        return FALSE;
    }
    UpdatePomodoroTimesState(times, count);
    return TRUE;
}

BOOL WriteConfigPomodoroTimes(int work, int shortBreak, int longBreak) {
    const int times[] = {work, shortBreak, longBreak};
    return WritePomodoroTimeOptionsStringIfChanged(
        times, (int)_countof(times));
}

BOOL WriteConfigPomodoroSettings(
    int work, int shortBreak, int longBreak, int longBreak2) {
    const int times[] = {work, shortBreak, longBreak, longBreak2};
    return WritePomodoroTimeOptionsStringIfChanged(
        times, (int)_countof(times));
}

BOOL WriteConfigPomodoroLoopCount(int loopCount) {
    if (loopCount < MIN_POMODORO_LOOP_COUNT) loopCount = MIN_POMODORO_LOOP_COUNT;
    if (loopCount > MAX_POMODORO_LOOP_COUNT) loopCount = MAX_POMODORO_LOOP_COUNT;

    char loopCountString[32];
    if (snprintf(loopCountString, sizeof(loopCountString),
                 "%d", loopCount) < 0) {
        return FALSE;
    }
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentValue[32] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", "",
                  currentValue, sizeof(currentValue), configPath);
    BOOL runtimeMatches = g_AppConfig.pomodoro.loop_count == loopCount;
    BOOL configMatches = strcmp(currentValue, loopCountString) == 0;
    if (runtimeMatches && configMatches) return TRUE;
    if (!configMatches && !WriteIniInt(
            INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT",
            loopCount, configPath)) {
        return FALSE;
    }
    g_AppConfig.pomodoro.loop_count = loopCount;
    return TRUE;
}

BOOL WriteConfigPomodoroTimeOptions(const int* times, int count) {
    return WritePomodoroTimeOptionsStringIfChanged(times, count);
}
