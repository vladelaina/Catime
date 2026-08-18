/**
 * @file tray_event_protocol.h
 * @brief Pure decoding for legacy and VERSION_4 tray callbacks.
 */

#ifndef CATIME_TRAY_EVENT_PROTOCOL_H
#define CATIME_TRAY_EVENT_PROTOCOL_H

#include <windows.h>

typedef enum {
    TRAY_CALLBACK_NONE = 0,
    TRAY_CALLBACK_HOVER_MOVE,
    TRAY_CALLBACK_HOVER_OPEN,
    TRAY_CALLBACK_HOVER_CLOSE,
    TRAY_CALLBACK_PRIMARY_MENU,
    TRAY_CALLBACK_SECONDARY_MENU
} TrayCallbackKind;

typedef struct {
    UINT iconId;
    UINT message;
    TrayCallbackKind kind;
    POINT anchor;
    BOOL hasAnchor;
} TrayCallbackEvent;

BOOL TrayCallback_Decode(BOOL version4, WPARAM wParam, LPARAM lParam,
                         TrayCallbackEvent* event);

BOOL TrayCallback_DecodeCompatible(BOOL preferVersion4,
                                   WPARAM wParam, LPARAM lParam,
                                   UINT expectedIconId,
                                   TrayCallbackEvent* event);

#endif
