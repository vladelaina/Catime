/**
 * @file color_parser.c
 * @brief Pure color parsing and conversion algorithms
 * 
 * No dependencies on global state - all functions are pure and testable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "color/color_parser.h"
#include "color/gradient.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEX_SHORT_LENGTH 4
#define HEX_DIGITS_LENGTH 6
#define NEAR_BLACK_COLOR "#000001"
#define HEX_DIGITS "0123456789abcdefABCDEF"

/* ============================================================================
 * CSS Color Table
 * ============================================================================ */

typedef struct {
    const char* name;
    const char* hex;
} CSSColor;

static const CSSColor CSS_COLORS[] = {
    {"white", "#FFFFFF"},   {"black", "#000000"},   {"red", "#FF0000"},
    {"lime", "#00FF00"},    {"blue", "#0000FF"},    {"yellow", "#FFFF00"},
    {"cyan", "#00FFFF"},    {"magenta", "#FF00FF"}, {"silver", "#C0C0C0"},
    {"gray", "#808080"},    {"maroon", "#800000"},  {"olive", "#808000"},
    {"green", "#008000"},   {"purple", "#800080"},  {"teal", "#008080"},
    {"navy", "#000080"},    {"orange", "#FFA500"},  {"pink", "#FFC0CB"},
    {"brown", "#A52A2A"},   {"violet", "#EE82EE"},  {"indigo", "#4B0082"},
    {"gold", "#FFD700"},    {"coral", "#FF7F50"},   {"salmon", "#FA8072"},
    {"khaki", "#F0E68C"},   {"plum", "#DDA0DD"},    {"azure", "#F0FFFF"},
    {"ivory", "#FFFFF0"},   {"wheat", "#F5DEB3"},   {"snow", "#FFFAFA"}
};

#define CSS_COLORS_COUNT (sizeof(CSS_COLORS) / sizeof(CSS_COLORS[0]))

/* ============================================================================
 * String Utilities (Internal use only)
 * ============================================================================ */

