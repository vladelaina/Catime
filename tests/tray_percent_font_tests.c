#include "drawing/system_ui_font.h"
#include "tray/tray_animation_percent_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int g_failures = 0;
static COLORREF g_testThemeTextColor = RGB(0, 0, 0);

COLORREF GetSystemMetricTextColor(void) {
    return g_testThemeTextColor;
}

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

static HFONT CreateSampleFont(
    HDC dc, const wchar_t* text, SIZE* size) {
    return CreateFittedMetricIconTextFont(
        dc, text, (int)wcslen(text), 15, 16, 96, size);
}

static void CheckFont(
    HFONT font, const SIZE* size, const LOGFONTW* metricTemplate,
    const char* creationMessage) {
    LOGFONTW actual = {0};
    Expect(font != NULL, creationMessage);
    if (!font) return;
    Expect(GetObjectW(font, sizeof(actual), &actual) == sizeof(actual),
           "failed to inspect a fitted percent font");
    if (actual.lfHeight != metricTemplate->lfHeight) {
        fprintf(stderr,
                "metric height mismatch: expected=%ld actual=%ld "
                "width=%ld measured=%ldx%ld\n",
                metricTemplate->lfHeight, actual.lfHeight,
                actual.lfWidth, size->cx, size->cy);
        ++g_failures;
    }
    Expect(actual.lfWeight == metricTemplate->lfWeight,
           "percent digits did not preserve the taskbar font weight");
    Expect(wcscmp(actual.lfFaceName, metricTemplate->lfFaceName) == 0,
           "percent digits did not preserve the taskbar font face");
    Expect(size->cx <= 15 && size->cy <= 17,
           "fitted percent digits exceeded the tray icon bounds");
}

static void CheckFallbackFont(HFONT sourceFont) {
    LOGFONTW actual = {0};
    HFONT fallback = CreateNonAntialiasedFontCopy(sourceFont);
    Expect(fallback != NULL, "failed to create a color-key fallback font");
    if (fallback) {
        Expect(GetObjectW(fallback, sizeof(actual), &actual) ==
                   sizeof(actual) &&
               actual.lfQuality == NONANTIALIASED_QUALITY,
               "the color-key fallback font retained antialiasing");
        DeleteObject(fallback);
    }
}

static void CheckTransparentTextPixels(HDC dc, HFONT font,
                                       COLORREF textColor) {
    DWORD pixels[16 * 16] = {0};
    BOOL drawn = DrawAlphaTextOnTransparentIcon(
        dc, pixels, 16, 16, font, L"7", 1, 4, 1, textColor);
    Expect(drawn, "failed to render transparent tray text");
    int coveredPixels = 0;
    BOOL valid = TRUE;
    for (size_t i = 0; i < _countof(pixels); ++i) {
        BYTE alpha = (BYTE)(pixels[i] >> 24);
        BYTE red = (BYTE)(pixels[i] >> 16);
        BYTE green = (BYTE)(pixels[i] >> 8);
        BYTE blue = (BYTE)pixels[i];
        if (alpha == 0) {
            if (pixels[i] != 0) valid = FALSE;
            continue;
        }
        ++coveredPixels;
        BYTE expectedRed = (BYTE)(GetRValue(textColor) * alpha / 255u);
        BYTE expectedGreen = (BYTE)(GetGValue(textColor) * alpha / 255u);
        BYTE expectedBlue = (BYTE)(GetBValue(textColor) * alpha / 255u);
        if (red < expectedRed || red > expectedRed + 1 ||
            green < expectedGreen || green > expectedGreen + 1 ||
            blue < expectedBlue || blue > expectedBlue + 1) {
            valid = FALSE;
        }
    }
    Expect(coveredPixels > 0, "transparent tray text mask was empty");
    Expect(valid, "transparent tray text was not premultiplied cleanly");
}

static void CheckThemeCacheInvalidation(void) {
    COLORREF textColor = CLR_INVALID;
    COLORREF bgColor = 0;
    SetPercentIconColors(RGB(12, 34, 56), TRANSPARENT_BG_AUTO);
    g_testThemeTextColor = RGB(0, 0, 0);
    Expect(GetPercentIconColorSnapshot(&textColor, &bgColor),
           "failed to resolve the initial tray theme color");
    Expect(textColor == RGB(0, 0, 0) && bgColor == TRANSPARENT_BG_AUTO,
           "the initial tray theme color was incorrect");

    g_testThemeTextColor = RGB(255, 255, 255);
    Expect(GetPercentIconColorSnapshot(&textColor, &bgColor),
           "failed to read the cached tray theme color");
    Expect(textColor == RGB(0, 0, 0),
           "the test did not exercise the tray theme cache");
    Expect(InvalidatePercentIconThemeCache(),
           "the transparent tray theme cache was not invalidated");
    Expect(GetPercentIconColorSnapshot(&textColor, &bgColor),
           "failed to resolve the refreshed tray theme color");
    Expect(textColor == RGB(255, 255, 255),
           "the tray theme color remained stale after invalidation");

    SetPercentIconColors(RGB(1, 2, 3), RGB(4, 5, 6));
    Expect(!InvalidatePercentIconThemeCache(),
           "a fixed-color tray icon was treated as theme-aware");
    CleanupPercentIconCache();
}

