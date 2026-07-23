#ifndef WINDOW_DESKTOP_INTEGRATION_INTERNAL_H
#define WINDOW_DESKTOP_INTEGRATION_INTERNAL_H

#include <windows.h>

BOOL WindowDesktop_IsValid(HWND hwnd, const char* caller);
HWND WindowDesktop_FindWorkerWindow(void);
BOOL WindowDesktop_TrySetOwner(HWND hwnd, HWND owner);
BOOL WindowDesktop_TrySetNoActivate(HWND hwnd, BOOL noActivate);
BOOL WindowDesktop_IsTopmostStateApplied(HWND hwnd, BOOL topmost);
BOOL WindowDesktop_GetTopmostState(HWND hwnd, BOOL* topmost);
BOOL WindowDesktop_OverlapsAnyTaskbar(const RECT* windowRect);

BOOL WindowTopmost_ApplyInternal(HWND hwnd, BOOL topmost,
                                 BOOL persistConfig,
                                 BOOL updatePreference,
                                 BOOL updateRuntimeTarget,
                                 BOOL scheduleRetry);
void WindowTopmost_LogDiagnostics(HWND hwnd, const char* phase,
                                  BOOL requestedTopmost);

BOOL WindowTopmostRetry_IsCoolingDown(BOOL targetTopmost);
BOOL WindowTopmostRetry_Schedule(HWND hwnd, BOOL targetTopmost);
void WindowTopmostRetry_ResetForRequest(void);
void WindowTopmostRetry_Clear(HWND hwnd);
void WindowTopmostRetry_Cleanup(HWND hwnd);
void WindowTopmostVisibility_Cleanup(HWND hwnd);

#endif /* WINDOW_DESKTOP_INTEGRATION_INTERNAL_H */
