#include "dialog/dialog_pomodoro.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_input.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "config.h"
#include "config/config_defaults.h"
#include "config/config_settings_api.h"
#include "timer/timer_events.h"
#include "window_procedure/window_procedure.h"
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <strsafe.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#define POMODORO_OPTIONS_MAX_INPUT_CHARS 512
#define POMODORO_OPTIONS_MAX_INPUT_BYTES ((POMODORO_OPTIONS_MAX_INPUT_CHARS * 4) + 1)
#define POMODORO_OPTIONS_TOKEN_DELIMITERS " \t\r\n"
#define POMODORO_LOOP_FOCUS_MESSAGE (WM_APP + 210)
#define POMODORO_LOOP_INFINITE_FONT_PROP L"Catime.PomodoroLoop.InfiniteFont"

static void LayoutInfiniteLoopControl(HWND hwndDlg) {
    HWND hwndInfinite = GetDlgItem(hwndDlg, IDC_POMODORO_LOOP_INFINITE);
    HWND hwndOk = GetDlgItem(hwndDlg, CLOCK_IDC_BUTTON_OK);
    if (!hwndInfinite || !hwndOk) return;

    UINT dpi = DialogModern_GetDpi(hwndDlg);
    RECT okRect = {0};
    if (!GetWindowRect(hwndOk, &okRect)) return;
    MapWindowPoints(NULL, hwndDlg, (LPPOINT)&okRect, 2);

    int gap = DialogModern_Scale(dpi, 6);
    int width = DialogModern_Scale(dpi, 50);
    SetWindowPos(hwndInfinite, NULL, okRect.left - gap - width, okRect.top,
                 width, okRect.bottom - okRect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    HFONT titleFont = (HFONT)GetPropW(
        hwndDlg, POMODORO_LOOP_INFINITE_FONT_PROP);
    if (!titleFont) {
        titleFont = DialogModern_CreateFont(dpi, 20, FW_SEMIBOLD);
        if (titleFont) {
            SetPropW(hwndDlg, POMODORO_LOOP_INFINITE_FONT_PROP,
                     (HANDLE)titleFont);
        }
    }
    if (titleFont) {
        SendMessageW(hwndInfinite, WM_SETFONT, (WPARAM)titleFont, TRUE);
    }
}

static void FocusPomodoroLoopInput(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return;
    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
    HWND hwndInfinite = GetDlgItem(hwndDlg, IDC_POMODORO_LOOP_INFINITE);
    if (!hwndEdit || !IsWindow(hwndEdit) ||
        !hwndInfinite || !IsWindow(hwndInfinite)) return;
    SetForegroundWindow(hwndDlg);
    if (IsWindowEnabled(hwndEdit)) {
        SetFocus(hwndEdit);
        Dialog_SelectAllText(hwndEdit);
    } else {
        SetFocus(hwndInfinite);
    }
}

static void LayoutPomodoroLoopPrompt(HWND hwndDlg) {
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    DialogModern_SetChildRect96(hwndDlg, CLOCK_IDC_STATIC, dpi,
                                10, 10, 288, 50);
    DialogModern_SetChildRect96(hwndDlg, CLOCK_IDC_EDIT, dpi,
                                10, 68, 288, 24);
}

static void UpdateInfiniteLoopControls(HWND hwndDlg) {
    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
    if (!hwndEdit || !IsWindow(hwndEdit)) return;

    BOOL isInfinite = IsDlgButtonChecked(
        hwndDlg, IDC_POMODORO_LOOP_INFINITE) == BST_CHECKED;
    EnableWindow(hwndEdit, !isInfinite);
}

static BOOL ParsePomodoroLoopCount(const wchar_t* input, int* loopCount) {
    if (!input || !loopCount) return FALSE;
    int value = 0;
    BOOL hasDigit = FALSE;
    for (int i = 0; input[i]; i++) {
        if (iswspace(input[i])) {
            continue;
        }
        if (!iswdigit(input[i])) {
            return FALSE;
        }
        hasDigit = TRUE;
        value = value * 10 + (int)(input[i] - L'0');
        if (value > 100) {
            return FALSE;
        }
    }
    if (!hasDigit || value < 1) {
        return FALSE;
    }
    *loopCount = value;
    return TRUE;
}
void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_POMODORO_LOOP)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_POMODORO_LOOP);
        Dialog_ActivateWindow(existing);
        PostMessageW(existing, POMODORO_LOOP_FOCUS_MESSAGE, 0, 0);
        return;
    }
    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
        hwndParent,
        PomodoroLoopDialogProc
    );
    if (hwndDlg) {
        DialogModern_ShowPaintedWindow(hwndDlg, SW_SHOW);
        LayoutInfiniteLoopControl(hwndDlg);
        SetForegroundWindow(hwndDlg);
        PostMessageW(hwndDlg, POMODORO_LOOP_FOCUS_MESSAGE, 0, 0);
    }
}
INT_PTR CALLBACK PomodoroLoopDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    DialogContext* ctx = Dialog_GetContext(hwndDlg);
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);
            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            Dialog_SetContext(hwndDlg, ctx);
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            BOOL isInfinite = PomodoroLoopCount_IsInfinite(
                g_AppConfig.pomodoro.loop_count);
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC,
                            GetLocalizedString(
                                NULL, L"Please enter loop count (1-100):"));
            LayoutPomodoroLoopPrompt(hwndDlg);
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);
            if (g_AppConfig.pomodoro.loop_count > 0) {
                wchar_t loopCountStr[16];
                _snwprintf_s(loopCountStr, 16, _TRUNCATE, L"%d", g_AppConfig.pomodoro.loop_count);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, loopCountStr);
            }
            CheckDlgButton(hwndDlg, IDC_POMODORO_LOOP_INFINITE,
                           isInfinite ? BST_CHECKED : BST_UNCHECKED);
            UpdateInfiniteLoopControls(hwndDlg);
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            FocusPomodoroLoopInput(hwndDlg);
            return FALSE;
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (Dialog_HandleColorMessages(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_POMODORO_LOOP_INFINITE &&
                HIWORD(wParam) == BN_CLICKED) {
                UpdateInfiniteLoopControls(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                if (IsDlgButtonChecked(hwndDlg,
                                      IDC_POMODORO_LOOP_INFINITE) == BST_CHECKED) {
                    if (!WriteConfigPomodoroLoopCount(
                            POMODORO_LOOP_COUNT_INFINITE)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg,
                                                   IDC_POMODORO_LOOP_INFINITE);
                        return TRUE;
                    }
                    (void)TimerEvents_SetActivePomodoroLoopCount(
                        POMODORO_LOOP_COUNT_INFINITE);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                if (Dialog_IsEmptyOrWhitespace(input_str)) {
                    if (PomodoroLoopCount_IsInfinite(
                            g_AppConfig.pomodoro.loop_count)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }
                int new_loop_count = 0;
                if (ParsePomodoroLoopCount(input_str, &new_loop_count)) {
                    int activeLoopCount = 0;
                    BOOL restartActiveInfinite =
                        TimerEvents_GetActivePomodoroLoopCount(&activeLoopCount) &&
                        PomodoroLoopCount_IsInfinite(activeLoopCount);
                    if (!WriteConfigPomodoroLoopCount(new_loop_count)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                    HWND hwndParent = Dialog_GetOwnerWindow(hwndDlg);
                    if (restartActiveInfinite && hwndParent &&
                        IsWindow(hwndParent)) {
                        StartPomodoroTimer(hwndParent);
                    }
                    DestroyWindow(hwndDlg);
                } else {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        case POMODORO_LOOP_FOCUS_MESSAGE:
            FocusPomodoroLoopInput(hwndDlg);
            return TRUE;
        case WM_THEMECHANGED:
            LayoutInfiniteLoopControl(hwndDlg);
            break;
        case WM_DESTROY:
            {
                HFONT titleFont = (HFONT)RemovePropW(
                    hwndDlg, POMODORO_LOOP_INFINITE_FONT_PROP);
                if (titleFont) {
                    DeleteObject(titleFont);
                }
            }
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);
            break;
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}