static void CheckGeneratedIconAlpha(BOOL transparent) {
    g_testThemeTextColor = RGB(255, 255, 255);
    SetPercentIconColors(
        RGB(255, 255, 255),
        transparent ? TRANSPARENT_BG_AUTO : RGB(20, 40, 60));
    HICON icon = CreatePercentIcon16(78);
    Expect(icon != NULL, "failed to create a generated percent icon");
    if (!icon) return;

    ICONINFO iconInfo = {0};
    BITMAP bitmap = {0};
    Expect(GetIconInfo(icon, &iconInfo),
           "failed to inspect a generated percent icon");
    BOOL hasBitmap = iconInfo.hbmColor &&
        GetObjectW(iconInfo.hbmColor, sizeof(bitmap), &bitmap) ==
            sizeof(bitmap);
    Expect(hasBitmap, "generated percent icon had no color bitmap");
    if (hasBitmap) {
        size_t count = (size_t)bitmap.bmWidth * (size_t)bitmap.bmHeight;
        DWORD* pixels = calloc(count, sizeof(*pixels));
        BITMAPINFO info = {0};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = bitmap.bmWidth;
        info.bmiHeader.biHeight = -bitmap.bmHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        HDC dc = GetDC(NULL);
        BOOL copied = pixels && dc && GetDIBits(
            dc, iconInfo.hbmColor, 0, (UINT)bitmap.bmHeight,
            pixels, &info, DIB_RGB_COLORS) == bitmap.bmHeight;
        Expect(copied, "failed to read generated percent icon pixels");
        if (copied) {
            int transparentPixels = 0;
            int coveredPixels = 0;
            for (size_t i = 0; i < count; ++i) {
                BYTE alpha = (BYTE)(pixels[i] >> 24);
                if (alpha == 0) ++transparentPixels;
                else ++coveredPixels;
                if (!transparent && alpha != 255) {
                    Expect(FALSE, "solid percent icon contained partial alpha");
                    break;
                }
            }
            Expect(coveredPixels > 0,
                   "generated percent icon contained no visible pixels");
            if (transparent) {
                Expect(transparentPixels > 0,
                       "transparent percent icon had an opaque background");
            }
        }
        if (dc) ReleaseDC(NULL, dc);
        free(pixels);
    }
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    DestroyIcon(icon);
    CleanupPercentIconCache();
}

int main(void) {
    HDC dc = GetDC(NULL);
    Expect(dc != NULL, "failed to acquire a screen DC");
    if (!dc) return 1;

    LOGFONTW metricTemplate = {0};
    InitializeSystemUiMetricTextLogFont(
        &metricTemplate, 96, ANTIALIASED_QUALITY);
    Expect(metricTemplate.lfHeight == -12,
           "the shared 9pt metric font was not 12px at 96 DPI");

    SIZE oneDigitSize = {0};
    SIZE twoDigitSize = {0};
    SIZE threeDigitSize = {0};
    HFONT oneDigit = CreateSampleFont(dc, L"7", &oneDigitSize);
    HFONT twoDigits = CreateSampleFont(dc, L"42", &twoDigitSize);
    HFONT threeDigits = CreateSampleFont(dc, L"100", &threeDigitSize);

    CheckFont(oneDigit, &oneDigitSize, &metricTemplate,
              "failed to create the one-digit percent font");
    CheckFont(twoDigits, &twoDigitSize, &metricTemplate,
              "failed to create the two-digit percent font");
    CheckFont(threeDigits, &threeDigitSize, &metricTemplate,
              "failed to create the three-digit percent font");
    if (oneDigit) {
        CheckFallbackFont(oneDigit);
        CheckTransparentTextPixels(dc, oneDigit, RGB(255, 255, 255));
        CheckTransparentTextPixels(dc, oneDigit, RGB(0, 0, 0));
    }
    CheckThemeCacheInvalidation();
    CheckGeneratedIconAlpha(TRUE);
    CheckGeneratedIconAlpha(FALSE);

    if (oneDigit) DeleteObject(oneDigit);
    if (twoDigits) DeleteObject(twoDigits);
    if (threeDigits) DeleteObject(threeDigits);
    ReleaseDC(NULL, dc);

    if (g_failures) {
        fprintf(stderr, "%d tray percent font test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
