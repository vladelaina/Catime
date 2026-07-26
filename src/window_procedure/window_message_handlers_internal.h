/**
 * @file window_message_handlers_internal.h
 * @brief Private interfaces shared by Windows message handler modules.
 */

#ifndef WINDOW_MESSAGE_HANDLERS_INTERNAL_H
#define WINDOW_MESSAGE_HANDLERS_INTERNAL_H

#include "window_procedure/window_message_handlers.h"

#define IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD 42427

void WindowMessageInternal_StopEditExitRightClickShield(HWND hwnd);
void WindowMessageInternal_ResetEditExitRightClickState(HWND hwnd);
void WindowMessageInternal_DispatchPendingMenuPreview(HWND hwnd);
void WindowMessageInternal_CancelTrackedMenuPreview(HWND hwnd);

#endif /* WINDOW_MESSAGE_HANDLERS_INTERNAL_H */
