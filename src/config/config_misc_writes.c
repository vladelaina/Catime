#include "config_misc_internal.h"

static void UpdateStartupModeBuffer(const char* mode) {
    strncpy(CLOCK_STARTUP_MODE, mode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
}

BOOL WriteConfigStartupMode(const char* mode) {
    if (!mode || !*mode) return FALSE;
    char normalizedMode[sizeof(CLOCK_STARTUP_MODE)];
    strncpy(normalizedMode, mode, sizeof(normalizedMode) - 1);
    normalizedMode[sizeof(normalizedMode) - 1] = '\0';
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentValue[sizeof(CLOCK_STARTUP_MODE)] = {0};
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "SHOW_TIME",
                  currentValue, sizeof(currentValue), configPath);
    BOOL runtimeMatches = strcmp(CLOCK_STARTUP_MODE, normalizedMode) == 0;
    BOOL configMatches = strcmp(currentValue, normalizedMode) == 0;
    if (runtimeMatches && configMatches) return TRUE;
    if (!configMatches && !WriteIniString(
            INI_SECTION_TIMER, "STARTUP_MODE", normalizedMode, configPath)) {
        return FALSE;
    }
    UpdateStartupModeBuffer(normalizedMode);
    return TRUE;
}

BOOL WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    const char* section = INI_SECTION_OPTIONS;
    if (strcmp(key, "CONFIG_VERSION") == 0 ||
        strcmp(key, "LANGUAGE") == 0 ||
        strcmp(key, "SHORTCUT_CHECK_DONE") == 0 ||
        strcmp(key, "FIRST_RUN") == 0 ||
        strcmp(key, "AUTO_START_PREFERENCE") == 0 ||
        strcmp(key, "FONT_LICENSE_ACCEPTED") == 0 ||
        strcmp(key, "FONT_LICENSE_VERSION_ACCEPTED") == 0) {
        section = INI_SECTION_GENERAL;
    } else if (strncmp(key, "CLOCK_TEXT_COLOR", 16) == 0 ||
               strncmp(key, "FONT_FILE_NAME", 14) == 0 ||
               strncmp(key, "CLOCK_BASE_FONT_SIZE", 20) == 0 ||
               strncmp(key, "WINDOW_SCALE", 12) == 0 ||
               strncmp(key, "PLUGIN_SCALE", 12) == 0 ||
               strncmp(key, "CLOCK_WINDOW_POS_X", 18) == 0 ||
               strncmp(key, "CLOCK_WINDOW_POS_Y", 18) == 0 ||
               strcmp(key, WINDOW_POSITION_MANUAL_KEY) == 0 ||
               strcmp(key, WINDOW_MONITOR_ID_KEY) == 0 ||
               strcmp(key, WINDOW_MONITOR_OFFSET_X_KEY) == 0 ||
               strcmp(key, WINDOW_MONITOR_OFFSET_Y_KEY) == 0 ||
               strcmp(key, WINDOW_TASKBAR_ANCHORED_KEY) == 0 ||
               strcmp(key, WINDOW_TASKBAR_AXIS_RATIO_KEY) == 0 ||
               strcmp(key, WINDOW_TASKBAR_CROSS_OFFSET_KEY) == 0 ||
               strncmp(key, "WINDOW_TOPMOST", 14) == 0 ||
               strncmp(key, "WINDOW_OPACITY", 14) == 0 ||
               strncmp(key, "TEXT_", 5) == 0) {
        section = INI_SECTION_DISPLAY;
    } else if (strncmp(key, "CLOCK_DEFAULT_START_TIME", 24) == 0 ||
               strncmp(key, "CLOCK_USE_24HOUR", 16) == 0 ||
               strncmp(key, "CLOCK_SHOW_SECONDS", 18) == 0 ||
               strncmp(key, "CLOCK_TIME_FORMAT", 17) == 0 ||
               strncmp(key, "CLOCK_SHOW_MILLISECONDS", 23) == 0 ||
               strncmp(key, "CLOCK_TIME_OPTIONS", 18) == 0 ||
               strncmp(key, "STARTUP_MODE", 12) == 0 ||
               strncmp(key, "CLOCK_TIMEOUT_TEXT", 18) == 0 ||
               strncmp(key, "CLOCK_TIMEOUT_ACTION", 20) == 0 ||
               strncmp(key, "CLOCK_TIMEOUT_FILE", 18) == 0 ||
               strncmp(key, "CLOCK_TIMEOUT_WEBSITE", 21) == 0) {
        section = INI_SECTION_TIMER;
    } else if (strncmp(key, "POMODORO_", 9) == 0) {
        section = INI_SECTION_POMODORO;
    } else if (strncmp(key, "NOTIFICATION_", 13) == 0 ||
               strncmp(key, "CLOCK_TIMEOUT_MESSAGE_TEXT", 26) == 0) {
        section = INI_SECTION_NOTIFICATION;
    } else if (strncmp(key, "HOTKEY_", 7) == 0) {
        section = INI_SECTION_HOTKEYS;
    } else if (strncmp(key, "CLOCK_RECENT_FILE", 17) == 0) {
        section = INI_SECTION_RECENTFILES;
    } else if (strncmp(key, "COLOR_OPTIONS", 13) == 0) {
        section = INI_SECTION_COLORS;
    }
    return WriteIniString(section, key, value, configPath);
}

int ReadConfigOpacityStepNormal(void) {
    return g_AppConfig.display.opacity_step_normal;
}

int ReadConfigOpacityStepFast(void) {
    return g_AppConfig.display.opacity_step_fast;
}

void WriteConfigOpacitySteps(int normalStep, int fastStep) {
    if (normalStep < 1) normalStep = 1;
    if (normalStep > 100) normalStep = 100;
    if (fastStep < 1) fastStep = 1;
    if (fastStep > 100) fastStep = 100;
    char normalString[32];
    char fastString[32];
    if (snprintf(normalString, sizeof(normalString), "%d", normalStep) < 0 ||
        snprintf(fastString, sizeof(fastString), "%d", fastStep) < 0) return;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentNormal[32] = {0};
    char currentFast[32] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", "",
                  currentNormal, sizeof(currentNormal), configPath);
    ReadIniString(INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", "",
                  currentFast, sizeof(currentFast), configPath);
    BOOL runtimeMatches =
        g_AppConfig.display.opacity_step_normal == normalStep &&
        g_AppConfig.display.opacity_step_fast == fastStep;
    BOOL configMatches = strcmp(currentNormal, normalString) == 0 &&
                         strcmp(currentFast, fastString) == 0;
    if (runtimeMatches && configMatches) return;
    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", normalString},
        {INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", fastString},
    };
    if (!configMatches && !WriteIniMultipleAtomic(
            configPath, updates, sizeof(updates) / sizeof(updates[0]))) return;
    g_AppConfig.display.opacity_step_normal = normalStep;
    g_AppConfig.display.opacity_step_fast = fastStep;
}
