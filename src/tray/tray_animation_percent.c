/**
 * @file tray_animation_percent.c
 * @brief Percent icon generation implementation
 */

#include "tray/tray_animation_percent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default colors: auto-theme detection with black text */
static COLORREF g_percentTextColor = RGB(0, 0, 0);
static COLORREF g_percentBgColor = TRANSPARENT_BG_AUTO;

typedef struct {
    HICON icon;
    COLORREF textColor;
    COLORREF bgColor;
    int cx;
    int cy;
    BOOL valid;
} PercentIconCacheEntry;

typedef struct {
    HICON icon;
    COLORREF textColor;
    COLORREF bgColor;
    int cx;
    int cy;
    BOOL capsOn;
    BOOL valid;
} CapsIconCacheEntry;

static PercentIconCacheEntry g_percentIconCache[1000];
static CapsIconCacheEntry g_capsIconCache[2];
static COLORREF g_cachedThemeTextColor = CLR_INVALID;
static DWORD g_lastThemeCheckTick = 0;

static DWORD ColorRefToDibRgb(COLORREF color) {
    return ((DWORD)GetBValue(color)) |
           ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

static void FillTransparentIconBackground(void* pvBits, int cx, int cy, DWORD marker) {
    DWORD* pixels = (DWORD*)pvBits;
    for (int i = 0; i < cx * cy; i++) {
        pixels[i] = marker;
    }
}

static void RepairTransparentIconAlpha(void* pvBits, int cx, int cy, DWORD marker) {
    DWORD* pixels = (DWORD*)pvBits;
    for (int i = 0; i < cx * cy; i++) {
        if ((pixels[i] & 0x00FFFFFFu) != marker) {
            pixels[i] |= 0xFF000000u;
        } else {
            pixels[i] = 0x00000000u;
        }
    }
}

static void MakeIconFullyOpaque(void* pvBits, int cx, int cy) {
    DWORD* pixels = (DWORD*)pvBits;
    for (int i = 0; i < cx * cy; i++) {
        pixels[i] |= 0xFF000000u;
    }
}

static HBITMAP CreateInitializedMaskBitmap(int cx, int cy, BYTE value) {
    if (cx <= 0 || cy <= 0) return NULL;

    SIZE_T stride = (SIZE_T)(((cx + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)cy;
    BYTE* maskBits = (BYTE*)malloc(size);
    if (!maskBits) return NULL;

    memset(maskBits, value, size);
    HBITMAP hMask = CreateBitmap(cx, cy, 1, 1, maskBits);
    free(maskBits);
    return hMask;
}

static void ClearGeneratedIconCache(void) {
    for (int i = 0; i < (int)_countof(g_percentIconCache); ++i) {
        if (g_percentIconCache[i].icon) {
            DestroyIcon(g_percentIconCache[i].icon);
        }
        ZeroMemory(&g_percentIconCache[i], sizeof(g_percentIconCache[i]));
    }

    for (int i = 0; i < (int)_countof(g_capsIconCache); ++i) {
        if (g_capsIconCache[i].icon) {
            DestroyIcon(g_capsIconCache[i].icon);
        }
        ZeroMemory(&g_capsIconCache[i], sizeof(g_capsIconCache[i]));
    }
}

void CleanupPercentIconCache(void) {
    ClearGeneratedIconCache();
    g_cachedThemeTextColor = CLR_INVALID;
    g_lastThemeCheckTick = 0;
}

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
    DWORD now = GetTickCount();
    if (g_cachedThemeTextColor != CLR_INVALID && (now - g_lastThemeCheckTick) < 5000) {
        return g_cachedThemeTextColor;
    }

    g_cachedThemeTextColor = IsSystemDarkTheme()
        ? RGB(255, 255, 255)
        : RGB(0, 0, 0);
    g_lastThemeCheckTick = now;
    return g_cachedThemeTextColor;
}

/**
 * @brief Set percent icon colors
 */
void SetPercentIconColors(COLORREF textColor, COLORREF bgColor) {
    if (g_percentTextColor == textColor && g_percentBgColor == bgColor) {
        return;
    }

    ClearGeneratedIconCache();
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
static HICON CreatePercentIcon16Uncached(int percent) {
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

    DWORD transparentMarker = ColorRefToDibRgb(textColor) ^ 0x00010101u;

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
    if (!hdc) {
        DeleteObject(hbmColor);
        return NULL;
    }
    HDC mem = CreateCompatibleDC(hdc);
    if (!mem) {
        ReleaseDC(NULL, hdc);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ old = SelectObject(mem, hbmColor);
    if (!old) {
        DeleteDC(mem);
        ReleaseDC(NULL, hdc);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        FillTransparentIconBackground(pvBits, cx, cy, transparentMarker);
    } else {
        /* Solid background: use configured color */
        RECT rc = {0, 0, cx, cy};
        HBRUSH bk = CreateSolidBrush(bgColor);
        if (bk) {
            FillRect(mem, &rc, bk);
            DeleteObject(bk);
        }
    }

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, textColor);

    /* Format text first to determine digit count */
    wchar_t txt[8];
    _snwprintf_s(txt, 8, _TRUNCATE, L"%d", percent);
    
    /* Dynamic font size: smaller for 3-digit numbers (100) */
    int fontSize = (percent >= 100) ? -9 : -12;

    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;

    int txtLen = (int)wcsnlen(txt, _countof(txt));


    /* Center text */
    SIZE sz = {0};
    GetTextExtentPoint32W(mem, txt, txtLen, &sz);
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;

    TextOutW(mem, x, y, txt, txtLen);

    if (useTransparentBg) {
        RepairTransparentIconAlpha(pvBits, cx, cy, transparentMarker);
    } else {
        MakeIconFullyOpaque(pvBits, cx, cy);
    }

    /* Cleanup font */
    if (oldf) SelectObject(mem, oldf);
    if (hFont) DeleteObject(hFont);

    SelectObject(mem, old);

    /* Create mask bitmap based on text rendering */
    HBITMAP hbmMask = CreateInitializedMaskBitmap(cx, cy, useTransparentBg ? 0xFF : 0x00);
    if (!hbmMask) {
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC memMask = CreateCompatibleDC(hdc);
    if (!memMask) {
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ oldMask = SelectObject(memMask, hbmMask);
    if (!oldMask) {
        DeleteDC(memMask);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        /* For transparent background: mask = white (1) for background, black (0) for text */
        /* First fill entire mask with white (transparent) */
        RECT rc = {0, 0, cx, cy};
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        if (whiteBrush) {
            FillRect(memMask, &rc, whiteBrush);
            DeleteObject(whiteBrush);
        }

        /* Draw text in black on mask to mark opaque text pixels */
        SetBkMode(memMask, TRANSPARENT);
        SetTextColor(memMask, RGB(0, 0, 0));

        HFONT hMaskFont = CreateFontW(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldMaskFont = hMaskFont ? (HFONT)SelectObject(memMask, hMaskFont) : NULL;

        TextOutW(memMask, x, y, txt, txtLen);

        if (oldMaskFont) SelectObject(memMask, oldMaskFont);
        if (hMaskFont) DeleteObject(hMaskFont);
    } else {
        /* For solid background: entire icon is opaque (mask all black) */
        RECT rc = {0, 0, cx, cy};
        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        if (blackBrush) {
            FillRect(memMask, &rc, blackBrush);
            DeleteObject(blackBrush);
        }
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

HICON CreatePercentIcon16(int percent) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    if (percent > 999) percent = 999;
    if (percent < 0) percent = 0;

    BOOL useTransparentBg = (g_percentBgColor == TRANSPARENT_BG_AUTO);
    COLORREF textColor = useTransparentBg ? GetThemeTextColor() : g_percentTextColor;
    COLORREF bgColor = g_percentBgColor;
    PercentIconCacheEntry* entry = &g_percentIconCache[percent];

    if (entry->valid && entry->icon &&
        entry->textColor == textColor && entry->bgColor == bgColor &&
        entry->cx == cx && entry->cy == cy) {
        return CopyIcon(entry->icon);
    }

    HICON generated = CreatePercentIcon16Uncached(percent);
    if (!generated) return NULL;

    if (entry->icon) {
        DestroyIcon(entry->icon);
    }
    entry->icon = generated;
    entry->textColor = textColor;
    entry->bgColor = bgColor;
    entry->cx = cx;
    entry->cy = cy;
    entry->valid = TRUE;

    return CopyIcon(entry->icon);
}

/**
 * @brief Check current Caps Lock state
 */
BOOL IsCapsLockOn(void) {
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

/**
 * @brief Create Caps Lock indicator icon
 */
static HICON CreateCapsLockIconUncached(BOOL capsOn) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    /* Determine colors: use theme-based or user-configured */
    BOOL useTransparentBg = (g_percentBgColor == TRANSPARENT_BG_AUTO);
    COLORREF textColor = useTransparentBg ? GetThemeTextColor() : g_percentTextColor;
    COLORREF bgColor = g_percentBgColor;
    DWORD transparentMarker = ColorRefToDibRgb(textColor) ^ 0x00010101u;

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
    if (!hdc) {
        DeleteObject(hbmColor);
        return NULL;
    }
    HDC mem = CreateCompatibleDC(hdc);
    if (!mem) {
        ReleaseDC(NULL, hdc);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ old = SelectObject(mem, hbmColor);
    if (!old) {
        DeleteDC(mem);
        ReleaseDC(NULL, hdc);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        FillTransparentIconBackground(pvBits, cx, cy, transparentMarker);
    } else {
        /* Solid background: use configured color */
        RECT rc = {0, 0, cx, cy};
        HBRUSH bk = CreateSolidBrush(bgColor);
        if (bk) {
            FillRect(mem, &rc, bk);
            DeleteObject(bk);
        }
    }

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, textColor);

    /* Display "A" or "a" based on Caps Lock state */
    const wchar_t* txt = capsOn ? L"A" : L"a";

    /* Font size - -13 is the standard system icon font size */
    int fontSize = -13;

    /* Use FW_NORMAL (400) to match system UI look, ANTIALIASED_QUALITY for smooth blending */
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;

    /* Center text */
    SIZE sz = {0};
    GetTextExtentPoint32W(mem, txt, 1, &sz);
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;

    TextOutW(mem, x, y, txt, 1);

    if (useTransparentBg) {
        RepairTransparentIconAlpha(pvBits, cx, cy, transparentMarker);
    } else {
        MakeIconFullyOpaque(pvBits, cx, cy);
    }

    /* Cleanup font */
    if (oldf) SelectObject(mem, oldf);
    if (hFont) DeleteObject(hFont);

    SelectObject(mem, old);

    /* Create mask bitmap */
    HBITMAP hbmMask = CreateInitializedMaskBitmap(cx, cy, useTransparentBg ? 0xFF : 0x00);
    if (!hbmMask) {
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC memMask = CreateCompatibleDC(hdc);
    if (!memMask) {
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ oldMask = SelectObject(memMask, hbmMask);
    if (!oldMask) {
        DeleteDC(memMask);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        /* For transparent background: mask = white for bg, black for text */
        RECT rc = {0, 0, cx, cy};
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        if (whiteBrush) {
            FillRect(memMask, &rc, whiteBrush);
            DeleteObject(whiteBrush);
        }

        /* Draw text in black on mask */
        SetBkMode(memMask, TRANSPARENT);
        SetTextColor(memMask, RGB(0, 0, 0));

        HFONT hMaskFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldMaskFont = hMaskFont ? (HFONT)SelectObject(memMask, hMaskFont) : NULL;

        TextOutW(memMask, x, y, txt, 1);

        if (oldMaskFont) SelectObject(memMask, oldMaskFont);
        if (hMaskFont) DeleteObject(hMaskFont);
    } else {
        /* For solid background: entire icon is opaque */
        RECT rc = {0, 0, cx, cy};
        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        if (blackBrush) {
            FillRect(memMask, &rc, blackBrush);
            DeleteObject(blackBrush);
        }
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

HICON CreateCapsLockIcon(BOOL capsOn) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    BOOL useTransparentBg = (g_percentBgColor == TRANSPARENT_BG_AUTO);
    COLORREF textColor = useTransparentBg ? GetThemeTextColor() : g_percentTextColor;
    COLORREF bgColor = g_percentBgColor;
    CapsIconCacheEntry* entry = &g_capsIconCache[capsOn ? 1 : 0];

    if (entry->valid && entry->icon &&
        entry->textColor == textColor && entry->bgColor == bgColor &&
        entry->cx == cx && entry->cy == cy && entry->capsOn == capsOn) {
        return CopyIcon(entry->icon);
    }

    HICON generated = CreateCapsLockIconUncached(capsOn);
    if (!generated) return NULL;

    if (entry->icon) {
        DestroyIcon(entry->icon);
    }
    entry->icon = generated;
    entry->textColor = textColor;
    entry->bgColor = bgColor;
    entry->cx = cx;
    entry->cy = cy;
    entry->capsOn = capsOn;
    entry->valid = TRUE;

    return CopyIcon(entry->icon);
}

