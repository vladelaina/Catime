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

#define ICON_MASK_STACK_BYTES 2048u
#define GENERATED_TRAY_ICON_FALLBACK_SIZE 16
#define GENERATED_TRAY_ICON_MAX_SIZE 256
#define GENERATED_PERCENT_ICON_MAX_VALUE 100
#define GENERATED_PERCENT_ICON_CACHE_SIZE (GENERATED_PERCENT_ICON_MAX_VALUE + 1)

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

static PercentIconCacheEntry g_percentIconCache[GENERATED_PERCENT_ICON_CACHE_SIZE];
static CapsIconCacheEntry g_capsIconCache[2];
static COLORREF g_cachedThemeTextColor = CLR_INVALID;
static DWORD g_lastThemeCheckTick = 0;
static INIT_ONCE g_percentIconCacheLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_percentIconCacheCS;

static BOOL CALLBACK InitPercentIconCacheLock(PINIT_ONCE initOnce, PVOID parameter, PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_percentIconCacheCS);
    return TRUE;
}

static BOOL BeginPercentIconCacheAccess(void) {
    if (!InitOnceExecuteOnce(&g_percentIconCacheLockOnce, InitPercentIconCacheLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_percentIconCacheCS);
    return TRUE;
}

static void EndPercentIconCacheAccess(void) {
    LeaveCriticalSection(&g_percentIconCacheCS);
}

static DWORD ColorRefToDibRgb(COLORREF color) {
    return ((DWORD)GetBValue(color)) |
           ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

static void GetGeneratedTrayIconSize(int* outCx, int* outCy) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    if (cy <= 0) cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    if (cx > GENERATED_TRAY_ICON_MAX_SIZE) cx = GENERATED_TRAY_ICON_MAX_SIZE;
    if (cy > GENERATED_TRAY_ICON_MAX_SIZE) cy = GENERATED_TRAY_ICON_MAX_SIZE;
    if (outCx) *outCx = cx;
    if (outCy) *outCy = cy;
}

void GetGeneratedTrayIconSizeSnapshot(int* outCx, int* outCy) {
    GetGeneratedTrayIconSize(outCx, outCy);
}

static void FillTransparentIconBackground(void* pvBits, int cx, int cy, DWORD marker) {
    DWORD* pixels = (DWORD*)pvBits;
    for (int i = 0; i < cx * cy; i++) {
        pixels[i] = marker;
    }
}

static void FillSolidIconBackground(void* pvBits, int cx, int cy, COLORREF bgColor) {
    DWORD* pixels = (DWORD*)pvBits;
    DWORD dibColor = ColorRefToDibRgb(bgColor);
    for (int i = 0; i < cx * cy; i++) {
        pixels[i] = dibColor;
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

static DWORD ComposeAlphaTextPixel(COLORREF color, BYTE alpha) {
    DWORD r = ((DWORD)GetRValue(color) * alpha + 127u) / 255u;
    DWORD g = ((DWORD)GetGValue(color) * alpha + 127u) / 255u;
    DWORD b = ((DWORD)GetBValue(color) * alpha + 127u) / 255u;
    return ((DWORD)alpha << 24) | (r << 16) | (g << 8) | b;
}

static BYTE GetMaskPixelAlpha(DWORD pixel) {
    BYTE r = (BYTE)((pixel >> 16) & 0xFF);
    BYTE g = (BYTE)((pixel >> 8) & 0xFF);
    BYTE b = (BYTE)(pixel & 0xFF);
    return (BYTE)(((unsigned int)r + (unsigned int)g + (unsigned int)b) / 3u);
}

static BOOL DrawAlphaTextOnTransparentIcon(HDC screenDc,
                                           void* targetBits,
                                           int cx,
                                           int cy,
                                           HFONT font,
                                           const wchar_t* text,
                                           int textLen,
                                           int x,
                                           int y,
                                           COLORREF textColor) {
    if (!screenDc || !targetBits || !font || !text || textLen <= 0 || cx <= 0 || cy <= 0) {
        return FALSE;
    }

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    VOID* maskBits = NULL;
    HBITMAP maskBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &maskBits, NULL, 0);
    if (!maskBitmap || !maskBits) {
        if (maskBitmap) DeleteObject(maskBitmap);
        return FALSE;
    }

    HDC maskDc = CreateCompatibleDC(screenDc);
    if (!maskDc) {
        DeleteObject(maskBitmap);
        return FALSE;
    }

    HGDIOBJ oldBitmap = SelectObject(maskDc, maskBitmap);
    if (!oldBitmap) {
        DeleteDC(maskDc);
        DeleteObject(maskBitmap);
        return FALSE;
    }

    ZeroMemory(maskBits, (size_t)cx * (size_t)cy * sizeof(DWORD));
    SetBkMode(maskDc, TRANSPARENT);
    SetTextColor(maskDc, RGB(255, 255, 255));
    HGDIOBJ oldFont = SelectObject(maskDc, font);
    TextOutW(maskDc, x, y, text, textLen);
    if (oldFont) {
        SelectObject(maskDc, oldFont);
    }

    DWORD* src = (DWORD*)maskBits;
    DWORD* dst = (DWORD*)targetBits;
    size_t count = (size_t)cx * (size_t)cy;
    for (size_t i = 0; i < count; ++i) {
        BYTE alpha = GetMaskPixelAlpha(src[i]);
        if (alpha != 0) {
            dst[i] = ComposeAlphaTextPixel(textColor, alpha);
        }
    }

    SelectObject(maskDc, oldBitmap);
    DeleteDC(maskDc);
    DeleteObject(maskBitmap);
    return TRUE;
}

static void GetSystemIconTextLogFont(LOGFONTW* lf, int pixelHeight, LONG weight) {
    if (!lf) return;

    ZeroMemory(lf, sizeof(*lf));
    NONCLIENTMETRICSW ncm;
    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);

    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0)) {
        *lf = ncm.lfStatusFont;
        if (lf->lfFaceName[0] == L'\0') {
            *lf = ncm.lfMessageFont;
        }
    }

    if (lf->lfFaceName[0] == L'\0') {
        wcscpy_s(lf->lfFaceName, _countof(lf->lfFaceName), L"Segoe UI");
        lf->lfCharSet = DEFAULT_CHARSET;
        lf->lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    }

    if (pixelHeight < 1) {
        pixelHeight = 1;
    }

    lf->lfHeight = -pixelHeight;
    lf->lfWidth = 0;
    lf->lfEscapement = 0;
    lf->lfOrientation = 0;
    lf->lfWeight = weight;
    lf->lfItalic = FALSE;
    lf->lfUnderline = FALSE;
    lf->lfStrikeOut = FALSE;
    lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    /*
     * ClearType can leave colored fringes on transparent tray icons, especially
     * on Windows 7. Grayscale antialiasing is more predictable for alpha icons.
     */
    lf->lfQuality = ANTIALIASED_QUALITY;
}

