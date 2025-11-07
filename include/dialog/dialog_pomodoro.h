/**
 * @file dialog_pomodoro.h
 * @brief Pomodoro-specific dialogs (loop count, time intervals)
 */

#ifndef DIALOG_POMODORO_H
#define DIALOG_POMODORO_H

#include <windows.h>

/* ============================================================================
 * Pomodoro Loop Dialog
 * ============================================================================ */

/**
 * @brief Show Pomodoro loop count configuration dialog
 * @param hwndParent Parent window
 * 
 * @details
 * Configures: Number of Pomodoro cycles before long break
 * Format: Plain number (1-99)
 * Persists to: [POMODORO]loop_count
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

/**
 * @brief Pomodoro loop dialog procedure
 */
INT_PTR CALLBACK PomodoroLoopDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Pomodoro Combo Dialog (Time Intervals)
 * ============================================================================ */

/**
 * @brief Show Pomodoro time intervals configuration dialog
 * @param hwndParent Parent window
 * 
 * @details
 * Configures: [work, short break, long break] times
 * Format: Space-separated durations (e.g., "25m 5m 15m")
 * Persists to: [POMODORO]times
 * 
 * @note Max 10 intervals
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/**
 * @brief Pomodoro combo dialog procedure
 */
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Global State
 * ============================================================================ */

/**
 * @brief Max Pomodoro time intervals
 * @note POMODORO_TIMES and POMODORO_TIMES_COUNT now in g_AppConfig.pomodoro
 * @details Default: [25m, 5m, 15m] (work, short break, long break)
 */
#define MAX_POMODORO_TIMES 10

#endif /* DIALOG_POMODORO_H */

