#ifndef MAIN_TIMER_INTERNAL_H
#define MAIN_TIMER_INTERNAL_H

#include <windows.h>

BOOL MainTimer_IsValidWindow(HWND hwnd);
void MainTimer_WaitForCallbacks(volatile LONG* activeCallbacks);

#endif
