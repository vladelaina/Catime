/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview (modeless version)
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
static HWND g_hwndParent = NULL;

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

    HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(GetParent(hwndEdit));

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
 * Modeless Dialog API
 * ============================================================================ */

/**
 * @brief Show color input dialog (modeless)
 * @param hwndParent Parent window handle
 */
void ShowColorInputDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_COLOR)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_COLOR);
        SetForegroundWindow(existing);
        return;
    }

    g_hwndParent = hwndParent;

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG),
        hwndParent,
        ColorDlgProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_COLOR, hwndDlg);

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
                    HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(hwndDlg);
                    CancelPreview(hwndMain);
                    if (g_hwndParent) {
                        PostMessage(g_hwndParent, WM_DIALOG_COLOR, 0, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                /* Support both single colors and gradients */
                if (isValidColorOrGradient(color)) {
                    HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(hwndDlg);
                    ApplyPreview(hwndMain);
                    if (g_hwndParent) {
                        PostMessage(g_hwndParent, WM_DIALOG_COLOR, 1, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                } else {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(hwndDlg);
                CancelPreview(hwndMain);
                if (g_hwndParent) {
                    PostMessage(g_hwndParent, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(hwndDlg);
                CancelPreview(hwndMain);
                if (g_hwndParent) {
                    PostMessage(g_hwndParent, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            {
                HWND hwndMain = g_hwndParent ? g_hwndParent : GetParent(hwndDlg);
                CancelPreview(hwndMain);
                if (g_hwndParent) {
                    PostMessage(g_hwndParent, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
            }
            return TRUE;

        case WM_DESTROY:
            /* Restore original edit control procedure */
            if (g_OldEditProc) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)g_OldEditProc);
                }
                g_OldEditProc = NULL;
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_COLOR);
            g_hwndParent = NULL;
            break;
    }
    return FALSE;
}
