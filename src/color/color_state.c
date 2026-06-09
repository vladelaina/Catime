/**
 * @file color_state.c
 * @brief Color state management and configuration persistence
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "color/gradient.h"
#include "color/color_state.h"
#include "config.h"
#include "log.h"
#include "../resource/resource.h"

#if MAX_COLOR_OPTIONS > (CMD_POMODORO_TIME_BASE - CMD_COLOR_OPTIONS_BASE)
#error "MAX_COLOR_OPTIONS overlaps the next dynamic menu command range"
#endif

PredefinedColor* COLOR_OPTIONS = NULL;
size_t COLOR_OPTIONS_COUNT = 0;
char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER] = "#FFFFFF";

void GetConfigPath(char* path, size_t size);
BOOL CreateDefaultConfig(const char* config_path);
BOOL WriteConfig(const char* config_path);

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

static BOOL IsGradientColorString(const char* color) {
    return color && strchr(color, '_') != NULL;
}

static char ToUpperHexDigit(char ch) {
    return (char)toupper((unsigned char)ch);
}

static BOOL CanGrowColorOptionCount(size_t count) {
    return count < MAX_COLOR_OPTIONS &&
           count < SIZE_MAX / sizeof(PredefinedColor) - 1;
}

static BOOL AppendColorOptionCopyTo(PredefinedColor** options, size_t* count,
                                    const char* color) {
    if (!options || !count || !color || !CanGrowColorOptionCount(*count)) {
        return FALSE;
    }

    char* colorCopy = _strdup(color);
    if (!colorCopy) return FALSE;

    PredefinedColor* newArray = realloc(*options, (*count + 1) * sizeof(PredefinedColor));
    if (!newArray) {
        free(colorCopy);
        return FALSE;
    }

    *options = newArray;
    (*options)[*count].hexColor = colorCopy;
    (*count)++;
    return TRUE;
}

static BOOL PaletteContainsColor(PredefinedColor* options, size_t count,
                                 const char* color) {
    if (!color) return FALSE;

    for (size_t i = 0; i < count; i++) {
        if (options[i].hexColor && strcasecmp(options[i].hexColor, color) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static void FreeColorOptionArray(PredefinedColor* options, size_t count) {
    if (!options) return;

    for (size_t i = 0; i < count; i++) {
        free((void*)options[i].hexColor);
    }
    free(options);
}

static BOOL AppendColorOptionTo(PredefinedColor** options, size_t* count,
                                const char* color) {
    if (!options || !count || !color || !*color) return TRUE;

    // Predefined gradient name
    GradientInfoSnapshot gradientSnapshot;
    GradientType gradType = GetGradientInfoSnapshotByName(color, &gradientSnapshot);
    if (gradType != GRADIENT_NONE) {
        const GradientInfo* info = &gradientSnapshot.info;
        if (!info->name) return TRUE;
        if (PaletteContainsColor(*options, *count, info->name)) return TRUE;
        return AppendColorOptionCopyTo(options, count, info->name);
    }

    // Invalid gradient-like strings should not pollute the menu/config state.
    if (IsGradientColorString(color)) {
        return TRUE;
    }

    // Solid color
    char normalized[COLOR_HEX_BUFFER];
    const char* hex = (color[0] == '#') ? color + 1 : color;
    if (strlen(hex) != 6) return TRUE;
    for (size_t i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) return TRUE;
    }
    normalized[0] = '#';
    for (size_t i = 0; i < 6; i++) {
        normalized[i + 1] = ToUpperHexDigit(hex[i]);
    }
    normalized[7] = '\0';

    if (PaletteContainsColor(*options, *count, normalized)) return TRUE;
    return AppendColorOptionCopyTo(options, count, normalized);
}

static BOOL AppendNormalizedGradientSegment(const char* begin, const char* end,
                                            char* outValue, size_t outSize,
                                            size_t* used, BOOL firstSegment) {
    if (!begin || !end || !outValue || outSize == 0 || !used) {
        return FALSE;
    }

    while (begin < end && isspace((unsigned char)*begin)) {
        begin++;
    }
    while (end > begin && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    if ((size_t)(end - begin) != HEX_COLOR_LENGTH || *begin != '#') {
        return FALSE;
    }

    size_t needed = firstSegment ? HEX_COLOR_LENGTH : (HEX_COLOR_LENGTH + 1);
    if (*used > outSize - 1 || needed > outSize - 1 - *used) {
        return FALSE;
    }

    if (!firstSegment) {
        outValue[(*used)++] = '_';
    }

    outValue[(*used)++] = '#';
    for (int i = 1; i < HEX_COLOR_LENGTH; i++) {
        if (!isxdigit((unsigned char)begin[i])) {
            return FALSE;
        }
        outValue[(*used)++] = ToUpperHexDigit(begin[i]);
    }

    outValue[*used] = '\0';
    return TRUE;
}

static BOOL NormalizeGradientConfigValue(const char* color_input,
                                         char* outValue, size_t outSize) {
    if (!color_input || !outValue || outSize == 0) {
        return FALSE;
    }

    outValue[0] = '\0';

    size_t used = 0;
    int colorCount = 0;
    const char* segmentBegin = color_input;
    for (const char* p = color_input;; p++) {
        if (*p != '_' && *p != '\0') {
            continue;
        }

        if (!AppendNormalizedGradientSegment(segmentBegin, p, outValue, outSize,
                                             &used, colorCount == 0)) {
            return FALSE;
        }
        colorCount++;

        if (*p == '\0') {
            break;
        }
        segmentBegin = p + 1;
    }

    return colorCount >= 2;
}

void AddColorOption(const char* hexColor) {
    (void)AppendColorOptionTo(&COLOR_OPTIONS, &COLOR_OPTIONS_COUNT, hexColor);
}

void ClearColorOptions(void) {
    if (COLOR_OPTIONS) {
        FreeColorOptionArray(COLOR_OPTIONS, COLOR_OPTIONS_COUNT);
        COLOR_OPTIONS = NULL;
        COLOR_OPTIONS_COUNT = 0;
    }
}

static BOOL BuildColorOptionsFromConfigValue(const char* colorOptions,
                                             PredefinedColor** outOptions,
                                             size_t* outCount) {
    if (!colorOptions || !outOptions || !outCount) return FALSE;

    *outOptions = NULL;
    *outCount = 0;

    if (strlen(colorOptions) >= 2048) {
        return FALSE;
    }

    char buffer[2048];
    strncpy(buffer, colorOptions, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    PredefinedColor* options = NULL;
    size_t count = 0;

    char* context = NULL;
    char* token = strtok_s(buffer, ",", &context);
    while (token) {
        TrimString(token);
        if (*token) {
            if (token[0] != '#') {
                char colorWithHash[COLOR_HEX_BUFFER];
                int written = snprintf(colorWithHash, sizeof(colorWithHash), "#%s", token);
                if (written >= 0 && (size_t)written < sizeof(colorWithHash) &&
                    !AppendColorOptionTo(&options, &count, colorWithHash)) {
                    FreeColorOptionArray(options, count);
                    return FALSE;
                }
            } else if (!AppendColorOptionTo(&options, &count, token)) {
                FreeColorOptionArray(options, count);
                return FALSE;
            }
        }
        token = strtok_s(NULL, ",", &context);
    }

    if (count == 0) {
        FreeColorOptionArray(options, count);
        return FALSE;
    }

    *outOptions = options;
    *outCount = count;
    return TRUE;
}

static BOOL AppendColorOptionConfigValue(char* outValue, size_t outSize,
                                         const char* color, BOOL* firstColor) {
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

static BOOL BuildColorOptionsConfigValueFromArray(PredefinedColor* options,
                                                  size_t count,
                                                  char* outValue,
                                                  size_t outSize) {
    if (!outValue || outSize == 0) return FALSE;

    outValue[0] = '\0';
    BOOL firstColor = TRUE;

    for (size_t i = 0; i < count; i++) {
        const char* color = options[i].hexColor;
        if (color && !IsGradientColorString(color) &&
            !AppendColorOptionConfigValue(outValue, outSize, color, &firstColor)) {
            return FALSE;
        }
    }

    for (size_t i = 0; i < count; i++) {
        const char* color = options[i].hexColor;
        if (color && IsGradientColorString(color) &&
            !AppendColorOptionConfigValue(outValue, outSize, color, &firstColor)) {
            return FALSE;
        }
    }

    return !firstColor;
}

BOOL ReplaceColorOptionsFromConfigValue(const char* color_options) {
    PredefinedColor* newOptions = NULL;
    size_t newCount = 0;
    if (!BuildColorOptionsFromConfigValue(color_options, &newOptions, &newCount)) {
        return FALSE;
    }

    ClearColorOptions();
    COLOR_OPTIONS = newOptions;
    COLOR_OPTIONS_COUNT = newCount;
    return TRUE;
}

void LoadColorConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    ClearColorOptions();

    if (!FileExists(config_path)) {
        if (!CreateDefaultConfig(config_path)) {
            LOG_WARNING("Failed to create default config while loading colors");
        }
    }

    char colors[2048];
    if (!ReadIniStringExact(INI_SECTION_COLORS, "COLOR_OPTIONS",
                            DEFAULT_COLOR_OPTIONS_INI,
                            colors, sizeof(colors), config_path)) {
        LOG_WARNING("COLOR_OPTIONS config is too long, using defaults");
        strcpy_s(colors, sizeof(colors), DEFAULT_COLOR_OPTIONS_INI);
    }

    if (!ReplaceColorOptionsFromConfigValue(colors) &&
        !ReplaceColorOptionsFromConfigValue(DEFAULT_COLOR_OPTIONS_INI)) {
        return;
    }
    
    // Ensure predefined gradients exist
    int gradCount = GetGradientCount();
    for (int i = 0; i < gradCount; i++) {
        const GradientInfo* info = GetGradientInfoByIndex(i);
        if (!info) continue;
        BOOL found = FALSE;
        for (size_t j = 0; j < COLOR_OPTIONS_COUNT; j++) {
            if (strcasecmp(COLOR_OPTIONS[j].hexColor, info->name) == 0) {
                found = TRUE;
                break;
            }
        }
        if (!found) AddColorOption(info->name);
    }
}

BOOL NormalizeColorConfigValue(const char* color_input, char* outValue, size_t outSize) {
    if (!color_input || !outValue || outSize == 0) {
        return FALSE;
    }

    outValue[0] = '\0';

    if (strchr(color_input, '_') != NULL) {
        return NormalizeGradientConfigValue(color_input, outValue, outSize);
    }

    if (outSize <= HEX_COLOR_LENGTH) {
        return FALSE;
    }

    char normalized[COLOR_HEX_BUFFER];
    normalizeColor(color_input, normalized, sizeof(normalized));
    if (!isValidColor(normalized)) {
        return FALSE;
    }

    for (size_t i = 1; normalized[i] != '\0'; i++) {
        normalized[i] = ToUpperHexDigit(normalized[i]);
    }

    ReplaceBlackColor(normalized, outValue, outSize);
    return outValue[0] != '\0';
}

BOOL WriteConfigColor(const char* color_input) {
    char colorValue[COLOR_HEX_BUFFER];
    if (!NormalizeColorConfigValue(color_input, colorValue, sizeof(colorValue))) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[COLOR_HEX_BUFFER];
    BOOL currentValueComplete = ReadIniStringExact(
        INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "",
        currentValue, sizeof(currentValue), config_path);

    BOOL runtimeMatches = strcmp(CLOCK_TEXT_COLOR, colorValue) == 0;
    BOOL configMatches = currentValueComplete &&
                         strcmp(currentValue, colorValue) == 0;
    if (runtimeMatches && configMatches) {
        if (strchr(colorValue, '_') != NULL) {
            GetGradientTypeByName(colorValue);
        }
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", colorValue, config_path)) {
        return FALSE;
    }

    if (!runtimeMatches) {
        strncpy(CLOCK_TEXT_COLOR, colorValue, sizeof(CLOCK_TEXT_COLOR) - 1);
        CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
    }

    if (strchr(colorValue, '_') != NULL) {
        GetGradientTypeByName(colorValue);
    }

    return TRUE;
}

BOOL WriteConfigColorOptions(const char* color_options) {
    PredefinedColor* newOptions = NULL;
    size_t newCount = 0;
    if (!BuildColorOptionsFromConfigValue(color_options, &newOptions, &newCount)) {
        return FALSE;
    }

    char normalizedOptions[2048];
    if (!BuildColorOptionsConfigValueFromArray(newOptions, newCount,
                                               normalizedOptions,
                                               sizeof(normalizedOptions))) {
        FreeColorOptionArray(newOptions, newCount);
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[2048];
    BOOL currentValueComplete = ReadIniStringExact(
        INI_SECTION_COLORS, "COLOR_OPTIONS", "",
        currentValue, sizeof(currentValue), config_path);

    if ((!currentValueComplete || strcmp(currentValue, normalizedOptions) != 0) &&
        !WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS",
                        normalizedOptions, config_path)) {
        FreeColorOptionArray(newOptions, newCount);
        return FALSE;
    }

    ClearColorOptions();
    COLOR_OPTIONS = newOptions;
    COLOR_OPTIONS_COUNT = newCount;
    return TRUE;
}
