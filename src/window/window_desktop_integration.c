/**
 * @file window_desktop_integration.c
 * @brief Desktop integration and Z-order management
 */

#include "window/window_desktop_integration.h"
#include "window/window_core.h"
#include "config.h"
#include "log.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"
#define TOPMOST_REASSERT_INTERVAL_MS 2000

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static HWND FindDesktopWorkerWindow(void);
static BOOL IsValidWindowHandle(HWND hwnd, const char* caller);
static void ShowWindowNoActivateIfNeeded(HWND hwnd);

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

static BOOL IsValidWindowHandle(HWND hwnd, const char* caller) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_WARNING("%s called with invalid window handle", caller ? caller : "Topmost API");
        return FALSE;
    }
    return TRUE;
}

static void ShowWindowNoActivateIfNeeded(HWND hwnd) {
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
}

static BOOL TrySetWindowOwner(HWND hwnd, HWND owner) {
    SetLastError(0);
    LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    if (result == 0 && GetLastError() != 0) {
        LOG_WARNING("SetWindowLongPtr(GWLP_HWNDPARENT) failed (err=%lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL TrySetWindowExStyle(HWND hwnd, LONG exStyle) {
    SetLastError(0);
    LONG result = SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    if (result == 0 && GetLastError() != 0) {
        LOG_WARNING("SetWindowLong(GWL_EXSTYLE) failed (err=%lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL ApplyWindowTopmostStateInternal(HWND hwnd, BOOL topmost, BOOL persistConfig, BOOL updateRuntimeState) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    LONG newExStyle = exStyle;
    BOOL zOrderApplied = FALSE;
    DWORD zOrderError = ERROR_SUCCESS;

    if (topmost) {
        newExStyle &= ~WS_EX_NOACTIVATE;
        TrySetWindowOwner(hwnd, NULL);
        zOrderApplied = SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                                     SWP_FRAMECHANGED);
        if (!zOrderApplied) zOrderError = GetLastError();
    } else {
        newExStyle |= WS_EX_NOACTIVATE;

        HWND hDesktop = FindDesktopWorkerWindow();
        if (hDesktop) {
            TrySetWindowOwner(hwnd, hDesktop);
            LOG_INFO("Window parented to desktop anchor for Win+D protection");
        } else {
            LOG_WARNING("Desktop anchor not found, clearing parent");
            TrySetWindowOwner(hwnd, NULL);
        }

        zOrderApplied = SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (!zOrderApplied) zOrderError = GetLastError();
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindowNoActivateIfNeeded(hwnd);
    }

    TrySetWindowExStyle(hwnd, newExStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    if (zOrderApplied) {
        if (updateRuntimeState) {
            CLOCK_WINDOW_TOPMOST = topmost;
        }
        if (persistConfig) {
            WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
        }
        LOG_INFO("Window topmost state applied%s", persistConfig ? " and saved" : "");
    } else {
        if (updateRuntimeState) {
            LONG currentStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            CLOCK_WINDOW_TOPMOST = (currentStyle & WS_EX_TOPMOST) != 0;
        }
        LOG_WARNING("Failed to apply requested topmost state (err=%lu)", zOrderError);
    }

    return zOrderApplied;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void SetWindowTopmost(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmost")) return;

    LOG_INFO("Setting window topmost: %s", topmost ? "true" : "false");

    ApplyWindowTopmostStateInternal(hwnd, topmost, TRUE, TRUE);
}

void SetWindowTopmostTransient(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmostTransient")) return;

    LOG_INFO("Applying transient window topmost: %s", topmost ? "true" : "false");

    ApplyWindowTopmostStateInternal(hwnd, topmost, FALSE, TRUE);
}

void RefreshWindowTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "RefreshWindowTopmostState")) return;
    ApplyWindowTopmostStateInternal(hwnd, CLOCK_WINDOW_TOPMOST, FALSE, FALSE);
}

void EnsureWindowVisibleWithTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "EnsureWindowVisibleWithTopmostState")) return;

    ShowWindowNoActivateIfNeeded(hwnd);

    RefreshWindowTopmostState(hwnd);

    if (CLOCK_WINDOW_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

BOOL HandleTopmostMinimizeCommand(HWND hwnd, UINT sysCommand) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostMinimizeCommand")) return FALSE;

    UINT cmd = sysCommand & 0xFFF0;
    if (cmd != SC_MINIMIZE || !CLOCK_WINDOW_TOPMOST) {
        return FALSE;
    }

    LOG_INFO("Blocking minimize request while topmost mode is enabled");
    EnsureWindowVisibleWithTopmostState(hwnd);
    return TRUE;
}

BOOL HandleTopmostSizeEvent(HWND hwnd, WPARAM sizeType) {
    static BOOL s_restoringTopmostMinimize = FALSE;

    if (!IsValidWindowHandle(hwnd, "HandleTopmostSizeEvent")) return FALSE;
    if (sizeType != SIZE_MINIMIZED || !CLOCK_WINDOW_TOPMOST) return FALSE;
    if (s_restoringTopmostMinimize) return TRUE;

    s_restoringTopmostMinimize = TRUE;
    LOG_INFO("Topmost window was minimized, restoring visibility");
    EnsureWindowVisibleWithTopmostState(hwnd);
    s_restoringTopmostMinimize = FALSE;
    return TRUE;
}

void ReattachToDesktop(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "ReattachToDesktop")) return;

    LOG_INFO("Reattaching window to desktop level");
    
    HWND hDesktop = FindDesktopWorkerWindow();
    
    if (hDesktop) {
        TrySetWindowOwner(hwnd, hDesktop);
        LOG_INFO("Window owner set to desktop anchor");
    } else {
        TrySetWindowOwner(hwnd, NULL);
        LOG_WARNING("Desktop worker not found, owner cleared");
    }
    
    ShowWindowNoActivateIfNeeded(hwnd);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

BOOL EnforceTopmostOverTaskbar(HWND hwnd) {
    static DWORD s_lastTopmostReassertTick = 0;

    if (!IsValidWindowHandle(hwnd, "EnforceTopmostOverTaskbar")) return FALSE;

    /* Only enforce if topmost mode is enabled */
    if (!CLOCK_WINDOW_TOPMOST) return FALSE;
    
    /* Get our window position */
    RECT rcWindow;
    if (!GetWindowRect(hwnd, &rcWindow)) return FALSE;
    
    /* Get taskbar position */
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!hTaskbar) return FALSE;
    
    RECT rcTaskbar;
    if (!GetWindowRect(hTaskbar, &rcTaskbar)) return FALSE;
    
    /* Check if our window overlaps with taskbar area */
    BOOL overlaps = !(rcWindow.right < rcTaskbar.left ||
                      rcWindow.left > rcTaskbar.right ||
                      rcWindow.bottom < rcTaskbar.top ||
                      rcWindow.top > rcTaskbar.bottom);
    
    DWORD nowTick = GetTickCount();
    BOOL needPeriodicReassert = (nowTick - s_lastTopmostReassertTick) >= TOPMOST_REASSERT_INTERVAL_MS;

    if (overlaps || needPeriodicReassert) {
        if (SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
            s_lastTopmostReassertTick = nowTick;
        }
    }
    
    return overlaps;
}

