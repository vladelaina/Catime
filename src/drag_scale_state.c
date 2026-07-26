/**
 * @file drag_scale_state.c
 * @brief Shared drag guards, window validation, and manual-position state.
 */

#include "drag_scale_internal.h"
#include "config.h"

DWORD TickElapsedMs(DWORD now, DWORD then) {
    return (DWORD)(now - then);
}

UINT GetScaleApplyInterval(HWND hwnd) {
    RECT rect = {0};
    if (!IsValidDragScaleWindow(hwnd) || !GetClientRect(hwnd, &rect)) {
        return SCALE_APPLY_INTERVAL_MS;
    }

    LONG width = rect.right - rect.left;
    LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return SCALE_APPLY_INTERVAL_MS;
    }

    ULONGLONG pixels = (ULONGLONG)(ULONG)width * (ULONGLONG)(ULONG)height;
    if (pixels >= SCALE_HUGE_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_HUGE_MS;
    }
    if (pixels >= SCALE_LARGE_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_LARGE_MS;
    }
    if (pixels >= SCALE_MEDIUM_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_MEDIUM_MS;
    }
    return SCALE_APPLY_INTERVAL_MS;
}

void ClearManualEditPosition(void) {
    g_manualEditPositionValid = FALSE;
    g_manualEditPositionHwnd = NULL;
    g_manualEditPosition.x = 0;
    g_manualEditPosition.y = 0;
}

void RecordManualEditPosition(HWND hwnd, int x, int y) {
    if (!CLOCK_EDIT_MODE || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    g_manualEditPositionValid = TRUE;
    g_manualEditPositionHwnd = hwnd;
    g_manualEditPosition.x = x;
    g_manualEditPosition.y = y;
}

void MarkManualEditWindowPosition(HWND hwnd) {
    RECT rect = {0};
    if (GetWindowRect(hwnd, &rect)) {
        RecordManualEditPosition(hwnd, rect.left, rect.top);
    }
}

void SuppressDragForDuration(DWORD durationMs) {
    DWORD until = GetTickCount() + durationMs;
    g_suppressDragUntilTick = until ? until : 1;
}

void SuppressDragAfterScale(void) {
    SuppressDragForDuration(SCALE_DRAG_SUPPRESS_MS);
}

BOOL IsDragSuppressedAfterScale(void) {
    if (g_suppressDragUntilTick == 0) {
        return FALSE;
    }

    if ((LONG)(GetTickCount() - g_suppressDragUntilTick) < 0) {
        return TRUE;
    }

    g_suppressDragUntilTick = 0;
    return FALSE;
}

BOOL IsLeftButtonPhysicallyDown(void) {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

void ClearDragBlockUntilLeftUp(void) {
    if (!g_dragBlockedUntilLeftUp) {
        return;
    }

    g_dragBlockedUntilLeftUp = FALSE;
    if (g_dragBlockNeedsReleaseCooldown && CLOCK_EDIT_MODE) {
        SuppressDragForDuration(SCALE_DRAG_RELEASE_SUPPRESS_MS);
    }
    g_dragBlockNeedsReleaseCooldown = FALSE;
}

BOOL IsDragBlockedUntilLeftUp(void) {
    if (!g_dragBlockedUntilLeftUp) {
        return FALSE;
    }

    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
        return FALSE;
    }

    return TRUE;
}

void BlockDragUntilLeftUp(HWND hwnd) {
    if (g_dragBlockedUntilLeftUp) {
        return;
    }

    if (!IsLeftButtonPhysicallyDown() &&
        !CLOCK_IS_DRAGGING &&
        GetCapture() != hwnd) {
        return;
    }

    g_dragBlockedUntilLeftUp = TRUE;
    g_dragBlockNeedsReleaseCooldown = TRUE;
}

void ClearDragAnchor(void) {
    g_dragAnchorValid = FALSE;
    g_dragStartCursorPos.x = 0;
    g_dragStartCursorPos.y = 0;
    ZeroMemory(&g_dragStartWindowRect, sizeof(g_dragStartWindowRect));
}

BOOL SetDragAnchorFromCurrentWindow(HWND hwnd, POINT cursorPos) {
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        ClearDragAnchor();
        return FALSE;
    }

    g_dragStartCursorPos = cursorPos;
    g_dragStartWindowRect = windowRect;
    g_dragAnchorValid = TRUE;
    return TRUE;
}

BOOL IsValidDragScaleWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

void RefreshWindow(HWND hwnd, BOOL eraseBackground) {
    InvalidateRect(hwnd, NULL, eraseBackground);
}
