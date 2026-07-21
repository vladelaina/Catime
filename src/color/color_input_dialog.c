/**
 * @file color_input_dialog.c
 * @brief Text-based color input dialog with live preview (modeless version)
 */
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include "color/color_feedback.h"
#include "color/color_input_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "../resource/resource.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define COLOR_EDIT_ORIG_PROC_PROP L"Catime.ColorInput.OrigEditProc"
#define COLOR_EDIT_REFRESH_MESSAGE (WM_APP + 1)

typedef struct {
    ColorFeedbackResult feedback;
} ColorInputDialogState;

static ColorInputDialogState* GetColorInputDialogState(HWND hwndDlg) {
    return hwndDlg
        ? (ColorInputDialogState*)GetWindowLongPtrW(hwndDlg, DWLP_USER)
        : NULL;
}

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

/** Keep inline feedback, live preview, and submit availability in sync. */
static void UpdateColorFeedbackFromEdit(HWND hwndDlg) {
    ColorInputDialogState* state = GetColorInputDialogState(hwndDlg);
    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
    if (!state || !hwndEdit) return;

    char color[COLOR_BUFFER_SIZE] = {0};
    wchar_t wcolor[COLOR_BUFFER_SIZE] = {0};
    GetWindowTextW(hwndEdit, wcolor, sizeof(wcolor) / sizeof(wchar_t));

    HWND hwndMain = GetColorInputParent(hwndDlg);
    if (!ConvertColorInputToUtf8(wcolor, color, sizeof(color))) {
        ZeroMemory(&state->feedback, sizeof(state->feedback));
        state->feedback.kind = COLOR_FEEDBACK_INVALID;
    } else {
        ColorFeedback_Evaluate(color, &state->feedback);
    }

    if (hwndMain && ColorFeedback_IsValid(&state->feedback)) {
        StartPreview(PREVIEW_TYPE_COLOR, state->feedback.normalized, hwndMain);
    } else if (hwndMain) {
        CancelPreview(hwndMain);
    }

    DialogModern_SetFieldInvalid(
        hwndEdit, state->feedback.kind == COLOR_FEEDBACK_INVALID);
    InvalidateRect(GetDlgItem(hwndDlg, IDC_COLOR_INLINE_FEEDBACK), NULL, FALSE);
}

/* ============================================================================
 * Edit Control Subclass
 * ============================================================================ */

static LRESULT ColorEditCustomCallback(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam, BOOL* processed) {
    (void)wParam;
    (void)lParam;

    switch (msg) {
        case WM_SETTEXT:
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
            PostMessageW(hwnd, COLOR_EDIT_REFRESH_MESSAGE, 0, 0);
            break;

        case COLOR_EDIT_REFRESH_MESSAGE:
            UpdateColorFeedbackFromEdit(GetParent(hwnd));
            *processed = TRUE;
            return 0;
    }

    *processed = FALSE;
    return 0;
}

/** @brief Subclass procedure preserving shared edit keyboard behavior. */
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
    switch (msg) {
        case WM_INITDIALOG: {
            ColorInputDialogState* state =
                (ColorInputDialogState*)calloc(1, sizeof(*state));
            if (!state) {
                DestroyWindow(hwndDlg);
                return FALSE;
            }
            SetWindowLongPtrW(hwndDlg, DWLP_USER, (LONG_PTR)state);
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

            UpdateColorFeedbackFromEdit(hwndDlg);

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_EDIT &&
                HIWORD(wParam) == EN_CHANGE) {
                UpdateColorFeedbackFromEdit(hwndDlg);
                return TRUE;
            }
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                ColorInputDialogState* state =
                    GetColorInputDialogState(hwndDlg);
                if (!state) return TRUE;

                if (state->feedback.kind == COLOR_FEEDBACK_EMPTY) {
                    HWND hwndMain = GetColorInputParent(hwndDlg);
                    CancelPreview(hwndMain);
                    if (hwndMain) {
                        PostMessage(hwndMain, WM_DIALOG_COLOR, 0, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                if (ColorFeedback_IsValid(&state->feedback)) {
                    HWND hwndMain = GetColorInputParent(hwndDlg);
                    if (!hwndMain) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    StartPreview(PREVIEW_TYPE_COLOR,
                                 state->feedback.normalized, hwndMain);
                    if (!ApplyPreview(hwndMain)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    PostMessage(hwndMain, WM_DIALOG_COLOR, 1, 0);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                } else {
                    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                    SetFocus(hwndEdit);
                    Dialog_SelectAllText(hwndEdit);
                    MessageBeep(MB_ICONWARNING);
                    InvalidateRect(GetDlgItem(hwndDlg,
                                             IDC_COLOR_INLINE_FEEDBACK),
                                   NULL, FALSE);
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

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
            ColorInputDialogState* state = GetColorInputDialogState(hwndDlg);
            if (item && state && item->CtlID == IDC_COLOR_INLINE_FEEDBACK) {
                ColorFeedback_DrawInline(
                    hwndDlg, item, &state->feedback,
                    GetLocalizedString(NULL, L"Invalid input format"));
                return TRUE;
            }
            break;
        }

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
            free(GetColorInputDialogState(hwndDlg));
            SetWindowLongPtrW(hwndDlg, DWLP_USER, 0);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_COLOR, hwndDlg);
            break;
    }
    return FALSE;
}
