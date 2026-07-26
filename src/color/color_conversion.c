#include "color/color_parser.h"
#include "utils/string_safe.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define NEAR_BLACK_COLOR "#000001"

void ColorRefToHex(COLORREF color, char* output, size_t size) {
    if (!output || size == 0) return;
    snprintf(output, size, "#%02X%02X%02X",
             GetRValue(color), GetGValue(color), GetBValue(color));
}

void ReplaceBlackColor(const char* color, char* output, size_t output_size) {
    if (!output || output_size == 0) return;
    if (!color) {
        output[0] = '\0';
        return;
    }
    safe_strncpy(output,
                 strcasecmp(color, "#000000") == 0 ? NEAR_BLACK_COLOR : color,
                 output_size);
}

BOOL ColorStringToColorRef(const char* input, COLORREF* outColor) {
    if (!input || !outColor) return FALSE;

    char normalized[COLOR_HEX_BUFFER] = {0};
    normalizeColor(input, normalized, sizeof(normalized));
    if (normalized[0] != '#' || strlen(normalized) != HEX_COLOR_LENGTH) {
        return FALSE;
    }
    for (size_t i = 1; i < HEX_COLOR_LENGTH; i++) {
        if (!isxdigit((unsigned char)normalized[i])) return FALSE;
    }

    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
    if (sscanf(normalized + 1, "%2x%2x%2x", &red, &green, &blue) != 3) {
        return FALSE;
    }
    *outColor = RGB(red, green, blue);
    return TRUE;
}
