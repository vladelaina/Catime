/**
 * @file tray_menu_tracking.h
 * @brief Foreground ownership lifecycle for dismissible tray menus.
 */

#ifndef CATIME_TRAY_MENU_TRACKING_H
#define CATIME_TRAY_MENU_TRACKING_H

#include <windows.h>

typedef struct {
    HWND owner;
    BOOL initialized;
    BOOL restoreNoActivate;
    BOOL restoreTransparent;
    BOOL foregroundAcquired;
} TrayMenuTrackingState;

BOOL TrayMenuTracking_Begin(HWND owner, TrayMenuTrackingState* state);
BOOL TrayMenuTracking_ReassertForeground(TrayMenuTrackingState* state);
void TrayMenuTracking_End(TrayMenuTrackingState* state);

#endif
