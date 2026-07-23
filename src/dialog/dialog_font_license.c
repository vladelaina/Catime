/**
 * @file dialog_font_license.c
 * @brief Custom-font license dialog and Markdown content.
 */

#include "dialog_info_internal.h"

static DialogMarkdownState* GetFontLicenseMarkdown(HWND hwndDlg) {
    return hwndDlg ? (DialogMarkdownState*)GetWindowLongPtrW(
                         hwndDlg, GWLP_USERDATA)
                   : NULL;
}

static void CleanupFontLicenseResources(HWND hwndDlg) {
    DialogMarkdownState* markdown = GetFontLicenseMarkdown(hwndDlg);
    SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
    DialogMarkdown_Destroy(markdown);
}

static BOOL IsValidFontLicenseParentWindow(HWND hwnd) {
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

static HWND GetFontLicenseParent(HWND hwndDlg) {
    HWND hwndParent = (HWND)GetPropW(hwndDlg, FONT_LICENSE_PARENT_PROP);
    return IsValidFontLicenseParentWindow(hwndParent) ? hwndParent : NULL;
}

static BOOL PostFontLicenseResult(HWND hwndDlg, WPARAM result) {
    HWND hwndParent = GetFontLicenseParent(hwndDlg);
    if (!hwndParent) {
        return FALSE;
    }

    return PostMessage(hwndParent, WM_DIALOG_FONT_LICENSE, result, 0) != 0;
}

INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            DialogMarkdownState* markdown = DialogMarkdown_Create();
            if (!markdown) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)markdown);
            Dialog_InitializeInstance(DIALOG_INSTANCE_FONT_LICENSE, hwndDlg);
            HWND hwndParent = (HWND)lParam;
            if (IsValidFontLicenseParentWindow(hwndParent)) {
                SetPropW(hwndDlg, FONT_LICENSE_PARENT_PROP, (HANDLE)hwndParent);
            }

            const wchar_t* title = GetLocalizedString(
                NULL,
                L"Custom Font Feature License Agreement"
            );
            SetWindowTextW(hwndDlg, title);

            const wchar_t* licenseText = GetLocalizedString(
                NULL,
                L"FontLicenseAgreementText"
            );

            if (!DialogMarkdown_Parse(markdown, licenseText, TRUE)) {
                CleanupFontLicenseResources(hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            const wchar_t* agreeText = GetLocalizedString(NULL, L"Agree");
            const wchar_t* cancelText = GetLocalizedString(NULL, L"Cancel");

            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_AGREE_BTN, agreeText);
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_CANCEL_BTN, cancelText);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FONT_LICENSE_AGREE_BTN:
                    CleanupFontLicenseResources(hwndDlg);
                    PostFontLicenseResult(hwndDlg, IDOK);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                case IDC_FONT_LICENSE_CANCEL_BTN:
                case IDCANCEL:
                    CleanupFontLicenseResources(hwndDlg);
                    PostFontLicenseResult(hwndDlg, IDCANCEL);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                case IDC_FONT_LICENSE_TEXT:
                    if (HIWORD(wParam) == STN_CLICKED) {
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(GetDlgItem(hwndDlg, IDC_FONT_LICENSE_TEXT), &pt);

                        if (DialogMarkdown_HandleClick(
                                GetFontLicenseMarkdown(hwndDlg), pt)) {
                            return TRUE;
                        }
                    }
                    return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CleanupFontLicenseResources(hwndDlg);
                PostFontLicenseResult(hwndDlg, IDCANCEL);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_FONT_LICENSE_TEXT) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;

                DialogModernPalette palette;
                DialogModern_CopyPalette(hwndDlg, &palette);
                HBRUSH surfaceBrush = CreateSolidBrush(palette.surface);
                if (surfaceBrush) {
                    FillRect(hdc, &rect, surfaceBrush);
                    DeleteObject(surfaceBrush);
                }
                RECT panelRect = rect;
                InflateRect(&panelRect, -1, -1);
                DialogModern_DrawRoundedRect(
                    hdc, &panelRect,
                    DialogModern_Scale(DialogModern_GetDpi(hwndDlg), 14),
                    palette.field, palette.border, 1);

                DialogMarkdownState* markdown =
                    GetFontLicenseMarkdown(hwndDlg);
                if (markdown) {
                    int oldBkMode = SetBkMode(hdc, TRANSPARENT);

                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;

                    RECT drawRect = panelRect;
                    int inset = DialogModern_Scale(
                        DialogModern_GetDpi(hwndDlg), 10);
                    InflateRect(&drawRect, -inset, -inset);

                    DialogMarkdown_Render(markdown, hdc, drawRect,
                                          palette.accent, palette.text);

                    if (hOldFont) {
                        SelectObject(hdc, hOldFont);
                    }
                    if (oldBkMode != 0) {
                        SetBkMode(hdc, oldBkMode);
                    }
                }

                return TRUE;
            }
            break;
        }

        case WM_DESTROY:
            CleanupFontLicenseResources(hwndDlg);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_FONT_LICENSE, hwndDlg);
            RemovePropW(hwndDlg, FONT_LICENSE_PARENT_PROP);
            break;

        case WM_CLOSE:
            CleanupFontLicenseResources(hwndDlg);
            PostFontLicenseResult(hwndDlg, IDCANCEL);
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}

void ShowFontLicenseDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_LICENSE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_FONT_LICENSE);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidFontLicenseParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FONT_LICENSE_DIALOG),
        hwndParent,
        FontLicenseDlgProc,
        (LPARAM)hwndParent
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}
