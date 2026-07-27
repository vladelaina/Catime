#include "tray_animation_percent_internal.h"

static HICON CreatePercentIcon16Uncached(
    int percent, int cx, int cy,
    COLORREF textColor, COLORREF bgColor) {
    if (percent > GENERATED_PERCENT_ICON_MAX_VALUE) percent = 100;
    if (percent < 0) percent = 0;
    BOOL transparent = bgColor == TRANSPARENT_BG_AUTO;
    DWORD marker = ColorRefToDibRgb(textColor) ^ 0x00010101u;

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = cx;
    info.bmiHeader.biHeight = -cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    VOID* bits = NULL;
    HBITMAP colorBitmap = CreateDIBSection(
        NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!colorBitmap || !bits) {
        if (colorBitmap) DeleteObject(colorBitmap);
        return NULL;
    }

    HDC screenDc = GetDC(NULL);
    HDC memoryDc = screenDc ? CreateCompatibleDC(screenDc) : NULL;
    if (!screenDc || !memoryDc) {
        if (memoryDc) DeleteDC(memoryDc);
        if (screenDc) ReleaseDC(NULL, screenDc);
        DeleteObject(colorBitmap);
        return NULL;
    }
    HGDIOBJ oldBitmap = SelectObject(memoryDc, colorBitmap);
    if (!oldBitmap) {
        DeleteDC(memoryDc);
        ReleaseDC(NULL, screenDc);
        DeleteObject(colorBitmap);
        return NULL;
    }
    if (transparent) {
        ZeroMemory(bits, (size_t)cx * (size_t)cy * sizeof(DWORD));
    } else {
        FillSolidIconBackground(bits, cx, cy, bgColor);
    }

    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, textColor);
    wchar_t text[8];
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%d", percent);
    int textLength = (int)wcsnlen(text, _countof(text));
    SIZE textSize = {0};
    UINT dpi = (UINT)GetDeviceCaps(memoryDc, LOGPIXELSY);
    HFONT font = CreateFittedMetricIconTextFont(
        memoryDc, text, textLength, cx - 1, cy, dpi, &textSize);
    HFONT oldFont = font ? (HFONT)SelectObject(memoryDc, font) : NULL;
    if (!font) GetTextExtentPoint32W(
        memoryDc, text, textLength, &textSize);
    int x = (cx - textSize.cx) / 2;
    int y = (cy - textSize.cy) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    BOOL textDrawn = TRUE;
    if (transparent) {
        textDrawn = DrawAlphaTextOnTransparentIcon(
            screenDc, bits, cx, cy, font,
            text, textLength, x, y, textColor);
        if (!textDrawn) {
            textDrawn = DrawFallbackTextOnTransparentIcon(
                memoryDc, bits, cx, cy, marker, font,
                text, textLength, x, y, textColor);
        }
    } else {
        textDrawn = TextOutW(memoryDc, x, y, text, textLength) &&
                    GdiFlush();
        if (textDrawn) MakeIconFullyOpaque(bits, cx, cy);
    }
    if (oldFont) SelectObject(memoryDc, oldFont);
    SelectObject(memoryDc, oldBitmap);
    if (!textDrawn) {
        if (font) DeleteObject(font);
        ReleaseDC(NULL, screenDc);
        DeleteDC(memoryDc);
        DeleteObject(colorBitmap);
        return NULL;
    }

    HBITMAP maskBitmap = CreateInitializedMaskBitmap(
        cx, cy, transparent ? 0xFF : 0x00);
    if (!maskBitmap) {
        if (font) DeleteObject(font);
        ReleaseDC(NULL, screenDc);
        DeleteDC(memoryDc);
        DeleteObject(colorBitmap);
        return NULL;
    }
    HDC maskDc = CreateCompatibleDC(screenDc);
    if (!maskDc) {
        if (font) DeleteObject(font);
        ReleaseDC(NULL, screenDc);
        DeleteDC(memoryDc);
        DeleteObject(maskBitmap);
        DeleteObject(colorBitmap);
        return NULL;
    }
    HGDIOBJ oldMask = SelectObject(maskDc, maskBitmap);
    if (!oldMask) {
        if (font) DeleteObject(font);
        DeleteDC(maskDc);
        ReleaseDC(NULL, screenDc);
        DeleteDC(memoryDc);
        DeleteObject(maskBitmap);
        DeleteObject(colorBitmap);
        return NULL;
    }
    if (transparent) {
        SetBkMode(maskDc, TRANSPARENT);
        SetTextColor(maskDc, RGB(0, 0, 0));
        HFONT oldMaskFont = font ? (HFONT)SelectObject(maskDc, font) : NULL;
        (void)TextOutW(maskDc, x, y, text, textLength);
        (void)GdiFlush();
        if (oldMaskFont) SelectObject(maskDc, oldMaskFont);
    }
    SelectObject(maskDc, oldMask);
    if (font) DeleteObject(font);
    DeleteDC(maskDc);
    ReleaseDC(NULL, screenDc);
    DeleteDC(memoryDc);

    ICONINFO iconInfo;
    ZeroMemory(&iconInfo, sizeof(iconInfo));
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);
    DeleteObject(maskBitmap);
    DeleteObject(colorBitmap);
    return icon;
}

HICON CreatePercentIcon16(int percent) {
    int cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    int cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    GetGeneratedTrayIconSize(&cx, &cy);
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;

    COLORREF textColor;
    COLORREF bgColor;
    if (!GetIconColorSnapshot(&textColor, &bgColor) ||
        !BeginPercentIconCacheAccess()) return NULL;
    PercentIconCacheEntry* entry = &g_percentIconCache[percent];
    if (entry->valid && entry->icon && entry->textColor == textColor &&
        entry->bgColor == bgColor && entry->cx == cx && entry->cy == cy) {
        HICON result = CopyIcon(entry->icon);
        EndPercentIconCacheAccess();
        return result;
    }
    EndPercentIconCacheAccess();

    HICON generated = CreatePercentIcon16Uncached(
        percent, cx, cy, textColor, bgColor);
    if (!generated) return NULL;
    HICON result = generated;
    if (BeginPercentIconCacheAccess()) {
        COLORREF currentTextColor;
        COLORREF currentBgColor;
        if (SnapshotIconColorsLocked(
                &currentTextColor, &currentBgColor) &&
            currentTextColor == textColor && currentBgColor == bgColor) {
            HICON cached = CopyIcon(generated);
            entry = &g_percentIconCache[percent];
            if (cached) {
                if (entry->icon) DestroyIcon(entry->icon);
                entry->icon = cached;
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
