#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>

extern BOOL PREVIOUS_TOPMOST_STATE;

BOOL HandleDragWindow(HWND hwnd);

BOOL HandleScaleWindow(HWND hwnd, int delta);

void StartDragWindow(HWND hwnd);

void EndDragWindow(HWND hwnd);

void StartEditMode(HWND hwnd);

void EndEditMode(HWND hwnd);

#endif