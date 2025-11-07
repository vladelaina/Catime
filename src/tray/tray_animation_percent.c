/**
 * @file tray_animation_percent.c
 * @brief Percent icon generation implementation
 */

#include "tray/tray_animation_percent.h"

/* Default colors */
static COLORREF g_percentTextColor = RGB(255, 255, 255);
static COLORREF g_percentBgColor = RGB(0, 0, 0);

/**
 * @brief Set percent icon colors
 */
void SetPercentIconColors(COLORREF textColor, COLORREF bgColor) {
    g_percentTextColor = textColor;
    g_percentBgColor = bgColor;
}

/**
 * @brief Get text color
 */
COLORREF GetPercentIconTextColor(void) {
    return g_percentTextColor;
}

/**
 * @brief Get background color
 */
COLORREF GetPercentIconBgColor(void) {
    return g_percentBgColor;
}

/**
 * @brief Create percent icon with text rendering
 */
HICON CreatePercentIcon16(int percent) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    /* Clamp percent */
    if (percent > 999) percent = 999;
    if (percent < 0) percent = 0;

    /* Create DIB section for color bitmap */
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    VOID* pvBits = NULL;
    HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (!hbmColor || !pvBits) {
        if (hbmColor) DeleteObject(hbmColor);
        return NULL;
    }

    /* Create mask bitmap */
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
    if (!hbmMask) {
        DeleteObject(hbmColor);
        return NULL;
    }

    /* Draw on color bitmap */
    HDC hdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, hbmColor);

    /* Fill background */
    RECT rc = {0, 0, cx, cy};
    HBRUSH bk = CreateSolidBrush(g_percentBgColor);
    FillRect(mem, &rc, bk);
    DeleteObject(bk);

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, g_percentTextColor);

    /* Create font: -12 size fits percentage text (0-100) within 16x16 tray icon */
    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;

    /* Format text */
    wchar_t txt[8];
    wsprintfW(txt, L"%d", percent);

    /* Center text */
    SIZE sz = {0};
    GetTextExtentPoint32W(mem, txt, lstrlenW(txt), &sz);
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;

    TextOutW(mem, x, y, txt, lstrlenW(txt));

    /* Cleanup font */
    if (oldf) SelectObject(mem, oldf);
    if (hFont) DeleteObject(hFont);

    SelectObject(mem, old);
    ReleaseDC(NULL, hdc);
    DeleteDC(mem);

    /* Create icon */
    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    
    HICON hIcon = CreateIconIndirect(&ii);
    
    DeleteObject(hbmMask);
    DeleteObject(hbmColor);
    
    return hIcon;
}

