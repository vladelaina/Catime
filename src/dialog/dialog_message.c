/**
 * @file dialog_message.c
 * @brief Reusable modern modal message panel implementation.
 */

#include "dialog/dialog_message.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "../resource/resource.h"
#include <windows.h>

typedef struct {
    const wchar_t* title;
    const wchar_t* message;
    DialogInstanceType instanceType;
    BOOL trackInstance;
} DialogMessageParams;

static INT_PTR CALLBACK DialogMessageDlgProc(HWND hwndDlg, UINT msg,
                                             WPARAM wParam, LPARAM lParam);

static int DialogMessageMeasureHeight96(HWND hwndDlg,
                                        const wchar_t* message) {
    HWND textControl = GetDlgItem(hwndDlg, IDC_MESSAGE_TEXT);
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    int width = DialogModern_Scale(dpi, 360);
    int height = DialogModern_Scale(dpi, 32);
    if (!textControl) return 36;

    HDC hdc = GetDC(textControl);
    if (!hdc) return 36;
    HFONT font = (HFONT)SendMessageW(textControl, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    RECT measure = {0, 0, width, 0};
    DrawTextW(hdc, message ? message : L"", -1, &measure,
              DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(textControl, hdc);

    if (measure.bottom > height) height = measure.bottom;
    int height96 = MulDiv(height, 96, dpi ? dpi : 96) + 4;
    if (height96 < 36) height96 = 36;
    if (height96 > 240) height96 = 240;
    return height96;
}

static void DialogMessageLayout(HWND hwndDlg, const wchar_t* message) {
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    int height96 = DialogMessageMeasureHeight96(hwndDlg, message);
    DialogModern_SetChildRect96(hwndDlg, IDC_MESSAGE_TEXT, dpi,
                                0, 0, 360, height96);
    DialogModern_SetChildRect96(hwndDlg, IDOK, dpi,
                                280, height96 + 24, 80, 36);
}

static DialogInstanceType DialogMessageTypeForTone(DialogMessageTone tone) {
    switch (tone) {
        case DIALOG_MESSAGE_WARNING:
            return DIALOG_INSTANCE_MESSAGE_WARNING;
        case DIALOG_MESSAGE_ERROR:
            return DIALOG_INSTANCE_MESSAGE_ERROR;
        default:
            return DIALOG_INSTANCE_MESSAGE_INFO;
    }
}

static BOOL DialogMessageShowInternal(HWND hwndOwner,
                                      const wchar_t* title,
                                      const wchar_t* message,
                                      DialogInstanceType instanceType,
                                      BOOL trackInstance,
                                      BOOL returnShown) {
    if (hwndOwner && !IsWindow(hwndOwner)) hwndOwner = NULL;
    if (trackInstance && Dialog_IsOpen(instanceType)) {
        HWND existing = Dialog_GetInstance(instanceType);
        if (existing) SetForegroundWindow(existing);
        return FALSE;
    }

    DialogMessageParams params = {
        title ? title : L"Catime",
        message ? message : L"",
        instanceType,
        trackInstance
    };
    INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_MESSAGE_DIALOG),
        hwndOwner, DialogMessageDlgProc, (LPARAM)&params);
    if (returnShown) return result != -1;
    return result == IDOK;
}

BOOL DialogMessage_Show(HWND hwndOwner, const wchar_t* title,
                        const wchar_t* message, DialogMessageTone tone) {
    return DialogMessageShowInternal(
        hwndOwner, title, message, DialogMessageTypeForTone(tone), TRUE,
        FALSE);
}

BOOL DialogMessage_ShowNotification(HWND hwndOwner, const wchar_t* title,
                                    const wchar_t* message) {
    return DialogMessageShowInternal(
        hwndOwner, title, message, DIALOG_INSTANCE_MESSAGE_INFO, FALSE, TRUE);
}

static INT_PTR CALLBACK DialogMessageDlgProc(HWND hwndDlg, UINT msg,
                                             WPARAM wParam, LPARAM lParam) {
    DialogMessageParams* params = (DialogMessageParams*)
        GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);

    switch (msg) {
        case WM_INITDIALOG: {
            params = (DialogMessageParams*)lParam;
            if (!params) return FALSE;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)params);
            SetWindowTextW(hwndDlg, params->title);
            SetDlgItemTextW(hwndDlg, IDC_MESSAGE_TEXT, params->message);
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            DialogMessageLayout(hwndDlg, params->message);
            if (params->trackInstance) {
                Dialog_InitializeInstance(params->instanceType, hwndDlg);
            } else {
                DialogModern_Attach(hwndDlg, (int)params->instanceType);
                Dialog_ApplyTopmost(hwndDlg);
            }
            SetFocus(GetDlgItem(hwndDlg, IDOK));
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        case WM_DESTROY:
            if (params && params->trackInstance) {
                Dialog_UnregisterInstanceForWindow(params->instanceType,
                                                   hwndDlg);
            }
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            break;
    }
    return FALSE;
}
