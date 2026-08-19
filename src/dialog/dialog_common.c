/**
 * @file dialog_common.c
 * @brief Common dialog infrastructure implementation
 */

#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_modern.h"
#include "window/window_placement.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DIALOG_BG_COLOR    RGB(0xF3, 0xF3, 0xF3)
#define EDIT_BG_COLOR      RGB(0xFF, 0xFF, 0xFF)
#define BUTTON_BG_COLOR    RGB(0xFD, 0xFD, 0xFD)

/* ============================================================================
 * Global State
 * ============================================================================ */


/* ============================================================================
 * Dialog Context Management
 * ============================================================================ */

DialogContext* Dialog_CreateContext(void) {
    DialogContext* ctx = (DialogContext*)calloc(1, sizeof(DialogContext));
    if (ctx) {
        ctx->hBackgroundBrush = CreateSolidBrush(DIALOG_BG_COLOR);
        ctx->hEditBrush = CreateSolidBrush(EDIT_BG_COLOR);
        ctx->hButtonBrush = CreateSolidBrush(BUTTON_BG_COLOR);
        ctx->wpOrigEditProc = NULL;
        ctx->userData = NULL;
    }
    return ctx;
}

void Dialog_FreeContext(DialogContext* ctx) {
    if (!ctx) return;

    if (ctx->hBackgroundBrush) DeleteObject(ctx->hBackgroundBrush);
    if (ctx->hEditBrush) DeleteObject(ctx->hEditBrush);
    if (ctx->hButtonBrush) DeleteObject(ctx->hButtonBrush);

    free(ctx);
}

static BOOL EnsureDialogBrush(HBRUSH* brush, COLORREF color) {
    if (!brush) {
        return FALSE;
    }
    if (!*brush) {
        *brush = CreateSolidBrush(color);
    }
    return *brush != NULL;
}

void Dialog_SetContext(HWND hwndDlg, DialogContext* ctx) {
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)ctx);
}

void Dialog_DestroyContext(HWND hwndDlg) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);
    if (!ctx) return;

    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
    Dialog_FreeContext(ctx);
}

