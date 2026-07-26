#include "tray_animation_percent_internal.h"

#include "drawing/system_ui_font.h"

void FillTransparentIconBackground(
    void* bits, int cx, int cy, DWORD marker) {
    DWORD* pixels = bits;
    for (int i = 0; i < cx * cy; i++) pixels[i] = marker;
}

void FillSolidIconBackground(
    void* bits, int cx, int cy, COLORREF bgColor) {
    DWORD* pixels = bits;
    DWORD dibColor = ColorRefToDibRgb(bgColor);
    for (int i = 0; i < cx * cy; i++) pixels[i] = dibColor;
}

void RepairTransparentIconAlpha(
    void* bits, int cx, int cy, DWORD marker) {
    DWORD* pixels = bits;
    for (int i = 0; i < cx * cy; i++) {
        if ((pixels[i] & 0x00FFFFFFu) != marker) {
            pixels[i] |= 0xFF000000u;
        } else {
            pixels[i] = 0;
        }
    }
}

void MakeIconFullyOpaque(void* bits, int cx, int cy) {
    DWORD* pixels = bits;
    for (int i = 0; i < cx * cy; i++) pixels[i] |= 0xFF000000u;
}

static DWORD ComposeAlphaTextPixel(COLORREF color, BYTE alpha) {
    DWORD red = ((DWORD)GetRValue(color) * alpha + 127u) / 255u;
    DWORD green = ((DWORD)GetGValue(color) * alpha + 127u) / 255u;
    DWORD blue = ((DWORD)GetBValue(color) * alpha + 127u) / 255u;
    return ((DWORD)alpha << 24) | (red << 16) | (green << 8) | blue;
}

static BYTE GetMaskPixelAlpha(DWORD pixel) {
    BYTE red = (BYTE)((pixel >> 16) & 0xFF);
    BYTE green = (BYTE)((pixel >> 8) & 0xFF);
    BYTE blue = (BYTE)(pixel & 0xFF);
    return (BYTE)(((unsigned int)red + green + blue) / 3u);
}

BOOL DrawAlphaTextOnTransparentIcon(
    HDC screenDc, void* targetBits, int cx, int cy,
    HFONT font, const wchar_t* text, int textLen,
    int x, int y, COLORREF textColor) {
    if (!screenDc || !targetBits || !font || !text || textLen <= 0 ||
        cx <= 0 || cy <= 0) return FALSE;

    BITMAPINFO bitmapInfo;
    ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = cx;
    bitmapInfo.bmiHeader.biHeight = -cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    VOID* maskBits = NULL;
    HBITMAP maskBitmap = CreateDIBSection(
        NULL, &bitmapInfo, DIB_RGB_COLORS, &maskBits, NULL, 0);
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
    if (oldFont) SelectObject(maskDc, oldFont);

    DWORD* source = maskBits;
    DWORD* target = targetBits;
    size_t count = (size_t)cx * (size_t)cy;
    for (size_t i = 0; i < count; ++i) {
        BYTE alpha = GetMaskPixelAlpha(source[i]);
        if (alpha != 0) target[i] = ComposeAlphaTextPixel(textColor, alpha);
    }
    SelectObject(maskDc, oldBitmap);
    DeleteDC(maskDc);
    DeleteObject(maskBitmap);
    return TRUE;
}

HFONT CreateFittedIconTextFont(
    HDC hdc, const wchar_t* text, int textLen,
    int maxWidth, int maxHeight, LONG weight,
    int minPixelHeight, int maxPixelHeight, SIZE* outSize) {
    if (!hdc || !text || textLen <= 0) return NULL;
    if (maxWidth < 1) maxWidth = 1;
    if (maxHeight < 1) maxHeight = 1;
    if (minPixelHeight < 1) minPixelHeight = 1;
    if (maxPixelHeight < minPixelHeight) maxPixelHeight = minPixelHeight;

    HFONT fallbackFont = NULL;
    SIZE fallbackSize = {0};
    for (int height = maxPixelHeight; height >= minPixelHeight; --height) {
        LOGFONTW logFont;
        InitializeSystemUiTextLogFont(&logFont, height, weight);
        HFONT font = CreateFontIndirectW(&logFont);
        if (!font) continue;
        HGDIOBJ oldFont = SelectObject(hdc, font);
        SIZE measured = {0};
        BOOL measuredOk = GetTextExtentPoint32W(
            hdc, text, textLen, &measured);
        if (oldFont) SelectObject(hdc, oldFont);
        if (!measuredOk) {
            DeleteObject(font);
            continue;
        }
        if (measured.cx <= maxWidth && measured.cy <= maxHeight) {
            if (outSize) *outSize = measured;
            if (fallbackFont) DeleteObject(fallbackFont);
            return font;
        }
        if (fallbackFont) DeleteObject(fallbackFont);
        fallbackFont = font;
        fallbackSize = measured;
    }
    if (outSize) *outSize = fallbackSize;
    return fallbackFont;
}

HBITMAP CreateInitializedMaskBitmap(int cx, int cy, BYTE value) {
    if (cx <= 0 || cy <= 0) return NULL;
    SIZE_T stride = (SIZE_T)(((cx + 15) / 16) * 2);
    SIZE_T size = stride * (SIZE_T)cy;
    BYTE stackBits[ICON_MASK_STACK_BYTES];
    BYTE* bits = size <= sizeof(stackBits) ? stackBits : malloc(size);
    if (!bits) return NULL;
    memset(bits, value, size);
    HBITMAP mask = CreateBitmap(cx, cy, 1, 1, bits);
    if (bits != stackBits) free(bits);
    return mask;
}
