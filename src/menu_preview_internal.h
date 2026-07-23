#ifndef MENU_PREVIEW_INTERNAL_H
#define MENU_PREVIEW_INTERNAL_H

#include "menu_preview.h"
#include "color/color.h"

typedef struct {
    PreviewType type;
    union {
        char colorHex[COLOR_HEX_BUFFER];
        struct { char fontName[MAX_PATH]; char internalName[MAX_PATH]; } font;
        TimeFormatType timeFormat;
        BOOL showMilliseconds;
        BOOL showSeconds;
        BOOL use24Hour;
        char animationPath[MAX_PATH];
        EffectType effect;
    } data;
} PreviewState;

extern PreviewState g_previewState;

#endif
