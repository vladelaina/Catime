#include "drawing/system_ui_font.h"
#include "tray/tray_animation_percent_internal.h"

#include <stdio.h>
#include <wchar.h>

static int g_failures = 0;

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