static HFONT CreateFittedIconTextFont(HDC hdc,
                                      const wchar_t* text,
                                      int textLen,
                                      int maxWidth,
                                      int maxHeight,
                                      LONG weight,
                                      int minPixelHeight,
                                      int maxPixelHeight,
                                      SIZE* outSize) {
    if (!hdc || !text || textLen <= 0) {
        return NULL;
    }

    if (maxWidth < 1) maxWidth = 1;
    if (maxHeight < 1) maxHeight = 1;
    if (minPixelHeight < 1) minPixelHeight = 1;
    if (maxPixelHeight < minPixelHeight) maxPixelHeight = minPixelHeight;

    HFONT fallbackFont = NULL;
    SIZE fallbackSize = {0};

    for (int pixelHeight = maxPixelHeight; pixelHeight >= minPixelHeight; --pixelHeight) {
        LOGFONTW lf;
        GetSystemIconTextLogFont(&lf, pixelHeight, weight);
        HFONT font = CreateFontIndirectW(&lf);
        if (!font) {
            continue;
        }

        HGDIOBJ oldFont = SelectObject(hdc, font);
        SIZE measured = {0};
        BOOL measuredOk = GetTextExtentPoint32W(hdc, text, textLen, &measured);
        if (oldFont) {
            SelectObject(hdc, oldFont);
        }

        if (!measuredOk) {
            DeleteObject(font);
            continue;
        }

        if (measured.cx <= maxWidth && measured.cy <= maxHeight) {
            if (outSize) {
                *outSize = measured;
            }
            if (font != fallbackFont && fallbackFont) {
                DeleteObject(fallbackFont);
            }
            return font;
        }

        if (font != fallbackFont) {
            if (fallbackFont) {
                DeleteObject(fallbackFont);
            }
            fallbackFont = font;
            fallbackSize = measured;
        }
    }

    if (outSize) {
        *outSize = fallbackSize;
    }
    return fallbackFont;
}

