/**
 * @file dialog_common.c
 * @brief Common dialog infrastructure implementation
 */

#include "dialog/dialog_common.h"
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

/** Global edit proc for compatibility with legacy code */
static WNDPROC g_wpOrigEditProc = NULL;

/** Dialog instance registry */
static HWND g_dialogInstances[DIALOG_INSTANCE_COUNT] = {0};

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

void Dialog_SetContext(HWND hwndDlg, DialogContext* ctx) {
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)ctx);
}

DialogContext* Dialog_GetContext(HWND hwndDlg) {
    return (DialogContext*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
}

/* ============================================================================
 * Edit Control Subclassing
 * ============================================================================ */

LRESULT APIENTRY Dialog_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL firstKeyProcessed = FALSE;

    switch (msg) {
        case WM_SETFOCUS:
            PostMessage(hwnd, EM_SETSEL, 0, -1);
            firstKeyProcessed = FALSE;
            break;

        case WM_KEYDOWN:
            if (!firstKeyProcessed) {
                firstKeyProcessed = TRUE;
            }

            if (wParam == VK_RETURN) {
                HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
                SendMessage(GetParent(hwnd), WM_COMMAND, 
                           MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), 
                           (LPARAM)hwndOkButton);
                return 0;
            }
            
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == 1 || ((wParam == 'a' || wParam == 'A') && GetKeyState(VK_CONTROL) < 0)) {
                return 0;
            }
            if (wParam == VK_RETURN) {
                return 0;
            }
            break;
    }

    return CallWindowProc(g_wpOrigEditProc, hwnd, msg, wParam, lParam);
}

BOOL Dialog_SubclassEdit(HWND hwndEdit, DialogContext* ctx) {
    if (!hwndEdit || !ctx) return FALSE;
    
    WNDPROC origProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                  (LONG_PTR)Dialog_EditSubclassProc);
    if (!origProc) return FALSE;
    
    ctx->wpOrigEditProc = origProc;
    
    /* Set global for legacy compatibility */
    if (!g_wpOrigEditProc) {
        g_wpOrigEditProc = origProc;
    }
    
    return TRUE;
}

void Dialog_UnsubclassEdit(HWND hwndEdit, DialogContext* ctx) {
    if (!hwndEdit || !ctx || !ctx->wpOrigEditProc) return;

    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
    ctx->wpOrigEditProc = NULL;
}

/* ============================================================================
 * Extended Edit Control Subclassing
 * ============================================================================ */

LRESULT Dialog_EditSubclassProc_Ex(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                    Dialog_EditCustomCallback callback, WNDPROC origProc) {
    if (!origProc) return 0;

    BOOL processed = FALSE;
    LRESULT customResult = 0;

    if (callback) {
        customResult = callback(hwnd, msg, wParam, lParam, &processed);
        if (processed) {
            return customResult;
        }
    }

    static BOOL firstKeyProcessed = FALSE;

    switch (msg) {
        case WM_SETFOCUS:
            PostMessage(hwnd, EM_SETSEL, 0, -1);
            firstKeyProcessed = FALSE;
            break;

        case WM_KEYDOWN:
            if (!firstKeyProcessed) {
                firstKeyProcessed = TRUE;
            }

            if (wParam == VK_RETURN) {
                HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
                SendMessage(GetParent(hwnd), WM_COMMAND,
                           MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED),
                           (LPARAM)hwndOkButton);
                return 0;
            }

            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == 1 || ((wParam == 'a' || wParam == 'A') && GetKeyState(VK_CONTROL) < 0)) {
                return 0;
            }
            if (wParam == VK_RETURN) {
                return 0;
            }
            break;
    }

    return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
}

/* ============================================================================
 * Edit Control Helper Functions
 * ============================================================================ */

void Dialog_SelectAllText(HWND hwndEdit) {
    if (!hwndEdit || !IsWindow(hwndEdit)) return;
    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
}

void Dialog_InitEditWithValue(HWND hwndEdit, const wchar_t* initialValue) {
    if (!hwndEdit || !IsWindow(hwndEdit)) return;

    if (initialValue) {
        SetWindowTextW(hwndEdit, initialValue);
    } else {
        SetWindowTextW(hwndEdit, L"");
    }

    Dialog_SelectAllText(hwndEdit);
}

/* ============================================================================
 * Color Message Handling
 * ============================================================================ */

BOOL Dialog_HandleColorMessages(UINT msg, WPARAM wParam, DialogContext* ctx, INT_PTR* result) {
    if (!ctx || !result) return FALSE;
    
    switch (msg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, DIALOG_BG_COLOR);
            *result = (INT_PTR)ctx->hBackgroundBrush;
            return TRUE;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, EDIT_BG_COLOR);
            *result = (INT_PTR)ctx->hEditBrush;
            return TRUE;
            
        case WM_CTLCOLORBTN:
            SetBkColor((HDC)wParam, BUTTON_BG_COLOR);
            *result = (INT_PTR)ctx->hButtonBrush;
            return TRUE;
    }
    
    return FALSE;
}

/* ============================================================================
 * Dialog Positioning
 * ============================================================================ */

void Dialog_CenterOnPrimaryScreen(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }
    
    HMONITOR hPrimaryMonitor = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(hPrimaryMonitor, &mi)) {
        return;
    }
    
    RECT dialogRect;
    if (!GetWindowRect(hwndDlg, &dialogRect)) {
        return;
    }
    
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    
    int primaryWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int primaryHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    
    int newX = mi.rcMonitor.left + (primaryWidth - dialogWidth) / 2;
    int newY = mi.rcMonitor.top + (primaryHeight - dialogHeight) / 2;
    
    /* Move dialog to center position (TOPMOST is applied separately by Dialog_RegisterInstance) */
    SetWindowPos(hwndDlg, NULL, newX, newY, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Dialog_ApplyTopmost(HWND hwndDlg) {
    if (!hwndDlg) return;
    
    SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    extern void ShowErrorDialogWithRefocus(HWND hwndParent, int editControlId);
    
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
    wchar_t cleanStr[16] = {0};
    int cleanIndex = 0;
    
    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            cleanStr[cleanIndex++] = str[i];
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    
    return hasDigit;
}

/* ============================================================================
 * Global Dialog Instance Management
 * ============================================================================ */

void Dialog_RegisterInstance(DialogInstanceType type, HWND hwnd) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = hwnd;
    
    /* Auto-apply topmost to ensure dialog stays visible across virtual desktops */
    if (hwnd && IsWindow(hwnd)) {
        Dialog_ApplyTopmost(hwnd);
    }
}

void Dialog_UnregisterInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = NULL;
}

HWND Dialog_GetInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return NULL;
    return g_dialogInstances[type];
}

BOOL Dialog_IsOpen(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return FALSE;
    HWND hwnd = g_dialogInstances[type];
    return hwnd != NULL && IsWindow(hwnd);
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

