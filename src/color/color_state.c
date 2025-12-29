/**
 * @file color_state.c
 * @brief Color state management and configuration persistence
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "color/gradient.h"
#include "color/color_state.h"
#include "config.h"

PredefinedColor* COLOR_OPTIONS = NULL;
size_t COLOR_OPTIONS_COUNT = 0;
char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER] = "#FFFFFF";

void GetConfigPath(char* path, size_t size);
void CreateDefaultConfig(const char* config_path);
void WriteConfig(const char* config_path);

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

void AddColorOption(const char* hexColor) {
    if (!hexColor || !*hexColor) return;

    // Predefined gradient name
    GradientType gradType = GetGradientTypeByName(hexColor);
    if (gradType != GRADIENT_NONE) {
        const GradientInfo* info = GetGradientInfo(gradType);
        if (!info) return;
        for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
            if (strcasecmp(COLOR_OPTIONS[i].hexColor, info->name) == 0) return;
        }
        PredefinedColor* newArray = realloc(COLOR_OPTIONS, (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
        if (newArray) {
            char* hexCopy = _strdup(info->name);
            if (hexCopy) {
                COLOR_OPTIONS = newArray;
                COLOR_OPTIONS[COLOR_OPTIONS_COUNT++].hexColor = hexCopy;
            }
        }
        return;
    }

    // Custom gradient
    if (IsGradientColorString(hexColor)) {
        for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
            if (strcasecmp(COLOR_OPTIONS[i].hexColor, hexColor) == 0) return;
        }
        PredefinedColor* newArray = realloc(COLOR_OPTIONS, (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
        if (newArray) {
            char* hexCopy = _strdup(hexColor);
            if (hexCopy) {
                COLOR_OPTIONS = newArray;
                COLOR_OPTIONS[COLOR_OPTIONS_COUNT++].hexColor = hexCopy;
            }
        }
        return;
    }

    // Solid color
    char normalized[COLOR_HEX_BUFFER];
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;
    if (strlen(hex) != 6) return;
    for (size_t i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) return;
    }
    unsigned int color;
    if (sscanf(hex, "%x", &color) != 1) return;
    snprintf(normalized, sizeof(normalized), "#%06X", color);

    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(normalized, COLOR_OPTIONS[i].hexColor) == 0) return;
    }
    PredefinedColor* newArray = realloc(COLOR_OPTIONS, (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
    if (newArray) {
        char* hexCopy = _strdup(normalized);
        if (hexCopy) {
            COLOR_OPTIONS = newArray;
            COLOR_OPTIONS[COLOR_OPTIONS_COUNT++].hexColor = hexCopy;
        }
    }
}

void ClearColorOptions(void) {
    if (COLOR_OPTIONS) {
        for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
            free((void*)COLOR_OPTIONS[i].hexColor);
        }
        free(COLOR_OPTIONS);
        COLOR_OPTIONS = NULL;
        COLOR_OPTIONS_COUNT = 0;
    }
}

void LoadColorConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    ClearColorOptions();

    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = _wfopen(wconfig_path, L"r");
    }

    if (file) {
        char line[2048];
        BOOL found_colors = FALSE;

        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "COLOR_OPTIONS=", 14) == 0) {
                ClearColorOptions();

                char* colors = line + 14;
                while (*colors == '=' || *colors == ' ') colors++;

                char* newline = strchr(colors, '\n');
                if (newline) *newline = '\0';

                char* token = strtok(colors, ",");
                while (token) {
                    TrimString(token);

                    if (*token) {
                        if (token[0] != '#') {
                            char colorWithHash[COLOR_HEX_BUFFER];
                            snprintf(colorWithHash, sizeof(colorWithHash), "#%s", token);
                            AddColorOption(colorWithHash);
                        } else {
                            AddColorOption(token);
                        }
                    }
                    token = strtok(NULL, ",");
                }
                found_colors = TRUE;
                break;
            }
        }
        fclose(file);

        if (!found_colors || COLOR_OPTIONS_COUNT == 0) {
            char defaults[] = DEFAULT_COLOR_OPTIONS_INI;
            char* token = strtok(defaults, ",");
            while (token) {
                while (*token == ' ') token++;
                AddColorOption(token);
                token = strtok(NULL, ",");
            }
        }
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

void WriteConfigColor(const char* color_input) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", color_input, config_path);
}
