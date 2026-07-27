#include "tray_animation_percent_internal.h"

BOOL IsCapsLockOn(void) {
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

static HICON CreateCapsLockIconUncached(
    BOOL capsOn, int cx, int cy,
    COLORREF textColor, COLORREF bgColor) {
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
    if (transparent) ZeroMemory(
        bits, (size_t)cx * (size_t)cy * sizeof(DWORD));
    else FillSolidIconBackground(bits, cx, cy, bgColor);
    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, textColor);
    const wchar_t* text = capsOn ? L"A" : L"a";
    SIZE textSize = {0};
    HFONT font = CreateFittedIconTextFont(
        memoryDc, text, 1, cx, cy, FW_NORMAL, 7, cy, &textSize);
    HFONT oldFont = font ? (HFONT)SelectObject(memoryDc, font) : NULL;
    if (!font) GetTextExtentPoint32W(memoryDc, text, 1, &textSize);
    int x = (cx - textSize.cx) / 2;
    int y = (cy - textSize.cy) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    BOOL textDrawn = TRUE;
    if (transparent) {
        textDrawn = DrawAlphaTextOnTransparentIcon(
            screenDc, bits, cx, cy, font,
            text, 1, x, y, textColor);
        if (!textDrawn) {
            textDrawn = DrawFallbackTextOnTransparentIcon(
                memoryDc, bits, cx, cy, marker, font,
                text, 1, x, y, textColor);
        }
    } else {
        textDrawn = TextOutW(memoryDc, x, y, text, 1) && GdiFlush();
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
        (void)TextOutW(maskDc, x, y, text, 1);
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

HICON CreateCapsLockIcon(BOOL capsOn) {
    int cx = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    int cy = GENERATED_TRAY_ICON_FALLBACK_SIZE;
    GetGeneratedTrayIconSize(&cx, &cy);
    COLORREF textColor;
    COLORREF bgColor;
    if (!GetIconColorSnapshot(&textColor, &bgColor) ||
        !BeginPercentIconCacheAccess()) return NULL;
    CapsIconCacheEntry* entry = &g_capsIconCache[capsOn ? 1 : 0];
    if (entry->valid && entry->icon && entry->textColor == textColor &&
        entry->bgColor == bgColor && entry->cx == cx && entry->cy == cy &&
        entry->capsOn == capsOn) {
        HICON result = CopyIcon(entry->icon);
        EndPercentIconCacheAccess();
        return result;
    }
    EndPercentIconCacheAccess();
    HICON generated = CreateCapsLockIconUncached(
        capsOn, cx, cy, textColor, bgColor);
    if (!generated) return NULL;
    HICON result = generated;
    if (BeginPercentIconCacheAccess()) {
        COLORREF currentTextColor;
        COLORREF currentBgColor;
        if (SnapshotIconColorsLocked(
                &currentTextColor, &currentBgColor) &&
            currentTextColor == textColor && currentBgColor == bgColor) {
            HICON cached = CopyIcon(generated);
            entry = &g_capsIconCache[capsOn ? 1 : 0];
            if (cached) {
                if (entry->icon) DestroyIcon(entry->icon);
                entry->icon = cached;
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
