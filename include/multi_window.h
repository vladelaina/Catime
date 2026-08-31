/** @file multi_window.h @brief Independent Catime window process support. */
#ifndef CATIME_MULTI_WINDOW_H
#define CATIME_MULTI_WINDOW_H

#include <windows.h>

#define CATIME_MAX_TIMER_WINDOWS 20

BOOL MultiWindow_IsSecondary(void);
BOOL MultiWindow_BeginMainWindowCreation(void);
void MultiWindow_EndMainWindowCreation(void);
void MultiWindow_OffsetInitialPosition(int* x, int* y);
int MultiWindow_GetOffsetForWindowIndex(int index);
BOOL MultiWindow_LaunchNewTimerWindow(HWND hwnd);

#endif
