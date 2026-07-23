/**
 * @file window_desktop_integration.c
 * @brief Shared lifetime cleanup for desktop integration components
 */
#include "window/window_desktop_integration.h"
#include "window_desktop_integration_internal.h"

void CleanupWindowDesktopIntegrationState(HWND hwnd) {
    WindowTopmostRetry_Cleanup(hwnd);
    WindowTopmostVisibility_Cleanup(hwnd);
}
