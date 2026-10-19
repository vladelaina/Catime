#include "config.h"
#include "config/config_defaults.h"
#include "pomodoro_phase_action.h"
#include <string.h>

static const char* GetPhaseActionDefault(const char* key) {
    if (strcmp(key, "POMODORO_WORK_ACTIONS") == 0) {
        return DEFAULT_POMODORO_WORK_ACTIONS;
    }
    if (strcmp(key, "POMODORO_SHORT_BREAK_ACTIONS") == 0) {
        return DEFAULT_POMODORO_SHORT_BREAK_ACTIONS;
    }
    if (strcmp(key, "POMODORO_LONG_BREAK_ACTIONS") == 0) {
        return DEFAULT_POMODORO_LONG_BREAK_ACTIONS;
    }
    return NULL;
}

BOOL WriteConfigPomodoroPhaseActionKey(const char* key, const char* actions) {
    if (!key || !actions) {
        return FALSE;
    }

    const char* defaultActions = GetPhaseActionDefault(key);
    if (!defaultActions) return FALSE;

    char normalized[256];
    if (!PomodoroPhaseAction_NormalizeChain(actions, normalized, sizeof(normalized))) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[sizeof(normalized)] = {0};
    /* A missing key enables the default actions; it is not an explicit empty list. */
    BOOL currentValueComplete = ReadIniStringExact(INI_SECTION_POMODORO, key,
        defaultActions, currentValue, sizeof(currentValue), config_path);
    if (currentValueComplete && strcmp(currentValue, normalized) == 0) {
        if (strcmp(key, "POMODORO_WORK_ACTIONS") == 0) {
            strncpy_s(g_AppConfig.pomodoro.work_actions, sizeof(g_AppConfig.pomodoro.work_actions),
                      normalized, _TRUNCATE);
        } else if (strcmp(key, "POMODORO_SHORT_BREAK_ACTIONS") == 0) {
            strncpy_s(g_AppConfig.pomodoro.short_break_actions,
                      sizeof(g_AppConfig.pomodoro.short_break_actions),
                      normalized, _TRUNCATE);
        } else if (strcmp(key, "POMODORO_LONG_BREAK_ACTIONS") == 0) {
            strncpy_s(g_AppConfig.pomodoro.long_break_actions,
                      sizeof(g_AppConfig.pomodoro.long_break_actions),
                      normalized, _TRUNCATE);
        }
        return TRUE;
    }

    if (!WriteIniString(INI_SECTION_POMODORO, key, normalized, config_path)) {
        return FALSE;
    }

    if (strcmp(key, "POMODORO_WORK_ACTIONS") == 0) {
        strncpy_s(g_AppConfig.pomodoro.work_actions, sizeof(g_AppConfig.pomodoro.work_actions),
                  normalized, _TRUNCATE);
    } else if (strcmp(key, "POMODORO_SHORT_BREAK_ACTIONS") == 0) {
        strncpy_s(g_AppConfig.pomodoro.short_break_actions,
                  sizeof(g_AppConfig.pomodoro.short_break_actions),
                  normalized, _TRUNCATE);
    } else if (strcmp(key, "POMODORO_LONG_BREAK_ACTIONS") == 0) {
        strncpy_s(g_AppConfig.pomodoro.long_break_actions,
                  sizeof(g_AppConfig.pomodoro.long_break_actions),
                  normalized, _TRUNCATE);
    }

    return TRUE;
}

BOOL WriteConfigPomodoroPhaseActions(const char* workActions,
                                     const char* shortBreakActions,
                                     const char* longBreakActions) {
    if (!workActions || !shortBreakActions || !longBreakActions) {
        return FALSE;
    }

    char normalizedWork[256], normalizedShort[256], normalizedLong[256];
    if (!PomodoroPhaseAction_NormalizeChain(workActions, normalizedWork, sizeof(normalizedWork)) ||
        !PomodoroPhaseAction_NormalizeChain(shortBreakActions, normalizedShort, sizeof(normalizedShort)) ||
        !PomodoroPhaseAction_NormalizeChain(longBreakActions, normalizedLong, sizeof(normalizedLong))) {
        return FALSE;
    }
    workActions = normalizedWork;
    shortBreakActions = normalizedShort;
    longBreakActions = normalizedLong;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentWork[256] = {0};
    char currentShort[256] = {0};
    char currentLong[256] = {0};
    BOOL workComplete = ReadIniStringExact(INI_SECTION_POMODORO, "POMODORO_WORK_ACTIONS",
        DEFAULT_POMODORO_WORK_ACTIONS, currentWork, sizeof(currentWork), config_path);
    BOOL shortComplete = ReadIniStringExact(INI_SECTION_POMODORO, "POMODORO_SHORT_BREAK_ACTIONS",
        DEFAULT_POMODORO_SHORT_BREAK_ACTIONS, currentShort, sizeof(currentShort), config_path);
    BOOL longComplete = ReadIniStringExact(INI_SECTION_POMODORO, "POMODORO_LONG_BREAK_ACTIONS",
        DEFAULT_POMODORO_LONG_BREAK_ACTIONS, currentLong, sizeof(currentLong), config_path);

    BOOL configMatches =
        workComplete && shortComplete && longComplete &&
        strcmp(currentWork, workActions) == 0 &&
        strcmp(currentShort, shortBreakActions) == 0 &&
        strcmp(currentLong, longBreakActions) == 0;
    if (!configMatches) {
        const IniKeyValue updates[] = {
            {INI_SECTION_POMODORO, "POMODORO_WORK_ACTIONS", workActions},
            {INI_SECTION_POMODORO, "POMODORO_SHORT_BREAK_ACTIONS", shortBreakActions},
            {INI_SECTION_POMODORO, "POMODORO_LONG_BREAK_ACTIONS", longBreakActions},
        };
        if (!WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
            return FALSE;
        }
    }

    strncpy_s(g_AppConfig.pomodoro.work_actions, sizeof(g_AppConfig.pomodoro.work_actions),
              workActions, _TRUNCATE);
    strncpy_s(g_AppConfig.pomodoro.short_break_actions,
              sizeof(g_AppConfig.pomodoro.short_break_actions),
              shortBreakActions, _TRUNCATE);
    strncpy_s(g_AppConfig.pomodoro.long_break_actions,
              sizeof(g_AppConfig.pomodoro.long_break_actions),
              longBreakActions, _TRUNCATE);
    return TRUE;
}
