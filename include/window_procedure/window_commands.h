/**
 * @file window_commands.h
 * @brief Window command handlers and dispatch system
 */

#ifndef WINDOW_COMMANDS_H
#define WINDOW_COMMANDS_H

#include <windows.h>

/* ============================================================================
 * Command Handler Function Type
 * ============================================================================ */

typedef LRESULT (*CommandHandler)(HWND hwnd, WPARAM wp, LPARAM lp);

/* ============================================================================
 * Main Command Dispatcher
 * ============================================================================ */

/**
 * @brief Route menu command to appropriate handler
 */
LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp);

/**
 * @brief Dispatch range command (colors, fonts, quick countdowns, etc.)
 */
BOOL DispatchRangeCommand(HWND hwnd, UINT cmd, WPARAM wp, LPARAM lp);

/**
 * @brief Handle language selection menu
 */
BOOL HandleLanguageSelection(HWND hwnd, UINT menuId);

/**
 * @brief Handle Pomodoro time configuration
 */
BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex);

#endif /* WINDOW_COMMANDS_H */

