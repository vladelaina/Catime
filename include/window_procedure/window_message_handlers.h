/**
 * @file window_message_handlers.h
 * @brief Windows message handlers for window procedure
 */

#ifndef WINDOW_MESSAGE_HANDLERS_H
#define WINDOW_MESSAGE_HANDLERS_H

#include <windows.h>

/* ============================================================================
 * Windows Message Handlers
 * ============================================================================ */

LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleMouseMove(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandlePaint(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleKeyDown(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleQuickCountdownIndex(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleShowCliHelp(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleTrayUpdateIcon(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleAppReregisterHotkeys(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleAnimationPreviewLoaded(HWND hwnd, WPARAM wp, LPARAM lp);

// Owner-drawn menu handlers
LRESULT HandleMeasureItem(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleDrawItem(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp);

#endif /* WINDOW_MESSAGE_HANDLERS_H */
