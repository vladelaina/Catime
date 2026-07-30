/**
 * @file tray_event_protocol.c
 * @brief Tray callback normalization independent of Shell/UI state.
 */

#include "tray/tray_event_protocol.h"
#include <shellapi.h>
#include <string.h>

static TrayCallbackKind ClassifyMessage(BOOL version4, UINT message) {
    if (message == WM_MOUSEMOVE) return TRAY_CALLBACK_HOVER_MOVE;
    if (message == NIN_POPUPOPEN) return TRAY_CALLBACK_HOVER_OPEN;
    if (message == NIN_POPUPCLOSE) return TRAY_CALLBACK_HOVER_CLOSE;

    if (version4) {
        if (message == NIN_SELECT || message == NIN_KEYSELECT) {
            return TRAY_CALLBACK_PRIMARY_MENU;
        }
        if (message == WM_CONTEXTMENU) {
            return TRAY_CALLBACK_SECONDARY_MENU;
        }
        return TRAY_CALLBACK_NONE;
    }

    if (message == WM_LBUTTONUP) return TRAY_CALLBACK_PRIMARY_MENU;
    if (message == WM_RBUTTONUP) return TRAY_CALLBACK_SECONDARY_MENU;
    return TRAY_CALLBACK_NONE;
}

BOOL TrayCallback_Decode(BOOL version4, WPARAM wParam, LPARAM lParam,
                         TrayCallbackEvent* event) {
    if (!event) return FALSE;
    memset(event, 0, sizeof(*event));

    if (version4) {
        event->message = LOWORD((DWORD_PTR)lParam);
        event->iconId = HIWORD((DWORD_PTR)lParam);
        event->anchor.x = (SHORT)LOWORD((DWORD_PTR)wParam);
        event->anchor.y = (SHORT)HIWORD((DWORD_PTR)wParam);
    } else {
        event->iconId = (UINT)wParam;
        event->message = (UINT)lParam;
    }

    event->kind = ClassifyMessage(version4, event->message);
    event->hasAnchor = version4 &&
        (event->kind == TRAY_CALLBACK_PRIMARY_MENU ||
         event->kind == TRAY_CALLBACK_SECONDARY_MENU);
    return TRUE;
}
