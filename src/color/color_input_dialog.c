/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview (modeless version)
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <windows.h>
#include "color/color_input_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define COLOR_EDIT_ORIG_PROC_PROP L"Catime.ColorInput.OrigEditProc"

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
    HWND hwndMain = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidColorInputParentWindow(hwndMain) ? hwndMain : NULL;
}

static BOOL ConvertColorInputToUtf8(const wchar_t* source, char* dest, size_t destSize) {
    if (!source || !dest || destSize == 0 || destSize > INT_MAX) {
        return FALSE;
    }

    dest[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > destSize) {
        return FALSE;
    }

    return WideCharToMultiByte(CP_UTF8, 0, source, -1, dest,
                               (int)destSize, NULL, NULL) > 0;
}

static WNDPROC GetColorEditOrigProc(HWND hwndEdit) {
    return (WNDPROC)(LONG_PTR)GetPropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP);
}

static BOOL SubclassColorEdit(HWND hwndEdit) {
    if (!hwndEdit) {
        return FALSE;
    }

    if (GetColorEditOrigProc(hwndEdit)) {
        return TRUE;
    }

    SetLastError(0);
    WNDPROC origProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC,
                                                 (LONG_PTR)ColorEditSubclassProc);
    if (!origProc && GetLastError() != 0) {
        return FALSE;
    }

    if (!SetPropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP, (HANDLE)(LONG_PTR)origProc)) {
        SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)origProc);
        return FALSE;
    }

    return TRUE;
}

static void UnsubclassColorEdit(HWND hwndEdit) {
    if (!hwndEdit) {
        return;
    }

    WNDPROC origProc = GetColorEditOrigProc(hwndEdit);
    if (!origProc) {
        return;
    }

    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)origProc);
    RemovePropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP);
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
    if (!ConvertColorInputToUtf8(wcolor, color, sizeof(color))) {
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
    WNDPROC origProc = GetColorEditOrigProc(hwnd);
    if (!origProc) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return Dialog_EditSubclassProc_Ex(hwnd, msg, wParam, lParam,
                                      ColorEditCustomCallback, origProc);
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
            Dialog_InitializeInstance(DIALOG_INSTANCE_COLOR, hwndDlg);

            /* Set localized dialog title and button text */
            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Set Color Value"));
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_BUTTON_OK, GetLocalizedString(NULL, L"OK"));
            
            /* Set localized format help text */
            SetDlgItemTextW(hwndDlg, IDC_COLOR_FORMAT_HELP, 
                           GetLocalizedString(NULL, L"ColorFormatHelp"));
            
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            if (hwndEdit) {
                SubclassColorEdit(hwndEdit);

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
                if (!ConvertColorInputToUtf8(wcolor, color, sizeof(color))) {
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
            UnsubclassColorEdit(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_COLOR, hwndDlg);
            break;
    }
    return FALSE;
}
