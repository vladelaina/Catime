#include "config_misc_internal.h"

BOOL WriteConfigTimeFormat(TimeFormatType format) {
    const char* formatString = ConfigMisc_EnumToString(
        (const ConfigMiscEnumStrMap[]){
            {TIME_FORMAT_DEFAULT, "DEFAULT"},
            {TIME_FORMAT_ZERO_PADDED, "ZERO_PADDED"},
            {TIME_FORMAT_FULL_PADDED, "FULL_PADDED"},
            {-1, NULL}}, format, "DEFAULT");
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentValue[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT",
                  currentValue, sizeof(currentValue), configPath);
    BOOL runtimeMatches = g_AppConfig.display.time_format.format == format;
    BOOL configMatches = strcmp(currentValue, formatString) == 0;
    if (runtimeMatches && configMatches) return TRUE;
    if (!configMatches && !WriteIniString(
            INI_SECTION_TIMER, "CLOCK_TIME_FORMAT",
            formatString, configPath)) return FALSE;
    g_AppConfig.display.time_format.format = format;
    return TRUE;
}

BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds) {
    showMilliseconds = showMilliseconds ? TRUE : FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    char currentValue[16] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "",
                  currentValue, sizeof(currentValue), configPath);
    const char* value = showMilliseconds ? "TRUE" : "FALSE";
    BOOL runtimeMatches =
        g_AppConfig.display.time_format.show_milliseconds == showMilliseconds;
    BOOL configMatches = strcmp(currentValue, value) == 0;
    if (runtimeMatches && configMatches) return TRUE;
    if (!configMatches && !WriteIniBool(
            INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS",
            showMilliseconds, configPath)) return FALSE;
    g_AppConfig.display.time_format.show_milliseconds = showMilliseconds;
    return TRUE;
}

static BOOL IsActiveTimerColorAnimated(void) {
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    static char lastColor[COLOR_HEX_BUFFER] = {0};
    static BOOL lastAnimated = FALSE;
    if (strcmp(activeColor, lastColor) == 0) return lastAnimated;
    strncpy_s(lastColor, sizeof(lastColor), activeColor, _TRUNCATE);
    lastAnimated = IsGradientNameAnimated(activeColor);
    return lastAnimated;
}

static BOOL IsRunningCountUpTimer(void) {
    return CLOCK_COUNT_UP && !CLOCK_IS_PAUSED;
}

static BOOL IsRunningCountdownTimer(void) {
    return !CLOCK_COUNT_UP && !CLOCK_IS_PAUSED && CLOCK_TOTAL_TIME > 0 &&
           countdown_elapsed_time < CLOCK_TOTAL_TIME &&
           !countdown_message_shown;
}

static BOOL ShouldRunMainTimer(HWND hwnd) {
    (void)hwnd;
    BOOL pluginActive = PluginData_IsActive();
    BOOL colorAnimated = IsActiveTimerColorAnimated();
    if (CLOCK_SHOW_CURRENT_TIME || IsRunningCountUpTimer() ||
        IsRunningCountdownTimer() || IsPreviewActive()) return TRUE;
    if (pluginActive && (PluginData_HasCatimeTag() || colorAnimated)) return TRUE;
    return CLOCK_EDIT_MODE &&
           (GetActiveShowMilliseconds() || colorAnimated);
}

UINT GetTimerInterval(void) {
    if (GetActiveShowMilliseconds()) return 20;
    if (IsActiveTimerColorAnimated()) return 66;
    if (CLOCK_SHOW_CURRENT_TIME && GetActiveShowSeconds()) return 250;
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        return 100;
    }
    return 1000;
}

void ResetTimerWithInterval(HWND hwnd) {
    UINT interval = GetTimerInterval();
    if (ShouldRunMainTimer(hwnd)) {
        if (!MainTimer_Start(hwnd, interval)) {
            LOG_WARNING(
                "Failed to reset main timer with interval %u; pausing active timer state",
                interval);
            if (IsRunningCountUpTimer() || IsRunningCountdownTimer()) {
                CLOCK_IS_PAUSED = TRUE;
            }
        }
    } else {
        MainTimer_Stop();
    }
    ResetTimerMilliseconds();
}
