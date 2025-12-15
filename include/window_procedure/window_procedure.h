/**
 * @file window_procedure.h
 * @brief Window message dispatcher and timer control API
 * 
 * Table-driven dispatch eliminates switch statement bloat (MESSAGE_DISPATCH_TABLE).
 * Data-driven hotkey management scales via g_hotkeyConfigs array (no parameter explosion).
 * Unified timer mode switching prevents code duplication across actions.
 */

#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>
#include "../resource/resource.h"
#include "config.h"

/* ============================================================================
 * Custom Window Messages
 * ============================================================================ */

#ifndef WM_APP_SHOW_CLI_HELP
#define WM_APP_SHOW_CLI_HELP (WM_APP + 2)
#endif

#ifndef WM_APP_QUICK_COUNTDOWN_INDEX
#define WM_APP_QUICK_COUNTDOWN_INDEX (WM_APP + 3)
#endif

/* Config reload notifications */
#ifndef WM_APP_ANIM_PATH_CHANGED
#define WM_APP_ANIM_PATH_CHANGED (WM_APP + 50)
#endif
#ifndef WM_APP_ANIM_SPEED_CHANGED
#define WM_APP_ANIM_SPEED_CHANGED (WM_APP + 51)
#endif
#ifndef WM_APP_DISPLAY_CHANGED
#define WM_APP_DISPLAY_CHANGED (WM_APP + 52)
#endif
#ifndef WM_APP_TIMER_CHANGED
#define WM_APP_TIMER_CHANGED (WM_APP + 53)
#endif
#ifndef WM_APP_POMODORO_CHANGED
#define WM_APP_POMODORO_CHANGED (WM_APP + 54)
#endif
#ifndef WM_APP_NOTIFICATION_CHANGED
#define WM_APP_NOTIFICATION_CHANGED (WM_APP + 55)
#endif
#ifndef WM_APP_HOTKEYS_CHANGED
#define WM_APP_HOTKEYS_CHANGED (WM_APP + 56)
#endif
#ifndef WM_APP_RECENTFILES_CHANGED
#define WM_APP_RECENTFILES_CHANGED (WM_APP + 57)
#endif
#ifndef WM_APP_COLORS_CHANGED
#define WM_APP_COLORS_CHANGED (WM_APP + 58)
#endif

#ifndef COPYDATA_ID_CLI_TEXT
#define COPYDATA_ID_CLI_TEXT 0x10010001
#endif

/* ============================================================================
 * Core Window Procedure
 * ============================================================================ */

/**
 * @brief Main window procedure (table-driven dispatch)
 * @return Message result
 * 
 * @details Table-driven architecture:
 * - MESSAGE_DISPATCH_TABLE (27 entries)
 * - APP_MESSAGE_DISPATCH_TABLE (9 config reload entries)
 * - COMMAND_DISPATCH_TABLE (80+ menu entries)
 * 
 * Flow: WM_TASKBARCREATED → DispatchAppMessage → MESSAGE_DISPATCH_TABLE → DefWindowProc
 * 
 * Benefits: Easy handler addition, testable handlers, minimal complexity
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* ============================================================================
 * Hotkey Management
 * ============================================================================ */

/**
 * @brief Register configured hotkeys
 * @return TRUE if at least one registered
 * 
 * @details
 * Loop-based via g_hotkeyConfigs array (configKey-driven).
 * Loads from INI, registers with Windows, writes back conflicts.
 * Scalable (just add to array).
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister all hotkeys (prevents reload conflicts)
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/* ============================================================================
 * Timer Action API
 * ============================================================================ */

/**
 * @brief Toggle to clock display mode
 */
void ToggleShowTimeMode(HWND hwnd);

/**
 * @brief Start stopwatch from zero
 */
void StartCountUp(HWND hwnd);

/**
 * @brief Start default countdown (prompts if unset)
 */
void StartDefaultCountDown(HWND hwnd);

/**
 * @brief Start Pomodoro work session
 */
void StartPomodoroTimer(HWND hwnd);

/**
 * @brief Toggle edit mode (click-through vs draggable)
 */
void ToggleEditMode(HWND hwnd);

/**
 * @brief Pause/resume active timer (preserves millisecond precision)
 */
void TogglePauseResume(HWND hwnd);

/**
 * @brief Restart current timer (stops sounds/notifications)
 */
void RestartCurrentTimer(HWND hwnd);

/**
 * @brief Start quick countdown preset
 * @param index 1-based preset index
 */
void StartQuickCountdownByIndex(HWND hwnd, int index);

/**
 * @brief Stop sounds/notifications before mode changes
 * 
 * @details Prevents overlap and audio issues during transitions
 */
void CleanupBeforeTimerAction(void);

/**
 * @brief Start countdown programmatically
 * @param seconds Duration
 * @return TRUE if started, FALSE if seconds <= 0
 */
BOOL StartCountdownWithTime(HWND hwnd, int seconds);

/**
 * @brief Toggle window visibility (show/hide)
 */
void ToggleWindowVisibility(HWND hwnd);

/**
 * @brief Toggle window topmost status
 */
void ToggleTopmost(HWND hwnd);

/**
 * @brief Toggle millisecond display
 */
void ToggleMilliseconds(HWND hwnd);

/* ============================================================================
 * Configuration Handlers
 * ============================================================================ */

/**
 * @brief Process language selection
 * @param menuId CLOCK_IDM_LANG_*
 * @return TRUE if changed
 */
BOOL HandleLanguageSelection(HWND hwnd, UINT menuId);

/**
 * @brief Configure Pomodoro phase duration
 * @param selectedIndex Phase (0=work, 1=short break, 2=long break)
 * @return TRUE if updated, FALSE if cancelled
 */
BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex);

#endif