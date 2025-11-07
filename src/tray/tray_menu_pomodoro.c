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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External variables */
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_TIMES[10];
extern int POMODORO_TIMES_COUNT;
extern int POMODORO_LOOP_COUNT;
extern int current_pomodoro_time_index;
extern POMODORO_PHASE current_pomodoro_phase;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern int CLOCK_TOTAL_TIME;

#define MAX_POMODORO_TIMES 10

/**
 * @brief Load Pomodoro time options from config using standard API
 * @note Updates global POMODORO_TIMES array
 */
void LoadPomodoroConfig(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    /* Read Pomodoro time options */
    char options[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", 
                  "1500,300,1500,600", options, sizeof(options), configPath);
    
    /* Parse comma-separated time values */
    POMODORO_TIMES_COUNT = 0;
    char* token = strtok(options, ",");
    int index = 0;
    
    while (token && index < MAX_POMODORO_TIMES) {
        POMODORO_TIMES[index++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    POMODORO_TIMES_COUNT = index;
    
    /* Update default Pomodoro times */
    if (index > 0) POMODORO_WORK_TIME = POMODORO_TIMES[0];
    if (index > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
    if (index > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
    
    /* Read loop count */
    POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 
                                     1, configPath);
    if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
}

/**
 * @brief Build Pomodoro submenu
 * @param hMenu Parent menu handle to append to
 */
void BuildPomodoroMenu(HMENU hMenu) {
    if (!hMenu) return;
    
    LoadPomodoroConfig();

    HMENU hPomodoroMenu = CreatePopupMenu();
    
    wchar_t timeBuffer[64];
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(L"开始", L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        FormatPomodoroTime(POMODORO_TIMES[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
        BOOL isCurrentPhase = (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                              current_pomodoro_time_index == i &&
                              !CLOCK_SHOW_CURRENT_TIME &&
                              !CLOCK_COUNT_UP &&
                              CLOCK_TOTAL_TIME == POMODORO_TIMES[i]);
        
        AppendMenuW(hPomodoroMenu, MF_STRING | (isCurrentPhase ? MF_CHECKED : MF_UNCHECKED), 
                    menuId, timeBuffer);
    }

    wchar_t menuText[64];
    _snwprintf(menuText, sizeof(menuText)/sizeof(wchar_t),
              GetLocalizedString(L"循环次数: %d", L"Loop Count: %d"),
              POMODORO_LOOP_COUNT);
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_LOOP_COUNT, menuText);

    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_COMBINATION,
              GetLocalizedString(L"组合", L"Combination"));
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPomodoroMenu,
                GetLocalizedString(L"番茄时钟", L"Pomodoro"));
}

