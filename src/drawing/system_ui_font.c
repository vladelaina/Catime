/**
 * @file system_ui_font.c
 * @brief Win7-compatible system UI font selection for compact text.
 */

#include "drawing/system_ui_font.h"

#include <limits.h>
#include <wchar.h>

#define SYSTEM_UI_METRIC_TEXT_POINT_SIZE 9

void InitializeSystemUiTextLogFont(
    LOGFONTW* logFont, int pixelHeight, LONG weight) {
    if (!logFont) return;
    ZeroMemory(logFont, sizeof(*logFont));

    NONCLIENTMETRICSW metrics;
    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(
            SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        *logFont = metrics.lfStatusFont;
        if (logFont->lfFaceName[0] == L'\0') {
            *logFont = metrics.lfMessageFont;
        }
    }
    if (logFont->lfFaceName[0] == L'\0') {
        wcscpy_s(logFont->lfFaceName, _countof(logFont->lfFaceName),
                 L"Segoe UI");
        logFont->lfCharSet = DEFAULT_CHARSET;
        logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    }

    if (pixelHeight < 1) pixelHeight = 1;
    logFont->lfHeight = -pixelHeight;
    logFont->lfWidth = 0;
    logFont->lfEscapement = 0;
    logFont->lfOrientation = 0;
    logFont->lfWeight = weight;
    logFont->lfItalic = FALSE;
    logFont->lfUnderline = FALSE;
    logFont->lfStrikeOut = FALSE;
    logFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
    logFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logFont->lfQuality = ANTIALIASED_QUALITY;
}

void InitializeSystemUiMetricTextLogFont(
    LOGFONTW* logFont, UINT dpi, BYTE quality) {
    if (dpi == 0) dpi = 96;
    int pixelHeight = MulDiv(
        SYSTEM_UI_METRIC_TEXT_POINT_SIZE, (int)dpi, 72);
    InitializeSystemUiTextLogFont(logFont, pixelHeight, FW_NORMAL);
    if (logFont) logFont->lfQuality = quality;
}

static int MeasureFontCellHeight(HDC dc, HFONT font) {
    if (!dc || !font) return 0;
    HGDIOBJ oldFont = SelectObject(dc, font);
    if (!oldFont || oldFont == HGDI_ERROR) return 0;
    TEXTMETRICW metrics = {0};
    BOOL measured = GetTextMetricsW(dc, &metrics);
    SelectObject(dc, oldFont);
    return measured ? metrics.tmHeight : 0;
}

HFONT CreateFittedSystemUiMetricTextFont(
    UINT dpi, BYTE quality, int maxCellHeight) {
    LOGFONTW logFont = {0};
    InitializeSystemUiMetricTextLogFont(&logFont, dpi, quality);
    int preferredPixelHeight = logFont.lfHeight < 0
        ? (int)-logFont.lfHeight : (int)logFont.lfHeight;
    if (preferredPixelHeight < 1) preferredPixelHeight = 1;
    if (maxCellHeight < 1) return CreateFontIndirectW(&logFont);

    HDC dc = GetDC(NULL);
    if (!dc) return CreateFontIndirectW(&logFont);
    int cellHeightLimit = maxCellHeight < INT_MAX
        ? maxCellHeight + 1 : maxCellHeight;
    for (int pixelHeight = preferredPixelHeight;
         pixelHeight >= 1; --pixelHeight) {
        logFont.lfHeight = -pixelHeight;
        HFONT font = CreateFontIndirectW(&logFont);
        if (!font) continue;
        int cellHeight = MeasureFontCellHeight(dc, font);
        /* GDI metric fonts can report one internal-leading pixel beyond the
         * requested row; clipping it does not clip the visible glyph. */
        if (cellHeight == 0 || cellHeight <= cellHeightLimit ||
            pixelHeight == 1) {
            ReleaseDC(NULL, dc);
            return font;
        }
        DeleteObject(font);
    }
    ReleaseDC(NULL, dc);
    return NULL;
}

HFONT CreateNonAntialiasedFontCopy(HFONT sourceFont) {
    LOGFONTW logFont = {0};
    if (!sourceFont ||
        GetObjectW(sourceFont, sizeof(logFont), &logFont) !=
            sizeof(logFont)) {
        return NULL;
    }
    logFont.lfQuality = NONANTIALIASED_QUALITY;
    return CreateFontIndirectW(&logFont);
}
