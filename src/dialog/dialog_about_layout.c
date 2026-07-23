/**
 * @file dialog_about_layout.c
 * @brief About dialog links, layout, and icon rendering.
 */

#include "dialog_info_internal.h"

AboutLinkInfo g_aboutLinkInfos[] = {
    {IDC_CREDIT_LINK, NULL, L"Special thanks to 猫屋敷梨梨Official for the icon", L"https://space.bilibili.com/26087398"},
    {IDC_COPYRIGHT, NULL, L"Copyright © 2025-2026 vladelaina", L"https://vladelaina.com/"},
    {IDC_CREDITS, NULL, L"Credits", L"https://cati.me/#thanks"},
    {IDC_BILIBILI_LINK, NULL, L"BiliBili", L"https://space.bilibili.com/1862395225"},
    {IDC_GITHUB_LINK, NULL, L"GitHub", L"https://github.com/vladelaina/Catime"},
    {IDC_COPYRIGHT_LINK, NULL, L"Copyright Notice", L"https://github.com/vladelaina/Catime#️copyright-notice"},
    {IDC_SUPPORT, NULL, L"Discord", L"https://discord.com/invite/W3tW2gtp6g"}
};

const size_t g_aboutLinkInfoCount = sizeof(g_aboutLinkInfos) / sizeof(g_aboutLinkInfos[0]);

static LRESULT CALLBACK AboutLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL DialogInfo_IsAboutLinkControlId(UINT controlId) {
    for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
        if (g_aboutLinkInfos[i].controlId == controlId) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL DialogInfo_IsAboutFooterLinkControlId(UINT controlId) {
    return controlId != IDC_CREDIT_LINK && controlId != IDC_COPYRIGHT &&
           DialogInfo_IsAboutLinkControlId(controlId);
}

static WNDPROC GetAboutLinkOrigProc(HWND hwndLink) {
    return (WNDPROC)(LONG_PTR)GetPropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP);
}

static BOOL SubclassAboutLinkControl(HWND hwndLink) {
    if (!hwndLink || GetAboutLinkOrigProc(hwndLink)) {
        return hwndLink != NULL;
    }

    WNDPROC origProc = (WNDPROC)(LONG_PTR)SetWindowLongPtrW(
        hwndLink, GWLP_WNDPROC, (LONG_PTR)AboutLinkSubclassProc);
    if (!origProc) {
        return FALSE;
    }

    if (!SetPropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP, (HANDLE)(LONG_PTR)origProc)) {
        SetWindowLongPtrW(hwndLink, GWLP_WNDPROC, (LONG_PTR)origProc);
        return FALSE;
    }

    return TRUE;
}

static void UnsubclassAboutLinkControl(HWND hwndLink) {
    if (!hwndLink) return;

    WNDPROC origProc = GetAboutLinkOrigProc(hwndLink);
    if (!origProc) return;

    SetWindowLongPtrW(hwndLink, GWLP_WNDPROC, (LONG_PTR)origProc);
    RemovePropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP);
}

static LRESULT CALLBACK AboutLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = GetAboutLinkOrigProc(hwnd);
    if (!origProc) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_MOUSEMOVE:
            if (!GetPropW(hwnd, ABOUT_LINK_HOVER_PROP)) {
                TRACKMOUSEEVENT track = {0};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                SetPropW(hwnd, ABOUT_LINK_HOVER_PROP, (HANDLE)1);
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            RemovePropW(hwnd, ABOUT_LINK_HOVER_PROP);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                SendMessageW(GetParent(hwnd), WM_COMMAND,
                             MAKEWPARAM(GetDlgCtrlID(hwnd), STN_CLICKED),
                             (LPARAM)hwnd);
                return 0;
            }
            break;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;

        case WM_NCDESTROY:
            RemovePropW(hwnd, ABOUT_LINK_HOVER_PROP);
            UnsubclassAboutLinkControl(hwnd);
            return CallWindowProcW(origProc, hwnd, msg, wParam, lParam);
    }

    return CallWindowProcW(origProc, hwnd, msg, wParam, lParam);
}

void DialogInfo_ConfigureAboutLinkControls(HWND hwndDlg) {
    for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
        HWND hwndLink = GetDlgItem(hwndDlg, g_aboutLinkInfos[i].controlId);
        if (!hwndLink) continue;

        LONG_PTR style = GetWindowLongPtrW(hwndLink, GWL_STYLE);
        LONG_PTR exStyle = GetWindowLongPtrW(hwndLink, GWL_EXSTYLE);
        SetWindowLongPtrW(hwndLink, GWL_STYLE, style | WS_TABSTOP);
        SetWindowLongPtrW(hwndLink, GWL_EXSTYLE,
                          exStyle & ~WS_EX_TRANSPARENT);
        SetWindowPos(hwndLink, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SubclassAboutLinkControl(hwndLink);
    }
}

