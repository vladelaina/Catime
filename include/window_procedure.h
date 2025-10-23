/**
 * @file window_procedure.h
 * @brief Window procedure and timer action API
 * @version 2.0 - Refactored with improved documentation
 * 
 * Public API for window message handling, hotkey registration,
 * and timer control operations (countdown, count-up, Pomodoro).
 */

#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>

/* ============================================================================
 * Custom Window Messages
 * ============================================================================ */

/** @brief CLI help display request */
#ifndef WM_APP_SHOW_CLI_HELP
#define WM_APP_SHOW_CLI_HELP (WM_APP + 2)
#endif

/** @brief Quick countdown by index trigger */
#ifndef WM_APP_QUICK_COUNTDOWN_INDEX
#define WM_APP_QUICK_COUNTDOWN_INDEX (WM_APP + 3)
#endif

/** @brief Config file change notifications (animation) */
#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif

/** @brief Config file change notification (display settings) */
#ifndef WM_APP_DISPLAY_CHANGED
#define WM_APP_DISPLAY_CHANGED (WM_APP + 52)
#endif

/** @brief Config file change notification (timer settings) */
#ifndef WM_APP_TIMER_CHANGED
#define WM_APP_TIMER_CHANGED (WM_APP + 53)
#endif

/** @brief Config file change notification (Pomodoro settings) */
#ifndef WM_APP_POMODORO_CHANGED
#define WM_APP_POMODORO_CHANGED (WM_APP + 54)
#endif

/** @brief Config file change notification (notification settings) */
#ifndef WM_APP_NOTIFICATION_CHANGED
#define WM_APP_NOTIFICATION_CHANGED (WM_APP + 55)
#endif

/** @brief Config file change notification (hotkey assignments) */
#ifndef WM_APP_HOTKEYS_CHANGED
#define WM_APP_HOTKEYS_CHANGED (WM_APP + 56)
#endif

/** @brief Config file change notification (recent files list) */
#ifndef WM_APP_RECENTFILES_CHANGED
#define WM_APP_RECENTFILES_CHANGED (WM_APP + 57)
#endif

/** @brief Config file change notification (color options) */
#ifndef WM_APP_COLORS_CHANGED
#define WM_APP_COLORS_CHANGED (WM_APP + 58)
#endif

/** @brief Inter-process communication identifier for CLI text */
#ifndef COPYDATA_ID_CLI_TEXT
#define COPYDATA_ID_CLI_TEXT 0x10010001
#endif

/* ============================================================================
 * Global Hotkey Identifiers
 * ============================================================================ */

#define HOTKEY_ID_SHOW_TIME       100  /**< Toggle time display mode */
#define HOTKEY_ID_COUNT_UP        101  /**< Start stopwatch timer */
#define HOTKEY_ID_COUNTDOWN       102  /**< Start configured countdown */
#define HOTKEY_ID_QUICK_COUNTDOWN1 103 /**< Quick countdown preset 1 */
#define HOTKEY_ID_QUICK_COUNTDOWN2 104 /**< Quick countdown preset 2 */
#define HOTKEY_ID_QUICK_COUNTDOWN3 105 /**< Quick countdown preset 3 */
#define HOTKEY_ID_POMODORO        106  /**< Start Pomodoro session */
#define HOTKEY_ID_TOGGLE_VISIBILITY 107 /**< Show/hide window */
#define HOTKEY_ID_EDIT_MODE       108  /**< Enter/exit positioning mode */
#define HOTKEY_ID_PAUSE_RESUME    109  /**< Toggle timer pause state */
#define HOTKEY_ID_RESTART_TIMER   110  /**< Reset and restart timer */
#define HOTKEY_ID_CUSTOM_COUNTDOWN 111 /**< Custom countdown input */

/* ============================================================================
 * Core Window Procedure
 * ============================================================================ */

