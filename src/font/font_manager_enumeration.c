#include "font_manager_internal.h"

void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return;
    }

    LOGFONT logFont;
    memset(&logFont, 0, sizeof(logFont));
    logFont.lfCharSet = DEFAULT_CHARSET;
    HFONT font = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        logFont.lfCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
    HFONT oldFont = font ? (HFONT)SelectObject(hdc, font) : NULL;
    EnumFontFamiliesExW(
        hdc, &logFont, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);
    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    if (font) {
        DeleteObject(font);
    }
    ReleaseDC(NULL, hdc);
}

int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEXW* logFont, NEWTEXTMETRICEX* textMetric,
    DWORD fontType, LPARAM parameter) {
    (void)logFont;
    (void)textMetric;
    (void)fontType;
    (void)parameter;
    return 1;
}
