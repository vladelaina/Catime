/**
 * @file dialog_message.h
 * @brief Reusable modern modal message panel.
 */

#ifndef DIALOG_MESSAGE_H
#define DIALOG_MESSAGE_H

#include <windows.h>

typedef enum {
    DIALOG_MESSAGE_INFO,
    DIALOG_MESSAGE_WARNING,
    DIALOG_MESSAGE_ERROR
} DialogMessageTone;

/** Show a synchronous, themed message panel on the caller's UI thread. */
BOOL DialogMessage_Show(HWND hwndOwner, const wchar_t* title,
                        const wchar_t* message, DialogMessageTone tone);

/** Show the same panel without blocking the owner thread's message loop. */
BOOL DialogMessage_ShowNotification(HWND hwndOwner, const wchar_t* title,
                                    const wchar_t* message);

#endif /* DIALOG_MESSAGE_H */
