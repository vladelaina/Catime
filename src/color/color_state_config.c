#include "color/color_state.h"
#include "color_state_internal.h"
#include "color/gradient.h"
#include "config.h"
#include "notification.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>

void GetConfigPath(char* path, size_t size);
BOOL CreateDefaultConfig(const char* config_path);

BOOL NormalizeColorConfigValue(const char* color_input, char* outValue,
                               size_t outSize) {
    if (!color_input || !outValue || outSize == 0) return FALSE;
    outValue[0] = '\0';
    if (strchr(color_input, '_') != NULL)
        return NormalizeGradientConfigValue(color_input, outValue, outSize);
    if (outSize <= HEX_COLOR_LENGTH) return FALSE;
    char normalized[COLOR_HEX_BUFFER];
    normalizeColor(color_input, normalized, sizeof(normalized));
    if (!isValidColor(normalized)) return FALSE;
    for (size_t i = 1; normalized[i] != '\0'; i++)
        normalized[i] = ToUpperHexDigit(normalized[i]);
    ReplaceBlackColor(normalized, outValue, outSize);
    return outValue[0] != '\0';
}

BOOL WriteConfigColor(const char* color_input) {
    char colorValue[COLOR_HEX_BUFFER];
    if (!NormalizeColorConfigValue(color_input, colorValue, sizeof(colorValue)))
        return FALSE;
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    char currentValue[COLOR_HEX_BUFFER];
    BOOL currentValueComplete = ReadIniStringExact(
        INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "", currentValue,
        sizeof(currentValue), config_path);
    BOOL runtimeMatches = strcmp(CLOCK_TEXT_COLOR, colorValue) == 0;
    BOOL configMatches = currentValueComplete &&
                         strcmp(currentValue, colorValue) == 0;
    if (runtimeMatches && configMatches) {
        if (strchr(colorValue, '_') != NULL) GetGradientTypeByName(colorValue);
        RefreshToastNotificationColors();
        return TRUE;
    }
    if (!configMatches && !WriteIniString(INI_SECTION_DISPLAY,
                                           "CLOCK_TEXT_COLOR", colorValue,
                                           config_path)) return FALSE;
    if (!runtimeMatches) {
        strncpy(CLOCK_TEXT_COLOR, colorValue, sizeof(CLOCK_TEXT_COLOR) - 1);
        CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
    }
    if (strchr(colorValue, '_') != NULL) GetGradientTypeByName(colorValue);
    RefreshToastNotificationColors();
    return TRUE;
}

BOOL WriteConfigColorOptions(const char* color_options) {
    PredefinedColor* newOptions = NULL;
    size_t newCount = 0;
    if (!BuildColorOptionsFromConfigValue(color_options, &newOptions,
                                          &newCount)) return FALSE;
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
        INI_SECTION_COLORS, "COLOR_OPTIONS", "", currentValue,
        sizeof(currentValue), config_path);
    if ((!currentValueComplete || strcmp(currentValue, normalizedOptions) != 0) &&
        !WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", normalizedOptions,
                        config_path)) {
        FreeColorOptionArray(newOptions, newCount);
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
    if (!FileExists(config_path) && !CreateDefaultConfig(config_path))
        LOG_WARNING("Failed to create default config while loading colors");
    char colors[2048];
    if (!ReadIniStringExact(INI_SECTION_COLORS, "COLOR_OPTIONS",
                            DEFAULT_COLOR_OPTIONS_INI, colors, sizeof(colors),
                            config_path)) {
        LOG_WARNING("COLOR_OPTIONS config is too long, using defaults");
        strcpy_s(colors, sizeof(colors), DEFAULT_COLOR_OPTIONS_INI);
    }
    if (!ReplaceColorOptionsFromConfigValue(colors) &&
        !ReplaceColorOptionsFromConfigValue(DEFAULT_COLOR_OPTIONS_INI)) return;
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
