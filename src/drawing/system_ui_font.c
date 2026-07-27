/**
 * @file system_ui_font.c
 * @brief Win7-compatible system UI font selection for compact text.
 */

#include "drawing/system_ui_font.h"

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
