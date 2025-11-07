/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview
 */
#include <stdio.h>
#include <ctype.h>
#include <windows.h>
#include "color/color_input_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "../resource/resource.h"

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern void ShowErrorDialog(HWND hwndParent);
extern void MoveDialogToPrimaryScreen(HWND hwndDlg);

/* ============================================================================
 * Static Variables
 * ============================================================================ */

static WNDPROC g_OldEditProc = NULL;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static BOOL IsEmptyOrWhitespaceA(const char* str) {
    if (!str || str[0] == '\0') return TRUE;
    for (int i = 0; str[i]; i++) {
        if (!isspace((unsigned char)str[i])) return FALSE;
    }
    return TRUE;
}

static void ShowErrorAndRefocus(HWND hwndDlg, int editControlId) {
    ShowErrorDialog(hwndDlg);
    HWND hwndEdit = GetDlgItem(hwndDlg, editControlId);
    if (hwndEdit) {
        SetFocus(hwndEdit);
        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
    }
}

/**
 * @brief Update color preview from edit control content
 * @param hwndEdit Edit control handle
 * 
 * @details Validates and normalizes input, updates preview on main window
 */
static void UpdateColorPreviewFromEdit(HWND hwndEdit) {
    char color[COLOR_BUFFER_SIZE];
    wchar_t wcolor[COLOR_BUFFER_SIZE];
    GetWindowTextW(hwndEdit, wcolor, sizeof(wcolor) / sizeof(wchar_t));
    WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);
    
    char normalized[COLOR_BUFFER_SIZE];
    normalizeColor(color, normalized, sizeof(normalized));
    
    HWND hwndMain = GetParent(GetParent(hwndEdit));
    
    if (normalized[0] == '#') {
        char finalColor[COLOR_HEX_BUFFER];
        ReplaceBlackColor(normalized, finalColor, sizeof(finalColor));
        StartPreview(PREVIEW_TYPE_COLOR, finalColor, hwndMain);
    } else {
        CancelPreview(hwndMain);
    }
}

/* ============================================================================
 * Edit Control Subclass
 * ============================================================================ */

LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        
        case WM_COMMAND:
            if (wParam == VK_RETURN) {
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                    return 0;
                }
            }
            break;
        
        case WM_CHAR:
            if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
                return 0;
            }
            {
                LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
                UpdateColorPreviewFromEdit(hwnd);
                return result;
            }
        
        case WM_PASTE:
        case WM_CUT: {
            LRESULT result = CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
            UpdateColorPreviewFromEdit(hwnd);
            return result;
        }
    }
    
    return CallWindowProc(g_OldEditProc, hwnd, msg, wParam, lParam);
}

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    
    switch (msg) {
        case WM_INITDIALOG: {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC,
                                                         (LONG_PTR)ColorEditSubclassProc);
                
                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    wchar_t wcolor[COLOR_BUFFER_SIZE];
                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TEXT_COLOR, -1, 
                                      wcolor, sizeof(wcolor) / sizeof(wchar_t));
                    SetWindowTextW(hwndEdit, wcolor);
                }
            }
            
            MoveDialogToPrimaryScreen(hwndDlg);
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                char color[COLOR_BUFFER_SIZE];
                wchar_t wcolor[COLOR_BUFFER_SIZE];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcolor, 
                              sizeof(wcolor) / sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);
                
                if (IsEmptyOrWhitespaceA(color)) {
                    CancelPreview(GetParent(hwndDlg));
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
                
                if (isValidColor(color)) {
                    /* Apply preview (saves to config automatically) */
                    HWND hwndMain = GetParent(hwndDlg);
                    ApplyPreview(hwndMain);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                } else {
                    ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                CancelPreview(GetParent(hwndDlg));
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

