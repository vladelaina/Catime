/**
 * @file dialog_website.c
 * @brief Timeout website input dialog.
 */

#include "dialog_info_internal.h"

char g_websiteInput[512] = {0};

static BOOL ConvertWideUrlToUtf8(const wchar_t* source, char* dest, size_t destSize) {
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

void ShowWebsiteDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_WEBSITE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_WEBSITE);
        Dialog_FocusControl(existing, CLOCK_IDC_EDIT, TRUE);
        return;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );

    if (hwndDlg) {
        DialogModern_ShowPaintedWindow(hwndDlg, SW_SHOW);
        Dialog_FocusControl(hwndDlg, CLOCK_IDC_EDIT, TRUE);
    }
}

INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_WEBSITE, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_WEBSITE, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            ctx->userData = (void*)lParam;
            Dialog_SetContext(hwndDlg, ctx);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                wchar_t wUrl[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1,
                                        wUrl, MAX_PATH) > 0) {
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wUrl);
                }
            }

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            DialogFormLayout_ApplyInstruction(
                hwndDlg, CLOCK_IDC_STATIC, CLOCK_IDC_EDIT,
                CLOCK_IDC_BUTTON_OK);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            SetFocus(hwndEdit);
            Dialog_SelectAllText(hwndEdit);

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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));

                if (Dialog_IsEmptyOrWhitespace(url)) {
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                /* Auto-prepend https:// if no protocol */
                if (wcsncmp(url, L"http://", 7) != 0 && wcsncmp(url, L"https://", 8) != 0) {
                    wchar_t tempUrl[MAX_PATH] = L"https://";
                    if (FAILED(StringCbCatW(tempUrl, sizeof(tempUrl), url)) ||
                        FAILED(StringCbCopyW(url, sizeof(url), tempUrl))) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                }

                char urlUtf8[MAX_PATH * 3] = {0};
                if (!ConvertWideUrlToUtf8(url, urlUtf8, sizeof(urlUtf8))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                if (!WriteConfigTimeoutWebsite(urlUtf8)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
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

        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_WEBSITE, hwndDlg);
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Font License Dialog Implementation
 * ============================================================================ */
