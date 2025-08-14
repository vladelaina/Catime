/**
 * @file window_procedure.h
 * @brief Window message handling procedure interface
 * 
 * This file defines the application's main window message handling callback function interface,
 * processing all window message events including drawing, mouse, keyboard, menu, and timer events.
 */

#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>

// Custom application messages
// Used for inter-instance communication (e.g., show CLI help without resetting timer)
#ifndef WM_APP_SHOW_CLI_HELP
#define WM_APP_SHOW_CLI_HELP (WM_APP + 2)
#endif

 // Forward quick countdown by index from another instance
 #ifndef WM_APP_QUICK_COUNTDOWN_INDEX
 #define WM_APP_QUICK_COUNTDOWN_INDEX (WM_APP + 3)
 #endif

// COPYDATA identifier for forwarding CLI text (countdown arguments)
#ifndef COPYDATA_ID_CLI_TEXT
#define COPYDATA_ID_CLI_TEXT 0x10010001
#endif

/**
 * @brief Hotkey ID definitions
 */
#define HOTKEY_ID_SHOW_TIME       100  // Show current time hotkey ID
#define HOTKEY_ID_COUNT_UP        101  // Count-up hotkey ID
#define HOTKEY_ID_COUNTDOWN       102  // Countdown hotkey ID
#define HOTKEY_ID_QUICK_COUNTDOWN1 103 // Quick countdown 1 hotkey ID
#define HOTKEY_ID_QUICK_COUNTDOWN2 104 // Quick countdown 2 hotkey ID
#define HOTKEY_ID_QUICK_COUNTDOWN3 105 // Quick countdown 3 hotkey ID
#define HOTKEY_ID_POMODORO        106  // Pomodoro hotkey ID
#define HOTKEY_ID_TOGGLE_VISIBILITY 107 // Hide/Show hotkey ID
#define HOTKEY_ID_EDIT_MODE       108  // Edit mode hotkey ID
#define HOTKEY_ID_PAUSE_RESUME    109  // Pause/Resume hotkey ID
#define HOTKEY_ID_RESTART_TIMER   110  // Restart timer hotkey ID

/**
 * @brief Main window message handling function
 * @param hwnd Window handle
 * @param msg Message ID
 * @param wp Message parameter
 * @param lp Message parameter
 * @return LRESULT Message handling result
 * 
 * Processes all window messages for the application's main window, including:
 * - Creation and destruction events
 * - Drawing and repainting
 * - Mouse and keyboard input
 * - Window position and size changes
 * - Tray icon events
 * - Menu command messages
 * - Timer events
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/**
 * @brief Register global hotkeys
 * @param hwnd Window handle
 * 
 * Reads and registers global hotkey settings from the configuration file, used for quickly switching to show current time, count-up, and default countdown.
 * If hotkeys are already registered, they will be unregistered first and then re-registered.
 * 
 * @return BOOL Whether at least one hotkey was successfully registered
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister global hotkeys
 * @param hwnd Window handle
 * 
 * Unregisters all previously registered global hotkeys.
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Switch to show current time mode
 * @param hwnd Window handle
 */
void ToggleShowTimeMode(HWND hwnd);

/**
 * @brief Start count-up timer
 * @param hwnd Window handle
 */
void StartCountUp(HWND hwnd);

/**
 * @brief Start default countdown
 * @param hwnd Window handle
 */
void StartDefaultCountDown(HWND hwnd);

/**
 * @brief Start Pomodoro timer
 * @param hwnd Window handle
 */
void StartPomodoroTimer(HWND hwnd);

/**
 * @brief Toggle edit mode
 * @param hwnd Window handle
 */
void ToggleEditMode(HWND hwnd);

/**
 * @brief Pause/Resume timer
 * @param hwnd Window handle
 */
void TogglePauseResume(HWND hwnd);

/**
 * @brief Restart current timer
 * @param hwnd Window handle
 */
void RestartCurrentTimer(HWND hwnd);

/**
 * @brief Quick countdown 1 function
 * @param hwnd Window handle
 * 
 * Start countdown using the first item in the preset time options
 */
void StartQuickCountdown1(HWND hwnd);

/**
 * @brief Quick countdown 2 function
 * @param hwnd Window handle
 * 
 * Start countdown using the second item in the preset time options
 */
void StartQuickCountdown2(HWND hwnd);

/**
 * @brief Quick countdown 3 function
 * @param hwnd Window handle
 * 
 * Start countdown using the third item in the preset time options
 */
void StartQuickCountdown3(HWND hwnd);

 /**
  * @brief Start quick countdown by preset index (1-based)
  * @param hwnd Window handle
  * @param index 1-based index into preset `time_options`
  */
 void StartQuickCountdownByIndex(HWND hwnd, int index);

#endif // WINDOW_PROCEDURE_H 