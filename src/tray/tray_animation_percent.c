/**
 * @file tray_animation_percent.c
 * @brief Percent icon generation implementation
 */

#include "tray/tray_animation_percent.h"

/* Default colors: auto-theme detection with black text */
static COLORREF g_percentTextColor = RGB(0, 0, 0);
static COLORREF g_percentBgColor = TRANSPARENT_BG_AUTO;

/**
 * @brief Detect if Windows is using dark theme
 * @return TRUE if dark theme, FALSE if light theme
 */
static BOOL IsSystemDarkTheme(void) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY hKey = NULL;

    /* Check Windows 10+ AppsUseLightTheme registry key */
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                     "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SystemUsesLightTheme", NULL, NULL,
                            (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (value == 0);  /* 0 = dark theme, 1 = light theme */
        }
        RegCloseKey(hKey);
    }

    /* Default to light theme if detection fails */
    return FALSE;
}

/**
 * @brief Get text color based on theme
 * @return Text color (white for dark theme, black for light theme)
 */
static COLORREF GetThemeTextColor(void) {
    if (IsSystemDarkTheme()) {
        return RGB(255, 255, 255);  /* White text for dark theme */
    } else {
        return RGB(0, 0, 0);  /* Black text for light theme */
    }
}

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

    /* Determine colors: use theme-based or user-configured */
    BOOL useTransparentBg = (g_percentBgColor == TRANSPARENT_BG_AUTO);
    COLORREF textColor = useTransparentBg ? GetThemeTextColor() : g_percentTextColor;
    COLORREF bgColor = g_percentBgColor;

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

    /* Draw on color bitmap */
    HDC hdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, hbmColor);

    if (useTransparentBg) {
        /* Transparent background: fill with transparent pixels (alpha=0) */
        DWORD* pixels = (DWORD*)pvBits;
        for (int i = 0; i < cx * cy; i++) {
            pixels[i] = 0x00000000;  /* Fully transparent */
        }
    } else {
        /* Solid background: use configured color */
        RECT rc = {0, 0, cx, cy};
        HBRUSH bk = CreateSolidBrush(bgColor);
        FillRect(mem, &rc, bk);
        DeleteObject(bk);
    }

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, textColor);

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

    /* Create mask bitmap based on text rendering */
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
    if (!hbmMask) {
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC memMask = CreateCompatibleDC(hdc);
    HGDIOBJ oldMask = SelectObject(memMask, hbmMask);

    if (useTransparentBg) {
        /* For transparent background: mask = white (1) for background, black (0) for text */
        /* First fill entire mask with white (transparent) */
        RECT rc = {0, 0, cx, cy};
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(memMask, &rc, whiteBrush);
        DeleteObject(whiteBrush);

        /* Draw text in black on mask to mark opaque text pixels */
        SetBkMode(memMask, TRANSPARENT);
        SetTextColor(memMask, RGB(0, 0, 0));

        HFONT hMaskFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldMaskFont = hMaskFont ? (HFONT)SelectObject(memMask, hMaskFont) : NULL;

        TextOutW(memMask, x, y, txt, lstrlenW(txt));

        if (oldMaskFont) SelectObject(memMask, oldMaskFont);
        if (hMaskFont) DeleteObject(hMaskFont);
    } else {
        /* For solid background: entire icon is opaque (mask all black) */
        RECT rc = {0, 0, cx, cy};
        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memMask, &rc, blackBrush);
        DeleteObject(blackBrush);
    }

    SelectObject(memMask, oldMask);
    DeleteDC(memMask);
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