void DialogInfo_PaintAboutLinkBackground(HWND hwndDlg, const DRAWITEMSTRUCT* drawItem) {
    DialogModernPalette palette;
    DialogModern_CopyPalette(hwndDlg, &palette);
    RECT rect = drawItem->rcItem;
    HBRUSH brush = CreateSolidBrush(palette.surface);
    if (brush) {
        FillRect(drawItem->hDC, &rect, brush);
        DeleteObject(brush);
    }
}

/* ============================================================================
 * Global State
 * ============================================================================ */

static UINT GetAboutDialogDpi(HWND hwndDlg) {
    return DialogModern_GetDpi(hwndDlg);
}

static void SetAboutControlRect96(HWND hwndDlg, UINT controlId, UINT dpi,
                                  int x, int y, int width, int height) {
    DialogModern_SetChildRect96(hwndDlg, (int)controlId, dpi,
                                x, y, width, height);
}

static int MeasureAboutLinkWidth96(HWND control, UINT dpi) {
    wchar_t text[256] = {0};
    GetWindowTextW(control, text, (int)_countof(text));
    HDC hdc = GetDC(control);
    if (!hdc) return 44;
    HFONT font = (HFONT)SendMessageW(control, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    SIZE size = {0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(control, hdc);
    int width = MulDiv(size.cx, 96, (int)(dpi ? dpi : 96u)) + 14;
    return width < 44 ? 44 : width;
}

void DialogInfo_LayoutAboutDialogControls(HWND hwndDlg) {
    static const UINT footerLinks[] = {
        IDC_CREDITS,
        IDC_BILIBILI_LINK,
        IDC_GITHUB_LINK,
        IDC_COPYRIGHT_LINK,
        IDC_SUPPORT
    };
    UINT dpi = GetAboutDialogDpi(hwndDlg);
    SetAboutControlRect96(hwndDlg, IDC_ABOUT_ICON, dpi,
                          0, 4, ABOUT_MODERN_ICON_SIZE_96,
                          ABOUT_MODERN_ICON_SIZE_96);
    SetAboutControlRect96(hwndDlg, IDC_VERSION_TEXT, dpi, 112, 0, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_BUILD_DATE, dpi, 112, 26, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_COPYRIGHT, dpi, 112, 52, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_CREDIT_LINK, dpi, 112, 78, 336, 24);

    int widths[_countof(footerLinks)] = {0};
    int totalWidth = 0;
    const int gap = 12;
    for (size_t i = 0; i < _countof(footerLinks); i++) {
        HWND link = GetDlgItem(hwndDlg, (int)footerLinks[i]);
        widths[i] = link ? MeasureAboutLinkWidth96(link, dpi) : 44;
        totalWidth += widths[i];
    }
    totalWidth += gap * ((int)_countof(footerLinks) - 1);

    int availableForText = ABOUT_CONTENT_WIDTH_96 -
                           gap * ((int)_countof(footerLinks) - 1);
    int textWidth = totalWidth -
                    gap * ((int)_countof(footerLinks) - 1);
    if (textWidth > availableForText) {
        int remainingWidth = availableForText;
        int remainingIdealWidth = textWidth;
        for (size_t i = 0; i < _countof(footerLinks); i++) {
            int remainingItems = (int)_countof(footerLinks) - (int)i;
            int idealWidth = widths[i];
            int width = remainingItems == 1 ? remainingWidth :
                MulDiv(idealWidth, remainingWidth, remainingIdealWidth);
            int maximum = remainingWidth - (remainingItems - 1) * 36;
            if (width < 36) width = 36;
            if (width > maximum) width = maximum;
            widths[i] = width;
            remainingWidth -= width;
            remainingIdealWidth -= idealWidth;
        }
        totalWidth = ABOUT_CONTENT_WIDTH_96;
    }

    int x = (ABOUT_CONTENT_WIDTH_96 - totalWidth) / 2;
    for (size_t i = 0; i < _countof(footerLinks); i++) {
        SetAboutControlRect96(hwndDlg, footerLinks[i], dpi,
                              x, 122, widths[i], 24);
        x += widths[i] + gap;
    }
}

void DialogInfo_ReloadAboutDialogIcon(HWND hwndDlg) {
    UINT dpi = GetAboutDialogDpi(hwndDlg);
    int iconSize = DialogModern_Scale(dpi, ABOUT_MODERN_ICON_SIZE_96);
    if (iconSize <= 0) {
        iconSize = ABOUT_MODERN_ICON_SIZE_96;
    }

    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
                                    MAKEINTRESOURCEW(IDI_CATIME),
                                    IMAGE_ICON,
                                    iconSize,
                                    iconSize,
                                    LR_DEFAULTCOLOR);
    if (!hIcon) {
        return;
    }

    HICON hOldIcon = (HICON)SendDlgItemMessageW(hwndDlg, IDC_ABOUT_ICON,
                                                STM_SETICON,
                                                (WPARAM)hIcon, 0);
    if (hOldIcon && hOldIcon != hIcon) {
        DestroyIcon(hOldIcon);
    }
}

/* ============================================================================
 * About Dialog Implementation
 * ============================================================================ */
