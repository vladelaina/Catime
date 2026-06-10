/**
 * @file tray_menu_pomodoro.c
 * @brief Pomodoro menu implementation
 */

#include "tray/tray_menu_pomodoro.h"
#include "tray/tray_menu.h"
#include "config.h"
#include "language.h"
#include "timer/timer.h"
#include "pomodoro.h"
#include "utils/string_format.h"
#include "config/config_defaults.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/* External variables */
extern int current_pomodoro_time_index;
extern POMODORO_PHASE current_pomodoro_phase;

static BOOL ParsePositiveSecondsToken(const char* token, int* seconds) {
    if (!token || !seconds) return FALSE;

    while (isspace((unsigned char)*token)) token++;
    if (*token == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(token, &end, 10);
    if (end == token || errno == ERANGE ||
        parsed <= 0 || parsed > MAX_POMODORO_OPTION_SECONDS) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *seconds = (int)parsed;
    return TRUE;
}

static BOOL ParsePomodoroTimeOptions(char* optionsStr, int* parsedOptions,
                                     int* parsedCount) {
    if (!optionsStr || !parsedOptions || !parsedCount) {
        return FALSE;
    }

    *parsedCount = 0;
    char* cursor = optionsStr;
    while (cursor) {
        char* next = strchr(cursor, ',');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*parsedCount >= MAX_POMODORO_TIMES) {
            LOG_WARNING("Too many Pomodoro intervals in tray config load; maximum is %d",
                        MAX_POMODORO_TIMES);
            return FALSE;
        }

        int seconds = 0;
        if (!ParsePositiveSecondsToken(cursor, &seconds)) {
            LOG_WARNING("Invalid Pomodoro interval in tray config load: '%s'", cursor);
            return FALSE;
        }

        parsedOptions[*parsedCount] = seconds;
        (*parsedCount)++;
        cursor = next;
    }

    return *parsedCount > 0;
}

static void ApplyPomodoroTimes(const int* times, int count) {
    if (!times || count <= 0 ||
        count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return;
    }

    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    memcpy(g_AppConfig.pomodoro.times, times, (size_t)count * sizeof(times[0]));
    g_AppConfig.pomodoro.times_count = count;
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    if (count > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    if (count > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
}

static void SetDefaultPomodoroTimes(void) {
    const int defaultTimes[] = {
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_SHORT_BREAK,
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_LONG_BREAK
    };
    ApplyPomodoroTimes(defaultTimes, (int)_countof(defaultTimes));
}

/**
 * @brief Load Pomodoro time options from config using standard API
 * @note Updates global POMODORO_TIMES array
 */
void LoadPomodoroConfig(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    /* Read Pomodoro time options */
    char options[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL optionsComplete = ReadIniStringExact(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS",
                                              DEFAULT_POMODORO_OPTIONS_INI,
                                              options, sizeof(options), configPath);

    int parsedTimes[MAX_POMODORO_TIMES] = {0};
    int parsedCount = 0;
    if (optionsComplete &&
        ParsePomodoroTimeOptions(options, parsedTimes, &parsedCount)) {
        ApplyPomodoroTimes(parsedTimes, parsedCount);
    } else {
        if (!optionsComplete) {
            LOG_WARNING("Pomodoro intervals config is too long in tray config load, using defaults");
        }
        SetDefaultPomodoroTimes();
    }

    /* Read loop count */
    g_AppConfig.pomodoro.loop_count = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 
                                     DEFAULT_POMODORO_LOOP_COUNT, configPath);
    if (g_AppConfig.pomodoro.loop_count < MIN_POMODORO_LOOP_COUNT) {
        g_AppConfig.pomodoro.loop_count = MIN_POMODORO_LOOP_COUNT;
    }
    if (g_AppConfig.pomodoro.loop_count > MAX_POMODORO_LOOP_COUNT) {
        g_AppConfig.pomodoro.loop_count = MAX_POMODORO_LOOP_COUNT;
    }
}

/**
 * @brief Build Pomodoro submenu
 * @param hMenu Parent menu handle to append to
 */
void BuildPomodoroMenu(HMENU hMenu) {
    if (!hMenu) return;

    HMENU hPomodoroMenu = CreatePopupMenu();
    if (!hPomodoroMenu) return;
    
    wchar_t timeBuffer[64];
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(NULL, L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    int timesCount = g_AppConfig.pomodoro.times_count;
    if (timesCount < 0) timesCount = 0;
    if (timesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        timesCount = (int)_countof(g_AppConfig.pomodoro.times);
    }

    for (int i = 0; i < timesCount; i++) {
        FormatPomodoroTime(g_AppConfig.pomodoro.times[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
        BOOL isCurrentPhase = (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                              current_pomodoro_time_index == i &&
                              !CLOCK_SHOW_CURRENT_TIME &&
                              !CLOCK_COUNT_UP &&
                              CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.times[i]);
        
        AppendMenuW(hPomodoroMenu, MF_STRING | (isCurrentPhase ? MF_CHECKED : MF_UNCHECKED), 
                    menuId, timeBuffer);
    }

    wchar_t menuText[64];
    _snwprintf_s(menuText, _countof(menuText), _TRUNCATE,
                GetLocalizedString(NULL, L"Loop Count: %d"),
                g_AppConfig.pomodoro.loop_count);
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_LOOP_COUNT, menuText);

    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_COMBINATION,
              GetLocalizedString(NULL, L"Combination"));
    
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPomodoroMenu,
                     GetLocalizedString(NULL, L"Pomodoro"))) {
        DestroyMenu(hPomodoroMenu);
    }
}

