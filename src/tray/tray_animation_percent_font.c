/**
 * @file tray_animation_percent_font.c
 * @brief Fitted system UI fonts for generated tray indicators.
 */

#include "tray_animation_percent_internal.h"

#include "drawing/system_ui_font.h"

static BOOL MeasureFont(
    HDC hdc, HFONT font, const wchar_t* text, int textLen,
    SIZE* size, TEXTMETRICW* metrics) {
    HGDIOBJ oldFont = SelectObject(hdc, font);
    if (!oldFont || oldFont == HGDI_ERROR) return FALSE;
    BOOL measured = GetTextExtentPoint32W(hdc, text, textLen, size);
    if (measured && metrics) measured = GetTextMetricsW(hdc, metrics);
    SelectObject(hdc, oldFont);
    return measured;
}

static HFONT CreateMeasuredFont(
    HDC hdc, const LOGFONTW* logFont,
    const wchar_t* text, int textLen,
    SIZE* size, TEXTMETRICW* metrics) {
    HFONT font = CreateFontIndirectW(logFont);
    if (!font) return NULL;
    if (!MeasureFont(hdc, font, text, textLen, size, metrics)) {
        DeleteObject(font);
        return NULL;
    }
    return font;
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
        SIZE measured = {0};
        HFONT font = CreateMeasuredFont(
            hdc, &logFont, text, textLen, &measured, NULL);
        if (!font) continue;
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

HFONT CreateFittedMetricIconTextFont(
    HDC hdc, const wchar_t* text, int textLen,
    int maxWidth, int maxHeight, UINT dpi, SIZE* outSize) {
    if (!hdc || !text || textLen <= 0) return NULL;
    if (maxWidth < 1) maxWidth = 1;
    if (maxHeight < 1) maxHeight = 1;

    LOGFONTW logFont;
    InitializeSystemUiMetricTextLogFont(
        &logFont, dpi, ANTIALIASED_QUALITY);
    int preferredHeight = -logFont.lfHeight;
    if (preferredHeight < 1) preferredHeight = 1;
    if (preferredHeight > maxHeight) preferredHeight = maxHeight;
    logFont.lfHeight = -preferredHeight;

    /* The 9pt taskbar font reports a cell one pixel taller than a 16px row;
     * clipping that internal-leading pixel preserves the visible glyph size. */
    int metricHeightLimit = maxHeight + 1;

    SIZE measured = {0};
    TEXTMETRICW metrics = {0};
    HFONT naturalFont = CreateMeasuredFont(
        hdc, &logFont, text, textLen, &measured, &metrics);
    if (naturalFont && measured.cx <= maxWidth &&
        measured.cy <= metricHeightLimit) {
        if (outSize) *outSize = measured;
        return naturalFont;
    }

    if (naturalFont && measured.cy <= metricHeightLimit &&
        measured.cx > maxWidth) {
        int naturalWidth = metrics.tmAveCharWidth;
        for (int width = naturalWidth - 1; width >= 1; --width) {
            logFont.lfWidth = width;
            SIZE fittedSize = {0};
            HFONT fittedFont = CreateMeasuredFont(
                hdc, &logFont, text, textLen, &fittedSize, NULL);
            if (!fittedFont) continue;
            if (fittedSize.cx <= maxWidth &&
                fittedSize.cy <= metricHeightLimit) {
                if (outSize) *outSize = fittedSize;
                DeleteObject(naturalFont);
                return fittedFont;
            }
            DeleteObject(fittedFont);
        }
    }
    if (naturalFont) DeleteObject(naturalFont);

    int minHeight = MulDiv(preferredHeight, 2, 3);
    if (minHeight < 1) minHeight = 1;
    return CreateFittedIconTextFont(
        hdc, text, textLen, maxWidth, maxHeight, FW_NORMAL,
        minHeight, preferredHeight, outSize);
}
