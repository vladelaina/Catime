/**
 * @file dialog_info.c
 * @brief About dialog lifecycle and message handling.
 */

#include "dialog_info_internal.h"

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
typedef HANDLE DPI_AWARENESS_CONTEXT;
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);

static GetThreadDpiAwarenessContextFunc LoadGetThreadDpiAwarenessContext(HMODULE module) {
    GetThreadDpiAwarenessContextFunc func = NULL;
    CATIME_LOAD_PROC_ADDRESS(module, "GetThreadDpiAwarenessContext", func);
    return func;
}

static SetThreadDpiAwarenessContextFunc LoadSetThreadDpiAwarenessContext(HMODULE module) {
    SetThreadDpiAwarenessContextFunc func = NULL;
    CATIME_LOAD_PROC_ADDRESS(module, "SetThreadDpiAwarenessContext", func);
    return func;
}

void ShowAboutDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_ABOUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_ABOUT);
        Dialog_ActivateWindow(existing);
        return;
    }

    /* DPI awareness for high-DPI displays */
    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc =
            LoadGetThreadDpiAwarenessContext(hUser32);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc =
            LoadSetThreadDpiAwarenessContext(hUser32);

        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {
            hOldDpiContext = getThreadDpiAwarenessContextFunc();
            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(IDD_ABOUT_DIALOG),
        hwndParent,
        AboutDlgProc
    );

    if (hwndDlg) {
        DialogModern_ShowPaintedWindow(hwndDlg, SW_SHOW);
    }

    if (hOldDpiContext && hUser32) {
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc =
            LoadSetThreadDpiAwarenessContext(hUser32);
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
}

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_ABOUT, hwndDlg);

            DialogInfo_ReloadAboutDialogIcon(hwndDlg);

            ApplyDialogLanguage(hwndDlg, IDD_ABOUT_DIALOG);

            /* The QQ group is intended for Simplified Chinese users only. It
             * must be hidden before the modern dialog host measures controls
             * so other languages do not reserve footer space for it. */
            if (GetCurrentLanguage() != APP_LANG_CHINESE_SIMP) {
                ShowWindow(GetDlgItem(hwndDlg, IDC_QQ_GROUP_LINK), SW_HIDE);
            }

            const wchar_t* versionFormat = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDC_VERSION_TEXT);
            if (versionFormat) {
                wchar_t versionText[256];
                StringCbPrintfW(versionText, sizeof(versionText), versionFormat, CATIME_VERSION);
                SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, versionText);
            }

            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            /* Convert build time from UTC+8 to local time */
            SYSTEMTIME buildTimeUTC8 = {0};
            buildTimeUTC8.wYear = (WORD)year;
            buildTimeUTC8.wMonth = (WORD)month_num;
            buildTimeUTC8.wDay = (WORD)day;
            buildTimeUTC8.wHour = (WORD)hour;
            buildTimeUTC8.wMinute = (WORD)min;
            buildTimeUTC8.wSecond = (WORD)sec;

            /* Convert to FILETIME (UTC+8 is 8 hours ahead of UTC) */
            FILETIME fileTime;
            SystemTimeToFileTime(&buildTimeUTC8, &fileTime);

            /* Subtract 8 hours to get UTC time */
            ULONGLONG utcTicks = (((ULONGLONG)fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
            utcTicks -= (ULONGLONG)8 * 60 * 60 * 10000000;  /* 8 hours in 100-nanosecond intervals */
            fileTime.dwLowDateTime = (DWORD)(utcTicks & 0xFFFFFFFFULL);
            fileTime.dwHighDateTime = (DWORD)(utcTicks >> 32);

            /* Convert to local time */
            FILETIME localFileTime;
            FileTimeToLocalFileTime(&fileTime, &localFileTime);

            SYSTEMTIME localTime;
            FileTimeToSystemTime(&localFileTime, &localTime);

            const wchar_t* buildDateLabel = GetLocalizedString(NULL, L"Build Date:");

            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), L"%s %04d/%02d/%02d %02d:%02d:%02d",
                    buildDateLabel, localTime.wYear, localTime.wMonth, localTime.wDay,
                    localTime.wHour, localTime.wMinute, localTime.wSecond);

            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                const wchar_t* linkText = GetLocalizedString(g_aboutLinkInfos[i].textCN, g_aboutLinkInfos[i].textEN);
                SetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, linkText);
            }
            DialogInfo_ConfigureAboutLinkControls(hwndDlg);
            DialogInfo_LayoutAboutDialogControls(hwndDlg);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_DPICHANGED:
            DialogInfo_ReloadAboutDialogIcon(hwndDlg);
            DialogInfo_LayoutAboutDialogControls(hwndDlg);
            break;

        case WM_DESTROY: {
            HICON hLargeIcon = (HICON)SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON,
                                                         STM_SETICON, 0, 0);
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_ABOUT, hwndDlg);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                if (LOWORD(wParam) == g_aboutLinkInfos[i].controlId && HIWORD(wParam) == STN_CLICKED) {
                    ShellExecuteW(NULL, L"open", g_aboutLinkInfos[i].url, NULL, NULL, SW_SHOWNORMAL);
                    return TRUE;
                }
            }
            break;

        case WM_SETCURSOR: {
            HWND hwndControl = (HWND)wParam;
            if (LOWORD(lParam) == HTCLIENT &&
                DialogInfo_IsAboutLinkControlId((UINT)GetDlgCtrlID(hwndControl))) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                if (lpDrawItem->CtlID == g_aboutLinkInfos[i].controlId) {
                    RECT rect = lpDrawItem->rcItem;
                    HDC hdc = lpDrawItem->hDC;

                    DialogInfo_PaintAboutLinkBackground(hwndDlg, lpDrawItem);

                    wchar_t text[256];
                    GetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, text, sizeof(text)/sizeof(text[0]));

                    HFONT hFont = (HFONT)SendMessageW(
                        lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    DialogModernPalette palette;
                    DialogModern_CopyPalette(hwndDlg, &palette);
                    BOOL active =
                        GetPropW(lpDrawItem->hwndItem, ABOUT_LINK_HOVER_PROP) ||
                        GetFocus() == lpDrawItem->hwndItem;
                    COLORREF color = active ? palette.accentHover :
                                              palette.accent;
                    UINT format = DT_VCENTER | DT_SINGLELINE |
                                  DT_END_ELLIPSIS;
                    format |= DialogInfo_IsAboutFooterLinkControlId(lpDrawItem->CtlID) ?
                              DT_CENTER : DT_LEFT;
                    DialogModern_DrawText(hdc, hFont, color, &rect, text,
                                          format);

                    if (active) {
                        HGDIOBJ oldFont = hFont ? SelectObject(hdc, hFont) : NULL;
                        SIZE textSize = {0};
                        GetTextExtentPoint32W(hdc, text, (int)wcslen(text),
                                              &textSize);
                        if (oldFont) SelectObject(hdc, oldFont);
                        int textWidth = textSize.cx;
                        int controlWidth = rect.right - rect.left;
                        if (textWidth > controlWidth) textWidth = controlWidth;
                        int left = DialogInfo_IsAboutFooterLinkControlId(lpDrawItem->CtlID) ?
                            rect.left + (controlWidth - textWidth) / 2 :
                            rect.left;
                        RECT underline = {
                            left,
                            rect.bottom - DialogModern_Scale(
                                DialogModern_GetDpi(hwndDlg), 2),
                            left + textWidth,
                            rect.bottom
                        };
                        DialogModern_DrawRoundedRect(
                            hdc, &underline,
                            DialogModern_Scale(DialogModern_GetDpi(hwndDlg), 2),
                            color, color, 0);
                    }
                    return TRUE;
                }
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Website Input Dialog Implementation
 * ============================================================================ */