static void TrimString(char* str) {
    if (!str) return;

    char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

/* ============================================================================
 * Color Conversion Functions
 * ============================================================================ */

void ColorRefToHex(COLORREF color, char* output, size_t size) {
    snprintf(output, size, "#%02X%02X%02X",
             GetRValue(color), GetGValue(color), GetBValue(color));
}

void ReplaceBlackColor(const char* color, char* output, size_t output_size) {
    if (color && strcasecmp(color, "#000000") == 0) {
        strncpy(output, NEAR_BLACK_COLOR, output_size);
    } else {
        strncpy(output, color, output_size);
    }
    output[output_size - 1] = '\0';
}

/* ============================================================================
 * Color Parsing Functions
 * ============================================================================ */

static BOOL ParseCSSColor(const char* name, char* output, size_t size) {
    for (size_t i = 0; i < CSS_COLORS_COUNT; i++) {
        if (strcmp(name, CSS_COLORS[i].name) == 0) {
            strncpy(output, CSS_COLORS[i].hex, size);
            output[size - 1] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL ParseHexColor(const char* hex, char* output, size_t size) {
    const char* ptr = (hex[0] == '#') ? hex + 1 : hex;
    size_t len = strlen(ptr);
    
    /* #RGB -> #RRGGBB */
    if (len == 3 && strspn(ptr, HEX_DIGITS) == 3) {
        snprintf(output, size, "#%c%c%c%c%c%c",
                ptr[0], ptr[0], ptr[1], ptr[1], ptr[2], ptr[2]);
        return TRUE;
    }
    
    if (len == HEX_DIGITS_LENGTH && strspn(ptr, HEX_DIGITS) == HEX_DIGITS_LENGTH) {
        snprintf(output, size, "#%s", ptr);
        return TRUE;
    }
    
    return FALSE;
}

static BOOL TryParseRGBWithSeparator(const char* str, const char* sep, int* r, int* g, int* b) {
    char format[32];
    snprintf(format, sizeof(format), "%%d%s%%d%s%%d", sep, sep);
    return (sscanf(str, format, r, g, b) == 3);
}

/** Multiple separators for international keyboards */
static BOOL ParseRGBColor(const char* rgb_input, char* output, size_t size) {
    int r = -1, g = -1, b = -1;
    const char* rgb_str = rgb_input;
    
    if (strncmp(rgb_str, "rgb", 3) == 0) {
        rgb_str += 3;
        while (*rgb_str && (*rgb_str == '(' || isspace(*rgb_str))) rgb_str++;
    }
    
    static const char* separators[] = {",", "，", ";", "；", " ", "|"};
    for (size_t i = 0; i < sizeof(separators) / sizeof(char*); i++) {
        if (TryParseRGBWithSeparator(rgb_str, separators[i], &r, &g, &b)) {
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                snprintf(output, size, "#%02X%02X%02X", r, g, b);
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

void normalizeColor(const char* input, char* output, size_t output_size) {
    if (!input || !output) return;
    
    while (isspace(*input)) input++;
    
    char lower[COLOR_BUFFER_SIZE];
    strncpy(lower, input, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (char* p = lower; *p; p++) {
        *p = tolower(*p);
    }
    
    if (ParseCSSColor(lower, output, output_size)) return;
    
    char cleaned[COLOR_BUFFER_SIZE] = {0};
    int j = 0;
    for (int i = 0; lower[i] && j < (int)sizeof(cleaned) - 1; i++) {
        if (!isspace(lower[i]) && lower[i] != ',' && lower[i] != '(' && lower[i] != ')') {
            cleaned[j++] = lower[i];
        }
    }
    
    if (ParseHexColor(cleaned, output, output_size)) return;
    
    if (ParseRGBColor(lower, output, output_size)) return;
    
    strncpy(output, input, output_size);
    output[output_size - 1] = '\0';
}

BOOL isValidColor(const char* input) {
    if (!input || !*input) return FALSE;
    
    char normalized[COLOR_BUFFER_SIZE];
    normalizeColor(input, normalized, sizeof(normalized));
    
    if (normalized[0] != '#' || strlen(normalized) != HEX_COLOR_LENGTH) {
        return FALSE;
    }
    
    for (int i = 1; i < HEX_COLOR_LENGTH; i++) {
        if (!isxdigit((unsigned char)normalized[i])) {
            return FALSE;
        }
    }
    
    return TRUE;
}

BOOL isValidColorOrGradient(const char* input) {
    if (!input || !*input) return FALSE;
    
    /* Check if it contains underscore (gradient format) */
    if (strchr(input, '_') != NULL) {
        /* Gradient format: validate each color segment */
        char temp[COLOR_BUFFER_SIZE];
        strncpy(temp, input, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        
        /* Use thread-safe strtok_s (Windows) */
        char* context = NULL;
        char* token = strtok_s(temp, "_", &context);
        int colorCount = 0;
        
        while (token != NULL) {
            /* Trim whitespace */
            while (*token && isspace((unsigned char)*token)) token++;
            
            /* Check for empty segment */
            if (*token == '\0') {
                return FALSE;
            }
            
            /* Each segment should be valid hex color */
            if (*token != '#') {
                /* Must start with # for gradient colors */
                return FALSE;
            }
            
            const char* hex = token + 1;
            size_t len = strlen(hex);
            
            /* Trim trailing whitespace from hex */
            while (len > 0 && isspace((unsigned char)hex[len - 1])) {
                len--;
            }
            
            /* Accept #RRGGBB (6 digits) */
            if (len != 6) {
                return FALSE;
            }
            
            for (size_t i = 0; i < len; i++) {
                if (!isxdigit((unsigned char)hex[i])) {
                    return FALSE;
                }
            }
            
            colorCount++;
            token = strtok_s(NULL, "_", &context);
        }
        
        /* Gradient must have at least 2 colors */
        return (colorCount >= 2);
    }
    
    /* Single color: use existing validation */
    return isValidColor(input);
}

