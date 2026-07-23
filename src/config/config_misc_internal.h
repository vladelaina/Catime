#ifndef CONFIG_MISC_INTERNAL_H
#define CONFIG_MISC_INTERNAL_H

#include "config.h"
#include "config/config_defaults.h"
#include "language.h"
#include "../resource/resource.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "menu_preview.h"
#include "plugin/plugin_data.h"
#include "timer/main_timer.h"
#include "drawing/drawing_timer_precision.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    int value;
    const char* str;
} ConfigMiscEnumStrMap;

const char* ConfigMisc_EnumToString(
    const ConfigMiscEnumStrMap* map, int value, const char* defaultValue);
int ConfigMisc_StringToEnum(
    const ConfigMiscEnumStrMap* map, const char* value, int defaultValue);

#endif
