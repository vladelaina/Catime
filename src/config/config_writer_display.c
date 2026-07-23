/**
 * @file config_writer_display.c
 * @brief Focused persistence for interactive display adjustments
 */
#include "config.h"
#include "config/config_defaults.h"

#include "window.h"

#include <stdio.h>
#include <string.h>

static BOOL IniValueMatches(const char* configPath, const char* section,
                            const char* key, const char* expected) {
    char current[64] = {0};

    if (!configPath || !section || !key) return FALSE;
    ReadIniString(section, key, "", current, sizeof(current), configPath);
    return strcmp(current, expected ? expected : "") == 0;
}

static int ClampInt(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void WriteConfigWindowOpacity(int opacity) {
    char configPath[MAX_PATH] = {0};
    char value[32];
    BOOL configMatches;

    opacity = ClampInt(opacity, MIN_VISIBLE_OPACITY, MAX_OPACITY);
    if (snprintf(value, sizeof(value), "%d", opacity) < 0) return;
    GetConfigPath(configPath, sizeof(configPath));
    configMatches = IniValueMatches(configPath, INI_SECTION_DISPLAY,
                                    "WINDOW_OPACITY", value);
    if (CLOCK_WINDOW_OPACITY == opacity && configMatches) return;
    if (!configMatches &&
        !WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY", opacity,
                     configPath)) {
        return;
    }
    CLOCK_WINDOW_OPACITY = opacity;
}

void WriteConfigMoveSteps(int smallStep, int largeStep) {
    char configPath[MAX_PATH] = {0};
    char smallValue[32];
    char largeValue[32];
    BOOL configMatches;

    smallStep = ClampInt(smallStep, 1, 500);
    largeStep = ClampInt(largeStep, 1, 500);
    if (snprintf(smallValue, sizeof(smallValue), "%d", smallStep) < 0 ||
        snprintf(largeValue, sizeof(largeValue), "%d", largeStep) < 0) {
        return;
    }
    GetConfigPath(configPath, sizeof(configPath));
    configMatches =
        IniValueMatches(configPath, INI_SECTION_DISPLAY,
                        "MOVE_STEP_SMALL", smallValue) &&
        IniValueMatches(configPath, INI_SECTION_DISPLAY,
                        "MOVE_STEP_LARGE", largeValue);
    if (g_AppConfig.display.move_step_small == smallStep &&
        g_AppConfig.display.move_step_large == largeStep && configMatches) {
        return;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "MOVE_STEP_SMALL", smallValue},
        {INI_SECTION_DISPLAY, "MOVE_STEP_LARGE", largeValue},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(configPath, updates, _countof(updates))) {
        return;
    }
    g_AppConfig.display.move_step_small = smallStep;
    g_AppConfig.display.move_step_large = largeStep;
}

void WriteConfigScaleSteps(int normalStep, int fastStep) {
    char configPath[MAX_PATH] = {0};
    char normalValue[32];
    char fastValue[32];
    BOOL configMatches;

    normalStep = ClampInt(normalStep, 1, 100);
    fastStep = ClampInt(fastStep, 1, 100);
    if (snprintf(normalValue, sizeof(normalValue), "%d", normalStep) < 0 ||
        snprintf(fastValue, sizeof(fastValue), "%d", fastStep) < 0) {
        return;
    }
    GetConfigPath(configPath, sizeof(configPath));
    configMatches =
        IniValueMatches(configPath, INI_SECTION_DISPLAY,
                        "SCALE_STEP_NORMAL", normalValue) &&
        IniValueMatches(configPath, INI_SECTION_DISPLAY,
                        "SCALE_STEP_FAST", fastValue);
    if (g_AppConfig.display.scale_step_normal == normalStep &&
        g_AppConfig.display.scale_step_fast == fastStep && configMatches) {
        return;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "SCALE_STEP_NORMAL", normalValue},
        {INI_SECTION_DISPLAY, "SCALE_STEP_FAST", fastValue},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(configPath, updates, _countof(updates))) {
        return;
    }
    g_AppConfig.display.scale_step_normal = normalStep;
    g_AppConfig.display.scale_step_fast = fastStep;
}
