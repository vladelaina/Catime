/**
 * @file window_commands.h
 * @brief Window command handlers and dispatch system
 */

#ifndef WINDOW_COMMANDS_H
#define WINDOW_COMMANDS_H

#include <windows.h>
#include "config.h"

/* ============================================================================
 * Command Handler Function Type
 * ============================================================================ */

typedef LRESULT (*CommandHandler)(HWND hwnd, WPARAM wp, LPARAM lp);

/* ============================================================================
 * Main Command Dispatcher (window_commands.c)
 * ============================================================================ */

LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp);
BOOL DispatchRangeCommand(HWND hwnd, UINT cmd, WPARAM wp, LPARAM lp);
BOOL HandleLanguageSelection(HWND hwnd, UINT menuId);

/* ============================================================================
 * Timer Commands (window_commands_timer.c)
 * ============================================================================ */

LRESULT CmdCustomCountdown(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdShowCurrentTime(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCountUp(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCountUpStart(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCountUpReset(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCountdownReset(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdPauseResume(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdRestartTimer(HWND hwnd, WPARAM wp, LPARAM lp);

/* Time format */
LRESULT CmdTimeFormat(HWND hwnd, TimeFormatType format);
LRESULT CmdToggleMilliseconds(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT Cmd24HourFormat(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdShowSeconds(HWND hwnd, WPARAM wp, LPARAM lp);

/* Timeout actions */
LRESULT CmdSetTimeoutAction(HWND hwnd, TimeoutActionType action);

/* Startup mode */
LRESULT CmdSetStartupMode(HWND hwnd, const char* mode);
LRESULT CmdSetCountdownTime(HWND hwnd, WPARAM wp, LPARAM lp);

/* Pomodoro */
LRESULT CmdPomodoroStart(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdPomodoroReset(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdPomodoroLoopCount(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdPomodoroCombo(HWND hwnd, WPARAM wp, LPARAM lp);

/* Time options */
LRESULT CmdModifyTimeOptions(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdModifyDefaultTime(HWND hwnd, WPARAM wp, LPARAM lp);

/* Range handlers */
BOOL HandleQuickCountdown(HWND hwnd, UINT cmd, int index);
BOOL HandlePomodoroTime(HWND hwnd, UINT cmd, int index);
BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex);

/* ============================================================================
 * Plugin Commands (window_commands_plugin.c)
 * ============================================================================ */

BOOL HandlePluginCommand(HWND hwnd, UINT cmd);

#endif /* WINDOW_COMMANDS_H */

