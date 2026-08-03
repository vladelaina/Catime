#ifndef MENU_PREVIEW_INTERNAL_H
#define MENU_PREVIEW_INTERNAL_H

#include "menu_preview.h"
#include "color/color.h"
#include "taskbar_monitor.h"

typedef struct {
    PreviewType type;
    PreviewSource source;
    union {
        char colorHex[COLOR_HEX_BUFFER];
        struct { char fontName[MAX_PATH]; char internalName[MAX_PATH]; } font;
        TimeFormatType timeFormat;
        BOOL showMilliseconds;
        BOOL showSeconds;
        BOOL use24Hour;
        char animationPath[MAX_PATH];
        EffectType effect;
        struct {
            TaskbarMonitorOption option;
            BOOL originalCpuMemoryEnabled;
            BOOL originalNetworkEnabled;
        } taskbarMonitor;
    } data;
} PreviewState;

extern PreviewState g_previewState;

#endif
