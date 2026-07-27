#ifndef CATIME_SYSTEM_UI_FONT_H
#define CATIME_SYSTEM_UI_FONT_H

#include <windows.h>

void InitializeSystemUiTextLogFont(
    LOGFONTW* logFont, int pixelHeight, LONG weight);

/** Initialize the shared 9pt metric font used by compact system monitors. */
void InitializeSystemUiMetricTextLogFont(
    LOGFONTW* logFont, UINT dpi, BYTE quality);

#endif /* CATIME_SYSTEM_UI_FONT_H */
