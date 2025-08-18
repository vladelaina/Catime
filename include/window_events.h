#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

BOOL HandleWindowCreate(HWND hwnd);

void HandleWindowDestroy(HWND hwnd);

void HandleWindowReset(HWND hwnd);

BOOL HandleWindowResize(HWND hwnd, int delta);

BOOL HandleWindowMove(HWND hwnd);

#endif