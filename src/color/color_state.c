/**
 * @file color_state.c
 * @brief Color state management and configuration persistence
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "color/color_state.h"
#include "config.h"

/* ============================================================================
 * Default Color Palette
 * ============================================================================ */

static const char* DEFAULT_COLOR_OPTIONS[] = {
    "#FFFFFF", "#F9DB91", "#F4CAE0", "#FFB6C1",
    "#A8E7DF", "#A3CFB3", "#92CBFC", "#BDA5E7",
    "#9370DB", "#8C92CF", "#72A9A5", "#EB99A7",
    "#EB96BD", "#FFAE8B", "#FF7F50", "#CA6174",
    "CANDY"
};

#define DEFAULT_COLOR_OPTIONS_COUNT (sizeof(DEFAULT_COLOR_OPTIONS) / sizeof(DEFAULT_COLOR_OPTIONS[0]))

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

PredefinedColor* COLOR_OPTIONS = NULL;
size_t COLOR_OPTIONS_COUNT = 0;

char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER] = "#FFFFFF";

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

void GetConfigPath(char* path, size_t size);
void CreateDefaultConfig(const char* config_path);
void WriteConfig(const char* config_path);

/* ============================================================================
 * Internal Helper Functions
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
 * Color Palette Management
 * ============================================================================ */

void AddColorOption(const char* hexColor) {
    if (!hexColor || !*hexColor) return;

    /* Special case for Candy mode */
    if (strcasecmp(hexColor, "CANDY") == 0) {
        /* Deduplication for special keyword */
        for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
            if (strcasecmp(COLOR_OPTIONS[i].hexColor, "CANDY") == 0) {
                return;
            }
        }
        PredefinedColor* newArray = realloc(COLOR_OPTIONS,
                                          (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
        if (newArray) {
            COLOR_OPTIONS = newArray;
            COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = _strdup("CANDY");
            COLOR_OPTIONS_COUNT++;
        }
        return;
    }

    char normalizedColor[COLOR_HEX_BUFFER];
    const char* hex = (hexColor[0] == '#') ? hexColor + 1 : hexColor;

    size_t len = strlen(hex);
    if (len != 6) return;
    
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) return;
    }

    unsigned int color;
    if (sscanf(hex, "%x", &color) != 1) return;

    snprintf(normalizedColor, sizeof(normalizedColor), "#%06X", color);

    /* Deduplication */
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(normalizedColor, COLOR_OPTIONS[i].hexColor) == 0) {
            return;
        }
    }

    PredefinedColor* newArray = realloc(COLOR_OPTIONS,
                                      (COLOR_OPTIONS_COUNT + 1) * sizeof(PredefinedColor));
    if (newArray) {
        COLOR_OPTIONS = newArray;
        COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = _strdup(normalizedColor);
        COLOR_OPTIONS_COUNT++;
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
        char line[1024];
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
            for (size_t i = 0; i < DEFAULT_COLOR_OPTIONS_COUNT; i++) {
                AddColorOption(DEFAULT_COLOR_OPTIONS[i]);
            }
        }
    }
    
    /* Always ensure CANDY option exists, even if config file didn't have it */
    BOOL hasCandy = FALSE;
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (strcasecmp(COLOR_OPTIONS[i].hexColor, "CANDY") == 0) {
            hasCandy = TRUE;
            break;
        }
    }
    if (!hasCandy) {
        AddColorOption("CANDY");
    }
}

/* ============================================================================
 * Configuration Persistence
 * ============================================================================ */

void WriteConfigColor(const char* color_input) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", color_input, config_path);
}