static HBITMAP CreateInitializedMaskBitmap(int cx, int cy, BYTE value) {
    if (cx <= 0 || cy <= 0) return NULL;

    SIZE_T stride = (SIZE_T)(((cx + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)cy;
    BYTE stackMaskBits[ICON_MASK_STACK_BYTES];
    BYTE* maskBits = size <= sizeof(stackMaskBits)
        ? stackMaskBits
        : (BYTE*)malloc(size);
    if (!maskBits) return NULL;

    memset(maskBits, value, size);
    HBITMAP hMask = CreateBitmap(cx, cy, 1, 1, maskBits);
    if (maskBits != stackMaskBits) free(maskBits);
    return hMask;
}

static void ClearGeneratedIconCacheLocked(void) {
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
    if (!BeginPercentIconCacheAccess()) return;
    ClearGeneratedIconCacheLocked();
    g_cachedThemeTextColor = CLR_INVALID;
    g_lastThemeCheckTick = 0;
    EndPercentIconCacheAccess();
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

static COLORREF QueryThemeTextColor(void) {
    return IsSystemDarkTheme()
        ? RGB(255, 255, 255)
        : RGB(0, 0, 0);
}

static BOOL TryGetCachedThemeTextColorLocked(COLORREF* textColor) {
    if (!textColor) return FALSE;
    DWORD now = GetTickCount();
    if (g_cachedThemeTextColor != CLR_INVALID && (now - g_lastThemeCheckTick) < 5000) {
        *textColor = g_cachedThemeTextColor;
        return TRUE;
    }

    return FALSE;
}

static void StoreThemeTextColorLocked(COLORREF textColor, DWORD tick) {
    g_cachedThemeTextColor = textColor;
    g_lastThemeCheckTick = tick;
}

static BOOL SnapshotIconColorsLocked(COLORREF* textColor, COLORREF* bgColor) {
    if (!textColor || !bgColor) return FALSE;

    *bgColor = g_percentBgColor;
    if (*bgColor == TRANSPARENT_BG_AUTO) {
        return TryGetCachedThemeTextColorLocked(textColor);
    }

    *textColor = g_percentTextColor;
    return TRUE;
}

static BOOL GetIconColorSnapshot(COLORREF* textColor, COLORREF* bgColor) {
    if (!textColor || !bgColor) return FALSE;

    if (!BeginPercentIconCacheAccess()) return FALSE;
    if (SnapshotIconColorsLocked(textColor, bgColor)) {
        EndPercentIconCacheAccess();
        return TRUE;
    }
    EndPercentIconCacheAccess();

    COLORREF themeTextColor = QueryThemeTextColor();
    DWORD themeTick = GetTickCount();

    if (!BeginPercentIconCacheAccess()) return FALSE;
    if (g_percentBgColor == TRANSPARENT_BG_AUTO) {
        StoreThemeTextColorLocked(themeTextColor, themeTick);
        *textColor = themeTextColor;
        *bgColor = TRANSPARENT_BG_AUTO;
    } else {
        *textColor = g_percentTextColor;
        *bgColor = g_percentBgColor;
    }
    EndPercentIconCacheAccess();
    return TRUE;
}

BOOL GetPercentIconColorSnapshot(COLORREF* textColor, COLORREF* bgColor) {
    return GetIconColorSnapshot(textColor, bgColor);
}

/**
 * @brief Set percent icon colors
 */
void SetPercentIconColors(COLORREF textColor, COLORREF bgColor) {
    if (!BeginPercentIconCacheAccess()) return;

    if (g_percentTextColor == textColor && g_percentBgColor == bgColor) {
        EndPercentIconCacheAccess();
        return;
    }

    ClearGeneratedIconCacheLocked();
    g_percentTextColor = textColor;
    g_percentBgColor = bgColor;
    EndPercentIconCacheAccess();
}

/**
 * @brief Get text color
 */
COLORREF GetPercentIconTextColor(void) {
    COLORREF textColor;
    if (!BeginPercentIconCacheAccess()) return RGB(0, 0, 0);
    textColor = g_percentTextColor;
    EndPercentIconCacheAccess();
    return textColor;
}

/**
 * @brief Get background color
 */
COLORREF GetPercentIconBgColor(void) {
    COLORREF bgColor;
    if (!BeginPercentIconCacheAccess()) return TRANSPARENT_BG_AUTO;
    bgColor = g_percentBgColor;
    EndPercentIconCacheAccess();
    return bgColor;
}

/**
 * @brief Create percent icon with text rendering
 */
static HICON CreatePercentIcon16Uncached(int percent,
                                         int cx,
                                         int cy,
                                         COLORREF textColor,
                                         COLORREF bgColor) {
    /* Clamp percent */
    if (percent > GENERATED_PERCENT_ICON_MAX_VALUE) {
        percent = GENERATED_PERCENT_ICON_MAX_VALUE;
    }
    if (percent < 0) percent = 0;

    BOOL useTransparentBg = (bgColor == TRANSPARENT_BG_AUTO);
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
        ZeroMemory(pvBits, (size_t)cx * (size_t)cy * sizeof(DWORD));
    } else {
        FillSolidIconBackground(pvBits, cx, cy, bgColor);
    }

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, textColor);

    /* Format text first to determine digit count */
    wchar_t txt[8];
    _snwprintf_s(txt, 8, _TRUNCATE, L"%d", percent);
    
    int txtLen = (int)wcsnlen(txt, _countof(txt));

    SIZE sz = {0};
    HFONT hFont = CreateFittedIconTextFont(mem, txt, txtLen,
                                           cx - 2, cy,
                                           FW_NORMAL,
                                           6, cy - 1,
                                           &sz);
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;
    if (!hFont) {
        GetTextExtentPoint32W(mem, txt, txtLen, &sz);
    }

    /* Center text */
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (useTransparentBg) {
        if (!DrawAlphaTextOnTransparentIcon(hdc, pvBits, cx, cy, hFont,
                                            txt, txtLen, x, y, textColor)) {
            FillTransparentIconBackground(pvBits, cx, cy, transparentMarker);
            TextOutW(mem, x, y, txt, txtLen);
            RepairTransparentIconAlpha(pvBits, cx, cy, transparentMarker);
        }
    } else {
        TextOutW(mem, x, y, txt, txtLen);
        MakeIconFullyOpaque(pvBits, cx, cy);
    }

    if (oldf) SelectObject(mem, oldf);

    SelectObject(mem, old);

    /* Create mask bitmap based on text rendering */
    HBITMAP hbmMask = CreateInitializedMaskBitmap(cx, cy, useTransparentBg ? 0xFF : 0x00);
    if (!hbmMask) {
        if (hFont) DeleteObject(hFont);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC memMask = CreateCompatibleDC(hdc);
    if (!memMask) {
        if (hFont) DeleteObject(hFont);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ oldMask = SelectObject(memMask, hbmMask);
    if (!oldMask) {
        if (hFont) DeleteObject(hFont);
        DeleteDC(memMask);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        /* Draw text in black on mask to mark opaque text pixels */
        SetBkMode(memMask, TRANSPARENT);
        SetTextColor(memMask, RGB(0, 0, 0));

        HFONT oldMaskFont = hFont ? (HFONT)SelectObject(memMask, hFont) : NULL;

        TextOutW(memMask, x, y, txt, txtLen);

        if (oldMaskFont) SelectObject(memMask, oldMaskFont);
    }

    SelectObject(memMask, oldMask);
    if (hFont) DeleteObject(hFont);
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
    int cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    int cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    GetGeneratedTrayIconSize(&cx, &cy);

    if (percent > GENERATED_PERCENT_ICON_MAX_VALUE) {
        percent = GENERATED_PERCENT_ICON_MAX_VALUE;
    }
    if (percent < 0) percent = 0;

    COLORREF textColor;
    COLORREF bgColor;
    if (!GetIconColorSnapshot(&textColor, &bgColor)) return NULL;

    if (!BeginPercentIconCacheAccess()) return NULL;

    PercentIconCacheEntry* entry = &g_percentIconCache[percent];
    if (entry->valid && entry->icon &&
        entry->textColor == textColor && entry->bgColor == bgColor &&
        entry->cx == cx && entry->cy == cy) {
        HICON result = CopyIcon(entry->icon);
        EndPercentIconCacheAccess();
        return result;
    }
    EndPercentIconCacheAccess();

    HICON generated = CreatePercentIcon16Uncached(percent, cx, cy, textColor, bgColor);
    if (!generated) {
        return NULL;
    }

    HICON result = generated;
    if (BeginPercentIconCacheAccess()) {
        COLORREF currentTextColor;
        COLORREF currentBgColor;
        if (SnapshotIconColorsLocked(&currentTextColor, &currentBgColor) &&
            currentTextColor == textColor && currentBgColor == bgColor) {
            HICON cachedIcon = CopyIcon(generated);
            entry = &g_percentIconCache[percent];
            if (cachedIcon) {
                if (entry->icon) {
                    DestroyIcon(entry->icon);
                }
                entry->icon = cachedIcon;
                entry->textColor = textColor;
                entry->bgColor = bgColor;
                entry->cx = cx;
                entry->cy = cy;
                entry->valid = TRUE;
            }
        }
        EndPercentIconCacheAccess();
    }

    return result;
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
static HICON CreateCapsLockIconUncached(BOOL capsOn,
                                        int cx,
                                        int cy,
                                        COLORREF textColor,
                                        COLORREF bgColor) {
    BOOL useTransparentBg = (bgColor == TRANSPARENT_BG_AUTO);
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
        ZeroMemory(pvBits, (size_t)cx * (size_t)cy * sizeof(DWORD));
    } else {
        FillSolidIconBackground(pvBits, cx, cy, bgColor);
    }

    /* Setup text rendering */
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, textColor);

    /* Display "A" or "a" based on Caps Lock state */
    const wchar_t* txt = capsOn ? L"A" : L"a";

    SIZE sz = {0};
    HFONT hFont = CreateFittedIconTextFont(mem, txt, 1,
                                           cx, cy,
                                           FW_NORMAL,
                                           7, cy,
                                           &sz);
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;
    if (!hFont) {
        GetTextExtentPoint32W(mem, txt, 1, &sz);
    }

    /* Center text */
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (useTransparentBg) {
        if (!DrawAlphaTextOnTransparentIcon(hdc, pvBits, cx, cy, hFont,
                                            txt, 1, x, y, textColor)) {
            FillTransparentIconBackground(pvBits, cx, cy, transparentMarker);
            TextOutW(mem, x, y, txt, 1);
            RepairTransparentIconAlpha(pvBits, cx, cy, transparentMarker);
        }
    } else {
        TextOutW(mem, x, y, txt, 1);
        MakeIconFullyOpaque(pvBits, cx, cy);
    }

    if (oldf) SelectObject(mem, oldf);

    SelectObject(mem, old);

    /* Create mask bitmap */
    HBITMAP hbmMask = CreateInitializedMaskBitmap(cx, cy, useTransparentBg ? 0xFF : 0x00);
    if (!hbmMask) {
        if (hFont) DeleteObject(hFont);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC memMask = CreateCompatibleDC(hdc);
    if (!memMask) {
        if (hFont) DeleteObject(hFont);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }
    HGDIOBJ oldMask = SelectObject(memMask, hbmMask);
    if (!oldMask) {
        if (hFont) DeleteObject(hFont);
        DeleteDC(memMask);
        ReleaseDC(NULL, hdc);
        DeleteDC(mem);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        return NULL;
    }

    if (useTransparentBg) {
        /* Draw text in black on mask */
        SetBkMode(memMask, TRANSPARENT);
        SetTextColor(memMask, RGB(0, 0, 0));

        HFONT oldMaskFont = hFont ? (HFONT)SelectObject(memMask, hFont) : NULL;

        TextOutW(memMask, x, y, txt, 1);

        if (oldMaskFont) SelectObject(memMask, oldMaskFont);
    }

    SelectObject(memMask, oldMask);
    if (hFont) DeleteObject(hFont);
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
    int cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    int cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    GetGeneratedTrayIconSize(&cx, &cy);

    COLORREF textColor;
    COLORREF bgColor;
    if (!GetIconColorSnapshot(&textColor, &bgColor)) return NULL;

    if (!BeginPercentIconCacheAccess()) return NULL;

    CapsIconCacheEntry* entry = &g_capsIconCache[capsOn ? 1 : 0];
    if (entry->valid && entry->icon &&
        entry->textColor == textColor && entry->bgColor == bgColor &&
        entry->cx == cx && entry->cy == cy && entry->capsOn == capsOn) {
        HICON result = CopyIcon(entry->icon);
        EndPercentIconCacheAccess();
        return result;
    }
    EndPercentIconCacheAccess();

    HICON generated = CreateCapsLockIconUncached(capsOn, cx, cy, textColor, bgColor);
    if (!generated) {
        return NULL;
    }

    HICON result = generated;
    if (BeginPercentIconCacheAccess()) {
        COLORREF currentTextColor;
        COLORREF currentBgColor;
        if (SnapshotIconColorsLocked(&currentTextColor, &currentBgColor) &&
            currentTextColor == textColor && currentBgColor == bgColor) {
            HICON cachedIcon = CopyIcon(generated);
            entry = &g_capsIconCache[capsOn ? 1 : 0];
            if (cachedIcon) {
                if (entry->icon) {
                    DestroyIcon(entry->icon);
                }
                entry->icon = cachedIcon;
                entry->textColor = textColor;
                entry->bgColor = bgColor;
                entry->cx = cx;
                entry->cy = cy;
                entry->capsOn = capsOn;
                entry->valid = TRUE;
            }
        }
        EndPercentIconCacheAccess();
    }

    return result;
}