/**
 * @brief Primary window procedure for all message handling
 * @param hwnd Window handle
 * @param msg Message identifier
 * @param wp Message-specific parameter
 * @param lp Message-specific parameter
 * @return Message processing result
 * 
 * Central dispatcher routing to specialized handlers for
 * window lifecycle, input, painting, timers, and commands.
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* ============================================================================
 * Hotkey Management
 * ============================================================================ */

/**
 * @brief Register all configured global hotkeys
 * @param hwnd Window to receive WM_HOTKEY messages
 * @return TRUE if at least one hotkey registered
 * 
 * Loads configuration and registers hotkeys, automatically
 * clearing conflicting entries.
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister all global hotkeys
 * @param hwnd Window that registered the hotkeys
 * 
 * Removes all hotkey registrations to prevent conflicts.
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/* ============================================================================
 * Timer Action API
 * ============================================================================ */

/**
 * @brief Toggle between timer and current time display
 * @param hwnd Window handle
 * 
 * Switches to current time mode and adjusts refresh interval.
 */
void ToggleShowTimeMode(HWND hwnd);

/**
 * @brief Start count-up timer (stopwatch) from zero
 * @param hwnd Window handle
 * 
 * Initializes stopwatch mode and resets timer interval if needed.
 */
void StartCountUp(HWND hwnd);

/**
 * @brief Start default countdown timer
 * @param hwnd Window handle
 * 
 * Uses configured default duration or prompts if not set.
 */
void StartDefaultCountDown(HWND hwnd);

/**
 * @brief Start Pomodoro work session
 * @param hwnd Window handle
 * 
 * Initiates Pomodoro technique with configured durations.
 */
void StartPomodoroTimer(HWND hwnd);

/**
 * @brief Toggle edit mode for window positioning
 * @param hwnd Window handle
 * 
 * Switches between click-through and interactive modes.
 */
void ToggleEditMode(HWND hwnd);

/**
 * @brief Toggle pause/resume for active timer
 * @param hwnd Window handle
 * 
 * Pauses/resumes countdown or count-up (not clock display).
 */
void TogglePauseResume(HWND hwnd);

/**
 * @brief Restart current timer from beginning
 * @param hwnd Window handle
 * 
 * Resets elapsed time and restarts the active timer mode.
 */
void RestartCurrentTimer(HWND hwnd);

/**
 * @brief Start quick countdown by 1-based index
 * @param hwnd Window handle
 * @param index Option index (1=first, 2=second, 3=third, etc.)
 * 
 * Unified API for starting configured quick countdown presets.
 * Used by hotkeys and menu commands.
 */
void StartQuickCountdownByIndex(HWND hwnd, int index);

/**
 * @brief Clean up before timer state changes
 * 
 * Stops notification sounds and closes notification windows.
 * Called internally before starting/stopping/switching timers.
 */
void CleanupBeforeTimerAction(void);

/**
 * @brief Start countdown with specified duration
 * @param hwnd Window handle
 * @param seconds Duration in seconds
 * @return TRUE if started, FALSE if seconds <= 0
 * 
 * Programmatic API for starting custom countdown timers.
 */
BOOL StartCountdownWithTime(HWND hwnd, int seconds);

/* ============================================================================
 * Configuration Handlers
 * ============================================================================ */

/**
 * @brief Process language selection menu command
 * @param hwnd Window handle
 * @param menuId Language menu ID (CLOCK_IDM_LANG_*)
 * @return TRUE if language changed, FALSE if invalid
 * 
 * Maps menu command to language enum and updates config.
 */
BOOL HandleLanguageSelection(HWND hwnd, UINT menuId);

/**
 * @brief Configure Pomodoro phase duration
 * @param hwnd Window handle
 * @param selectedIndex Phase index (0=work, 1=short break, 2=long break)
 * @return TRUE if updated, FALSE if cancelled
 * 
 * Shows input dialog and updates configuration.
 */
BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex);

#endif