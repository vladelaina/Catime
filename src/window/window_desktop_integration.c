/**
 * @file window_desktop_integration.c
 * @brief Desktop integration and Z-order management
 */

#include "window/window_desktop_integration.h"
#include "window/window_core.h"
#include "window/window_multimonitor.h"
#include "config.h"
#include "log.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Make window always-on-top
 * @param hwnd Window handle
 */
static void ApplyTopmostMode(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    LOG_INFO("Window set to topmost mode");
}

/**
 * @brief Make window normal z-order, parented to Progman
 * @param hwnd Window handle
 */
static void ApplyNormalMode(HWND hwnd) {
    HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
    if (hProgman) {
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
        LOG_INFO("Window parented to Progman for Win+D protection");
    }
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    LOG_INFO("Window set to normal mode");
}

/**
 * @brief Find WorkerW window containing SHELLDLL_DefView
 * @return WorkerW window handle, or Progman if WorkerW not found
 */
static HWND FindDesktopWorkerWindow(void) {
    HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
    if (!hProgman) return NULL;
    
    HWND hWorkerW = FindWindowExW(NULL, NULL, WORKERW_CLASS, NULL);
    while (hWorkerW) {
        HWND hView = FindWindowExW(hWorkerW, NULL, SHELLDLL_CLASS, NULL);
        if (hView) {
            LOG_INFO("Found desktop WorkerW window");
            return hWorkerW;
        }
        hWorkerW = FindWindowExW(NULL, hWorkerW, WORKERW_CLASS, NULL);
    }
    
    LOG_WARNING("Desktop WorkerW window not found");
    return hProgman;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void SetWindowTopmost(HWND hwnd, BOOL topmost) {
    LOG_INFO("Setting window topmost: %s", topmost ? "true" : "false");
    
    CLOCK_WINDOW_TOPMOST = topmost;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (topmost) {
        exStyle &= ~WS_EX_NOACTIVATE;
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        exStyle &= ~WS_EX_NOACTIVATE;
        
        HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
        if (hProgman) {
            SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
        } else {
            LOG_WARNING("Progman window not found, clearing parent");
            SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        }
        
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        AdjustWindowPosition(hwnd, TRUE);
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
    LOG_INFO("Window topmost setting applied and saved");
}

void ReattachToDesktop(HWND hwnd) {
    LOG_INFO("Reattaching window to desktop level");
    
    HWND hDesktop = FindDesktopWorkerWindow();
    
    if (hDesktop) {
        SetParent(hwnd, hDesktop);
        LOG_INFO("Window parented to desktop worker");
    } else {
        SetParent(hwnd, NULL);
        LOG_WARNING("Desktop worker not found, clearing parent");
    }
    
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

