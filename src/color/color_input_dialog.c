/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>
#include "color/color_input_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"

/* ============================================================================
 * Static Variables
 * ============================================================================ */

static WNDPROC g_OldEditProc = NULL;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Update color preview from edit control content
 * @param hwndEdit Edit control handle
 *
 * @details Validates and normalizes input, updates preview on main window
 * Supports both single colors and gradient colors
 */
static void UpdateColorPreviewFromEdit(HWND hwndEdit) {
    char color[COLOR_BUFFER_SIZE];
    wchar_t wcolor[COLOR_BUFFER_SIZE];
    GetWindowTextW(hwndEdit, wcolor, sizeof(wcolor) / sizeof(wchar_t));
    WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);

    HWND hwndMain = GetParent(GetParent(hwndEdit));

    /* Check if it's a gradient (contains underscore) */
    if (strchr(color, '_') != NULL) {
        /* Gradient color: validate and preview directly */
        if (isValidColorOrGradient(color)) {
            StartPreview(PREVIEW_TYPE_COLOR, color, hwndMain);
        } else {
            CancelPreview(hwndMain);
        }
    } else {
        /* Single color: normalize first */
        char normalized[COLOR_BUFFER_SIZE];
        normalizeColor(color, normalized, sizeof(normalized));

        if (normalized[0] == '#') {
            char finalColor[COLOR_HEX_BUFFER];
            ReplaceBlackColor(normalized, finalColor, sizeof(finalColor));
            StartPreview(PREVIEW_TYPE_COLOR, finalColor, hwndMain);
        } else {
            CancelPreview(hwndMain);
        }
    }
}

/* ============================================================================
 * Edit Control Subclass
 * ============================================================================ */

/**
 * @brief Custom callback for color preview updates
 */
static LRESULT ColorEditCustomCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, BOOL* pProcessed) {
    (void)wParam;
    (void)lParam;

    switch (msg) {
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
            *pProcessed = FALSE;
            PostMessage(hwnd, WM_APP + 1, 0, 0);
            return 0;

        case WM_APP + 1:
            UpdateColorPreviewFromEdit(hwnd);
            *pProcessed = TRUE;
            return 0;
    }

    *pProcessed = FALSE;
    return 0;
}

/**
 * @brief Subclass procedure for color edit control
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Dialog_EditSubclassProc_Ex(hwnd, msg, wParam, lParam,
                                      ColorEditCustomCallback, g_OldEditProc);
}

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG: {
            /* Set localized dialog title and button text */
            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Set Color Value"));
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_BUTTON_OK, GetLocalizedString(NULL, L"OK"));
            
            /* Set localized format help text */
            SetDlgItemTextW(hwndDlg, IDC_COLOR_FORMAT_HELP, 
                           GetLocalizedString(NULL, L"ColorFormatHelp"));
            
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC,
                                                         (LONG_PTR)ColorEditSubclassProc);

                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    wchar_t wcolor[COLOR_BUFFER_SIZE];
                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TEXT_COLOR, -1,
                                      wcolor, sizeof(wcolor) / sizeof(wchar_t));
                    Dialog_InitEditWithValue(hwndEdit, wcolor);
                } else {
                    Dialog_SelectAllText(hwndEdit);
                }
            }

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                char color[COLOR_BUFFER_SIZE];
                wchar_t wcolor[COLOR_BUFFER_SIZE];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcolor,
                              sizeof(wcolor) / sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, wcolor, -1, color, sizeof(color), NULL, NULL);

                if (Dialog_IsEmptyOrWhitespaceA(color)) {
                    CancelPreview(GetParent(hwndDlg));
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }

                /* Support both single colors and gradients */
                if (isValidColorOrGradient(color)) {
                    HWND hwndMain = GetParent(hwndDlg);
                    ApplyPreview(hwndMain);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                } else {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
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
