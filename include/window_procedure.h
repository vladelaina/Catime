/**
 * @file window_procedure.h
 * @brief Main window procedure and message handling
 * 
 * Core window message processing, hotkey management, and timer controls
 */

#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>

/** @brief Custom message for CLI help display */
#ifndef WM_APP_SHOW_CLI_HELP
#define WM_APP_SHOW_CLI_HELP (WM_APP + 2)
#endif

/** @brief Custom message for quick countdown by index */
#ifndef WM_APP_QUICK_COUNTDOWN_INDEX
#define WM_APP_QUICK_COUNTDOWN_INDEX (WM_APP + 3)
#endif

/** @brief Copy data identifier for CLI text */
#ifndef COPYDATA_ID_CLI_TEXT
#define COPYDATA_ID_CLI_TEXT 0x10010001
#endif

/** @brief Global hotkey identifiers */
#define HOTKEY_ID_SHOW_TIME       100  /**< Toggle current time display */
#define HOTKEY_ID_COUNT_UP        101  /**< Start count-up timer */
#define HOTKEY_ID_COUNTDOWN       102  /**< Start default countdown */
#define HOTKEY_ID_QUICK_COUNTDOWN1 103 /**< Quick countdown option 1 */
#define HOTKEY_ID_QUICK_COUNTDOWN2 104 /**< Quick countdown option 2 */
#define HOTKEY_ID_QUICK_COUNTDOWN3 105 /**< Quick countdown option 3 */
#define HOTKEY_ID_POMODORO        106  /**< Start Pomodoro timer */
#define HOTKEY_ID_TOGGLE_VISIBILITY 107 /**< Toggle window visibility */
#define HOTKEY_ID_EDIT_MODE       108  /**< Toggle edit mode */
#define HOTKEY_ID_PAUSE_RESUME    109  /**< Pause/resume timer */
#define HOTKEY_ID_RESTART_TIMER   110  /**< Restart current timer */

/**
 * @brief Main window procedure for message processing
 * @param hwnd Window handle
 * @param msg Message identifier
 * @param wp First message parameter
 * @param lp Second message parameter
 * @return Message processing result
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/**
 * @brief Register all global hotkeys with system
 * @param hwnd Window handle to receive hotkey messages
 * @return TRUE if any hotkeys registered successfully
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister all global hotkeys from system
 * @param hwnd Window handle that owns hotkeys
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Toggle between timer and current time display
 * @param hwnd Window handle
 */
void ToggleShowTimeMode(HWND hwnd);

/**
 * @brief Start count-up timer from zero
 * @param hwnd Window handle
 */
void StartCountUp(HWND hwnd);

/**
 * @brief Start default countdown timer
 * @param hwnd Window handle
 */
void StartDefaultCountDown(HWND hwnd);

/**
 * @brief Start Pomodoro timer session
 * @param hwnd Window handle
 */
void StartPomodoroTimer(HWND hwnd);

/**
 * @brief Toggle window edit mode
 * @param hwnd Window handle
 */
void ToggleEditMode(HWND hwnd);

/**
 * @brief Toggle timer pause/resume state
 * @param hwnd Window handle
 */
void TogglePauseResume(HWND hwnd);

/**
 * @brief Restart current timer from beginning
 * @param hwnd Window handle
 */
void RestartCurrentTimer(HWND hwnd);

/**
 * @brief Start first quick countdown option
 * @param hwnd Window handle
 */
void StartQuickCountdown1(HWND hwnd);

/**
 * @brief Start second quick countdown option
 * @param hwnd Window handle
 */
void StartQuickCountdown2(HWND hwnd);

/**
 * @brief Start third quick countdown option
 * @param hwnd Window handle
 */
void StartQuickCountdown3(HWND hwnd);

/**
 * @brief Start quick countdown by option index
 * @param hwnd Window handle
 * @param index 1-based index of countdown option
 */
void StartQuickCountdownByIndex(HWND hwnd, int index);

#endif