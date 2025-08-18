#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>

#ifndef WM_APP_SHOW_CLI_HELP
#define WM_APP_SHOW_CLI_HELP (WM_APP + 2)
#endif

#ifndef WM_APP_QUICK_COUNTDOWN_INDEX
#define WM_APP_QUICK_COUNTDOWN_INDEX (WM_APP + 3)
#endif

#ifndef COPYDATA_ID_CLI_TEXT
#define COPYDATA_ID_CLI_TEXT 0x10010001
#endif

#define HOTKEY_ID_SHOW_TIME       100
#define HOTKEY_ID_COUNT_UP        101
#define HOTKEY_ID_COUNTDOWN       102
#define HOTKEY_ID_QUICK_COUNTDOWN1 103
#define HOTKEY_ID_QUICK_COUNTDOWN2 104
#define HOTKEY_ID_QUICK_COUNTDOWN3 105
#define HOTKEY_ID_POMODORO        106
#define HOTKEY_ID_TOGGLE_VISIBILITY 107
#define HOTKEY_ID_EDIT_MODE       108
#define HOTKEY_ID_PAUSE_RESUME    109
#define HOTKEY_ID_RESTART_TIMER   110

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

BOOL RegisterGlobalHotkeys(HWND hwnd);

void UnregisterGlobalHotkeys(HWND hwnd);

void ToggleShowTimeMode(HWND hwnd);

void StartCountUp(HWND hwnd);

void StartDefaultCountDown(HWND hwnd);

void StartPomodoroTimer(HWND hwnd);

void ToggleEditMode(HWND hwnd);

void TogglePauseResume(HWND hwnd);

void RestartCurrentTimer(HWND hwnd);

void StartQuickCountdown1(HWND hwnd);

void StartQuickCountdown2(HWND hwnd);

void StartQuickCountdown3(HWND hwnd);

void StartQuickCountdownByIndex(HWND hwnd, int index);

#endif