/**
 * @file color_dialog.c
 * @brief Modern color picker integration and palette persistence
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "color/color_dialog.h"
#include "color/color_picker_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "config.h"
#include "log.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CUSTOM_COLORS 16
#define COLOR_OPTIONS_CONFIG_BUFFER 2048

/* Track the actual number of colors loaded */
static size_t g_loadedColorCount = 0;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline void RefreshWindow(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

static BOOL AppendColorOption(char* outValue, size_t outSize, const char* color, BOOL* firstColor) {
    if (!outValue || outSize == 0 || !color || !firstColor) return FALSE;

    size_t used = strlen(outValue);
    size_t colorLen = strlen(color);
    size_t sepLen = *firstColor ? 0 : 1;
    if (used + sepLen + colorLen >= outSize) {
        return FALSE;
    }

    if (!*firstColor) {
        outValue[used++] = ',';
        outValue[used] = '\0';
    }

    memcpy(outValue + used, color, colorLen + 1);
    *firstColor = FALSE;
    return TRUE;
}

static BOOL ColorOptionEquals(const char* entry, size_t entryLen, const char* color) {
    if (!entry || !color || entryLen != strlen(color)) return FALSE;

    for (size_t i = 0; i < entryLen; i++) {
        if (toupper((unsigned char)entry[i]) != toupper((unsigned char)color[i])) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL ColorOptionsConfigContains(const char* value, const char* color) {
    if (!value || !color) return FALSE;

    const char* entry = value;
    while (*entry) {
        const char* end = strchr(entry, ',');
        size_t entryLen = end ? (size_t)(end - entry) : strlen(entry);
        if (ColorOptionEquals(entry, entryLen, color)) {
            return TRUE;
        }
        if (!end) break;
        entry = end + 1;
    }

    return FALSE;
}

static BOOL AppendUniqueColorOption(char* outValue, size_t outSize,
                                    const char* color, BOOL* firstColor) {
    if (ColorOptionsConfigContains(outValue, color)) {
        return TRUE;
    }

    return AppendColorOption(outValue, outSize, color, firstColor);
}

static BOOL BuildCustomColorsConfigValue(COLORREF* lpCustColors,
                                         char* outValue, size_t outSize) {
    if (!lpCustColors || !outValue || outSize == 0) return FALSE;

    outValue[0] = '\0';
    BOOL firstColor = TRUE;

    for (size_t i = 0; i < g_loadedColorCount && i < MAX_CUSTOM_COLORS; i++) {
        char hexColor[COLOR_HEX_BUFFER];
        ColorRefToHex(lpCustColors[i], hexColor, sizeof(hexColor));
        if (!AppendUniqueColorOption(outValue, outSize, hexColor, &firstColor)) {
            return FALSE;
        }
    }

    for (size_t i = g_loadedColorCount; i < MAX_CUSTOM_COLORS; i++) {
        if (lpCustColors[i] != RGB(255, 255, 255)) {
            char hexColor[COLOR_HEX_BUFFER];
            ColorRefToHex(lpCustColors[i], hexColor, sizeof(hexColor));
            if (!AppendUniqueColorOption(outValue, outSize, hexColor, &firstColor)) {
                return FALSE;
            }
        }
    }

    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* color = COLOR_OPTIONS[i].hexColor;
        if (color && strchr(color, '_') &&
            !AppendUniqueColorOption(outValue, outSize, color, &firstColor)) {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Populate custom colors from saved palette
 * @param acrCustClr Custom color array
 * @param maxColors Array size
 */
static void PopulateCustomColors(COLORREF* acrCustClr, size_t maxColors) {
    /* Fill all slots with white (empty appearance) */
    for (size_t i = 0; i < maxColors; i++) {
        acrCustClr[i] = RGB(255, 255, 255);
    }
    
    size_t custIdx = 0;
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT && custIdx < maxColors; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        /* Skip gradient colors (contain underscore) */
        if (strchr(hexColor, '_') != NULL) continue;
        
        COLORREF parsedColor;
        if (ColorStringToColorRef(hexColor, &parsedColor)) {
            acrCustClr[custIdx++] = parsedColor;
        }
    }
    g_loadedColorCount = custIdx;
}

/**
 * @brief Save custom colors to palette
 * @param lpCustColors Custom colors array
 * 
 * @details Saves modified colors and new non-white colors to configuration.
 *          Preserves gradient colors from the original palette.
 */
static BOOL SaveCustomColorsToPalette(COLORREF* lpCustColors) {
    if (!lpCustColors) return FALSE;

    char newOptions[COLOR_OPTIONS_CONFIG_BUFFER];
    if (!BuildCustomColorsConfigValue(lpCustColors, newOptions, sizeof(newOptions))) {
        return FALSE;
    }

    return WriteConfigColorOptions(newOptions);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

COLORREF ShowColorDialog(HWND hwnd) {
    static COLORREF acrCustClr[MAX_CUSTOM_COLORS] = {0};

    COLORREF initialColor = RGB(255, 255, 255);
    ColorStringToColorRef(CLOCK_TEXT_COLOR, &initialColor);

    PopulateCustomColors(acrCustClr, MAX_CUSTOM_COLORS);
    size_t customColorCount = g_loadedColorCount;
    COLORREF selectedColor = initialColor;

    if (ModernColorPicker_Show(hwnd, initialColor, acrCustClr,
                               MAX_CUSTOM_COLORS, &customColorCount,
                               &selectedColor)) {
        if (!SaveCustomColorsToPalette(acrCustClr)) {
            LOG_WARNING("ColorDialog: failed to persist custom color palette");
        }
        
        if (IsPreviewActive()) {
            if (ApplyPreview(hwnd)) {
                return selectedColor;
            }
            CancelPreview(hwnd);
            return (COLORREF)-1;
        }

        char tempColor[COLOR_HEX_BUFFER];
        ColorRefToHex(selectedColor, tempColor, sizeof(tempColor));
        
        char finalColorStr[COLOR_HEX_BUFFER];
        ReplaceBlackColor(tempColor, finalColorStr, sizeof(finalColorStr));

        if (!WriteConfigColor(finalColorStr)) {
            CancelPreview(hwnd);
            return (COLORREF)-1;
        }

        RefreshWindow(hwnd);
        return selectedColor;
    }
    
    /* User cancelled */
    CancelPreview(hwnd);
    return (COLORREF)-1;
}
