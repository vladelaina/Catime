/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview (modeless version)
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "color/color_input_dialog.h"
#include "color/color_input_edit.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_form_layout.h"
#include "language.h"
#include "utils/string_convert.h"
#include "../resource/resource.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
static BOOL IsValidColorInputParentWindow(HWND hwnd) {
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

static HWND GetColorInputParent(HWND hwndDlg) {
    HWND hwndMain = Dialog_GetOwnerWindow(hwndDlg);
    return IsValidColorInputParentWindow(hwndMain) ? hwndMain : NULL;
}

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
    char color[COLOR_BUFFER_SIZE] = {0};
    wchar_t wcolor[COLOR_BUFFER_SIZE];
    GetWindowTextW(hwndEdit, wcolor, sizeof(wcolor) / sizeof(wchar_t));

    HWND hwndDlg = GetParent(hwndEdit);
    HWND hwndMain = GetColorInputParent(hwndDlg);
    if (!hwndMain) {
        return;
    }
    if (!WideToUtf8(wcolor, color, sizeof(color))) {
        CancelPreview(hwndMain);
        return;
    }

    char finalColor[COLOR_HEX_BUFFER];
    if (NormalizeColorConfigValue(color, finalColor, sizeof(finalColor))) {
        StartPreview(PREVIEW_TYPE_COLOR, finalColor, hwndMain);
        return;
    }

    CancelPreview(hwndMain);
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

    if (!IsValidColorInputParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG),
        hwndParent,
        ColorDlgProc
    );

    if (hwndDlg) {
        DialogModern_ShowPaintedWindow(hwndDlg, SW_SHOW);
    }
}

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_COLOR, hwndDlg);

            /* Set localized dialog title and button text */
            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Set Color Value"));
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_BUTTON_OK, GetLocalizedString(NULL, L"OK"));

            /* Set localized format help text */
            SetDlgItemTextW(hwndDlg, IDC_COLOR_FORMAT_HELP,
                           GetLocalizedString(NULL, L"ColorFormatHelp"));
            DialogFormLayout_ApplyInstruction(
                hwndDlg, IDC_COLOR_FORMAT_HELP, CLOCK_IDC_EDIT,
                CLOCK_IDC_BUTTON_OK);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                ColorInputEdit_Attach(hwndEdit);

                if (CLOCK_TEXT_COLOR[0] != '\0') {
                    wchar_t wcolor[COLOR_BUFFER_SIZE] = {0};
                    if (MultiByteToWideChar(CP_UTF8, 0, CLOCK_TEXT_COLOR, -1,
                                            wcolor, sizeof(wcolor) / sizeof(wchar_t)) > 0) {
                        Dialog_InitEditWithValue(hwndEdit, wcolor);
                    } else {
                        Dialog_SelectAllText(hwndEdit);
                    }
                } else {
                    Dialog_SelectAllText(hwndEdit);
                }
            }

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                char color[COLOR_BUFFER_SIZE] = {0};
                wchar_t wcolor[COLOR_BUFFER_SIZE];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcolor,
                              sizeof(wcolor) / sizeof(wchar_t));
                if (!WideToUtf8(wcolor, color, sizeof(color))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }

                if (Dialog_IsEmptyOrWhitespaceA(color)) {
                    HWND hwndMain = GetColorInputParent(hwndDlg);
                    CancelPreview(hwndMain);
                    if (hwndMain) {
                        PostMessage(hwndMain, WM_DIALOG_COLOR, 0, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                char finalColor[COLOR_HEX_BUFFER];
                if (NormalizeColorConfigValue(color, finalColor, sizeof(finalColor))) {
                    HWND hwndMain = GetColorInputParent(hwndDlg);
                    if (!hwndMain) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    StartPreview(PREVIEW_TYPE_COLOR, finalColor, hwndMain);
                    if (!ApplyPreview(hwndMain)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    PostMessage(hwndMain, WM_DIALOG_COLOR, 1, 0);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                } else {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                HWND hwndMain = GetColorInputParent(hwndDlg);
                CancelPreview(hwndMain);
                if (hwndMain) {
                    PostMessage(hwndMain, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case COLOR_INPUT_EDIT_CHANGED:
            UpdateColorPreviewFromEdit(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
            return TRUE;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                HWND hwndMain = GetColorInputParent(hwndDlg);
                CancelPreview(hwndMain);
                if (hwndMain) {
                    PostMessage(hwndMain, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            {
                HWND hwndMain = GetColorInputParent(hwndDlg);
                CancelPreview(hwndMain);
                if (hwndMain) {
                    PostMessage(hwndMain, WM_DIALOG_COLOR, 0, 0);
                }
                DestroyWindow(hwndDlg);
            }
            return TRUE;

        case WM_DESTROY:
            /* Restore original edit control procedure */
            ColorInputEdit_Detach(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_COLOR, hwndDlg);
            break;
    }
    return FALSE;
}
