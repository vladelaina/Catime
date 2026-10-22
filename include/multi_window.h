/** @file multi_window.h @brief Independent Catime window process support. */
#ifndef CATIME_MULTI_WINDOW_H
#define CATIME_MULTI_WINDOW_H

#include <windows.h>

#define CATIME_MAX_TIMER_WINDOWS 20

BOOL MultiWindow_IsSecondary(void);
/**
 * Returns the stable placement slot assigned while this process creates its
 * main window. Slot zero is the primary window; secondary windows use their
 * own slot for persisted display placement.
 */
int MultiWindow_GetPlacementSlot(void);
BOOL MultiWindow_BeginMainWindowCreation(void);
void MultiWindow_EndMainWindowCreation(void);
void MultiWindow_OffsetInitialPosition(int* x, int* y);
int MultiWindow_GetOffsetForWindowIndex(int index);
BOOL MultiWindow_LaunchNewTimerWindow(HWND hwnd);

#endif