DialogContext* Dialog_GetContext(HWND hwndDlg) {
    return (DialogContext*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
}

/* ============================================================================
 * Color Message Handling
 * ============================================================================ */

BOOL Dialog_HandleColorMessages(UINT msg, WPARAM wParam, DialogContext* ctx, INT_PTR* result) {
    if (!ctx || !result) return FALSE;

    switch (msg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            if (!EnsureDialogBrush(&ctx->hBackgroundBrush, DIALOG_BG_COLOR)) {
                return FALSE;
            }
            SetBkColor((HDC)wParam, DIALOG_BG_COLOR);
            *result = (INT_PTR)ctx->hBackgroundBrush;
            return TRUE;

        case WM_CTLCOLOREDIT:
            if (!EnsureDialogBrush(&ctx->hEditBrush, EDIT_BG_COLOR)) {
                return FALSE;
            }
            SetBkColor((HDC)wParam, EDIT_BG_COLOR);
            *result = (INT_PTR)ctx->hEditBrush;
            return TRUE;

        case WM_CTLCOLORBTN:
            if (!EnsureDialogBrush(&ctx->hButtonBrush, BUTTON_BG_COLOR)) {
                return FALSE;
            }
            SetBkColor((HDC)wParam, BUTTON_BG_COLOR);
            *result = (INT_PTR)ctx->hButtonBrush;
            return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Dialog Positioning
 * ============================================================================ */

static BOOL Dialog_GetUsableMonitorBounds(HMONITOR monitor, RECT* bounds) {
    if (!bounds) return FALSE;
    SetRectEmpty(bounds);

    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        *bounds = info.rcWork;
        if (bounds->right <= bounds->left ||
            bounds->bottom <= bounds->top) {
            *bounds = info.rcMonitor;
        }
    }

    if (bounds->right <= bounds->left || bounds->bottom <= bounds->top) {
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, bounds, 0) ||
            bounds->right <= bounds->left ||
            bounds->bottom <= bounds->top) {
            bounds->left = 0;
            bounds->top = 0;
            bounds->right = GetSystemMetrics(SM_CXSCREEN);
            bounds->bottom = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    return bounds->right > bounds->left && bounds->bottom > bounds->top;
}

void Dialog_CenterOnPrimaryScreen(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }

    HMONITOR hPrimaryMonitor = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    RECT bounds = {0};
    if (!Dialog_GetUsableMonitorBounds(hPrimaryMonitor, &bounds)) {
        return;
    }

    RECT dialogRect;
    if (!GetWindowRect(hwndDlg, &dialogRect)) {
        return;
    }

    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;

    int primaryWidth = bounds.right - bounds.left;
    int primaryHeight = bounds.bottom - bounds.top;

    int newX = bounds.left + (primaryWidth - dialogWidth) / 2;
    int newY = bounds.top + (primaryHeight - dialogHeight) / 2;
    WindowPlacement_ClampFullyVisible(
        &bounds, dialogWidth, dialogHeight, &newX, &newY);

    /* Moving the dialog must not change its current z-order. */
    SetWindowPos(hwndDlg, NULL, newX, newY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL Dialog_EnsureWindowVisible(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return FALSE;

    RECT rect = {0};
    if (!GetWindowRect(hwndDlg, &rect) ||
        rect.right <= rect.left || rect.bottom <= rect.top) {
        return FALSE;
    }

    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    RECT bounds = {0};
    if (!Dialog_GetUsableMonitorBounds(monitor, &bounds)) return FALSE;

    int x = rect.left;
    int y = rect.top;
    if (!WindowPlacement_ClampFullyVisible(
            &bounds, rect.right - rect.left, rect.bottom - rect.top,
            &x, &y)) {
        return FALSE;
    }
    if (x == rect.left && y == rect.top) return TRUE;

    return SetWindowPos(hwndDlg, NULL, x, y, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Dialog_ActivateWindow(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return;
    if (IsIconic(hwndDlg)) {
        ShowWindow(hwndDlg, SW_RESTORE);
    } else if (!IsWindowVisible(hwndDlg)) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
    Dialog_EnsureWindowVisible(hwndDlg);
    SetForegroundWindow(hwndDlg);
}

/* ============================================================================
 * Validation and Utilities
 * ============================================================================ */

BOOL Dialog_IsEmptyOrWhitespace(const wchar_t* str) {
    if (!str || str[0] == L'\0') {
        return TRUE;
    }
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL Dialog_IsEmptyOrWhitespaceA(const char* str) {
    if (!str || str[0] == '\0') {
        return TRUE;
    }
    for (int i = 0; str[i]; i++) {
        if (!isspace((unsigned char)str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

void Dialog_ShowErrorAndRefocus(HWND hwndDlg, int editControlId) {
    /* Use modeless error dialog with automatic refocus on close */
    ShowErrorDialogWithRefocus(hwndDlg, editControlId);
}

void Dialog_FormatSecondsToString(int totalSeconds, char* buffer, size_t bufferSize) {
    TimeParser_FormatToString(totalSeconds, buffer, bufferSize);
}

BOOL Dialog_IsValidNumberInput(const wchar_t* str) {
    if (!str || !*str) {
        return FALSE;
    }

    BOOL hasDigit = FALSE;

    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {
            return FALSE;
        }
    }

    return hasDigit;
}

/* ============================================================================
 * Legacy Compatibility
 * ============================================================================ */

/**
 * @brief Backward compatibility wrapper for Dialog_CenterOnPrimaryScreen
 * @param hwndDlg Dialog handle
 */
void MoveDialogToPrimaryScreen(HWND hwndDlg) {
    Dialog_CenterOnPrimaryScreen(hwndDlg);
}
