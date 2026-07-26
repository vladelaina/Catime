#ifndef CATIME_SYSTEM_UI_FONT_H
#define CATIME_SYSTEM_UI_FONT_H

#include <windows.h>

void InitializeSystemUiTextLogFont(
    LOGFONTW* logFont, int pixelHeight, LONG weight);

#endif /* CATIME_SYSTEM_UI_FONT_H */
