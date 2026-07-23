/**
 * @file config_animation_colors.c
 * @brief Percent tray icon color configuration
 */
#include "config.h"

#include "color/color_parser.h"
#include "tray/tray_animation_percent.h"

#include <string.h>

void ReadPercentIconColorsConfig(void) {
    char configPath[MAX_PATH] = {0};
    char textValue[32] = {0};
    char backgroundValue[32] = {0};
    COLORREF textColor = RGB(0, 0, 0);
    COLORREF backgroundColor = TRANSPARENT_BG_AUTO;

    GetConfigPath(configPath, sizeof(configPath));
    ReadIniString("Animation", "PERCENT_ICON_TEXT_COLOR", "auto",
                  textValue, sizeof(textValue), configPath);
    ReadIniString("Animation", "PERCENT_ICON_BG_COLOR", "transparent",
                  backgroundValue, sizeof(backgroundValue), configPath);
    if (textValue[0] != '\0' && _stricmp(textValue, "auto") != 0) {
        if (!ColorStringToColorRef(textValue, &textColor)) {
            textColor = RGB(0, 0, 0);
        }
    }
    if (backgroundValue[0] != '\0' &&
        _stricmp(backgroundValue, "transparent") != 0) {
        if (!ColorStringToColorRef(backgroundValue, &backgroundColor)) {
            backgroundColor = TRANSPARENT_BG_AUTO;
        }
    }
    SetPercentIconColors(textColor, backgroundColor);
}
