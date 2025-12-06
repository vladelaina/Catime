/**
 * @file config_loader.c
 * @brief Configuration loading implementation
 */

#include "config/config_loader.h"
#include "config/config_recovery.h"
#include "config/config_defaults.h"
#include "config.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path || !*utf8Path) return FALSE;
    
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

/* Enum-string mappings (reused from config_core.c) */
typedef struct {
    int value;
    const char* str;
} EnumStrMap;

static const EnumStrMap NOTIFICATION_TYPE_MAP[] = {
    {NOTIFICATION_TYPE_CATIME,       "CATIME"},
    {NOTIFICATION_TYPE_SYSTEM_MODAL, "SYSTEM_MODAL"},
    {NOTIFICATION_TYPE_OS,           "OS"},
    {-1, NULL}
};

static const EnumStrMap TIME_FORMAT_MAP[] = {
    {TIME_FORMAT_DEFAULT,      "DEFAULT"},
    {TIME_FORMAT_ZERO_PADDED,  "ZERO_PADDED"},
    {TIME_FORMAT_FULL_PADDED,  "FULL_PADDED"},
    {-1, NULL}
};

static const EnumStrMap TIMEOUT_ACTION_MAP[] = {
    {TIMEOUT_ACTION_MESSAGE,       "MESSAGE"},
    {TIMEOUT_ACTION_LOCK,          "LOCK"},
    {TIMEOUT_ACTION_SHUTDOWN,      "SHUTDOWN"},
    {TIMEOUT_ACTION_RESTART,       "RESTART"},
    {TIMEOUT_ACTION_OPEN_FILE,     "OPEN_FILE"},
    {TIMEOUT_ACTION_SHOW_TIME,     "SHOW_TIME"},
    {TIMEOUT_ACTION_COUNT_UP,      "COUNT_UP"},
    {TIMEOUT_ACTION_OPEN_WEBSITE,  "OPEN_WEBSITE"},
    {TIMEOUT_ACTION_SLEEP,         "SLEEP"},
    {-1, NULL}
};

static int StringToEnum(const EnumStrMap* map, const char* str, int defaultVal) {
    if (!map || !str) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (strcmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}

/* ============================================================================
 * Font path processing
 * ============================================================================ */

static void ProcessFontPath(ConfigSnapshot* snapshot, const char* config_path) {
    char actualFontFileName[MAX_PATH];
    BOOL isFontsFolderFont = FALSE;
    
    const char* localappdata_prefix = FONTS_PATH_PREFIX;
    if (_strnicmp(snapshot->fontFileName, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName, 
                snapshot->fontFileName + strlen(localappdata_prefix), 
                sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
        isFontsFolderFont = TRUE;
    } else {
        strncpy(actualFontFileName, snapshot->fontFileName, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    /* Set font internal name */
    if (isFontsFolderFont) {
        char fontPath[MAX_PATH];
        BOOL fontFound = FALSE;
        
        wchar_t wConfigPath[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);
        
        wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
        if (lastSep) {
            *lastSep = L'\0';
            
            wchar_t wActualFontFileName[MAX_PATH] = {0};
            MultiByteToWideChar(CP_UTF8, 0, actualFontFileName, -1, wActualFontFileName, MAX_PATH);
            
            wchar_t wFontPath[MAX_PATH] = {0};
            _snwprintf_s(wFontPath, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts\\%s", 
                        wConfigPath, wActualFontFileName);
            
            if (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
                fontFound = TRUE;
                WideCharToMultiByte(CP_UTF8, 0, wFontPath, -1, fontPath, MAX_PATH, NULL, NULL);
            }
            
            if (fontFound) {
                extern BOOL GetFontNameFromFile(const char* fontPath, char* fontName, size_t fontNameSize);
                if (!GetFontNameFromFile(fontPath, snapshot->fontInternalName, 
                                        sizeof(snapshot->fontInternalName))) {
                    char* lastSlash = strrchr(actualFontFileName, '\\');
                    const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                    strncpy(snapshot->fontInternalName, filenameOnly, 
                           sizeof(snapshot->fontInternalName) - 1);
                    snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
                    char* dot = strrchr(snapshot->fontInternalName, '.');
                    if (dot) *dot = '\0';
                }
            } else {
                char* lastSlash = strrchr(actualFontFileName, '\\');
                const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                strncpy(snapshot->fontInternalName, filenameOnly, 
                       sizeof(snapshot->fontInternalName) - 1);
                snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
                char* dot = strrchr(snapshot->fontInternalName, '.');
                if (dot) *dot = '\0';
            }
        } else {
            char* lastSlash = strrchr(actualFontFileName, '\\');
            const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
            strncpy(snapshot->fontInternalName, filenameOnly, 
                   sizeof(snapshot->fontInternalName) - 1);
            snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
            char* dot = strrchr(snapshot->fontInternalName, '.');
            if (dot) *dot = '\0';
        }
    } else {
        strncpy(snapshot->fontInternalName, actualFontFileName, 
               sizeof(snapshot->fontInternalName) - 1);
        snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
        char* dot = strrchr(snapshot->fontInternalName, '.');
        if (dot) *dot = '\0';
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void InitializeDefaultSnapshot(ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    
    memset(snapshot, 0, sizeof(ConfigSnapshot));
    
    /* Use defaults from config_defaults */
    int metaCount = 0;
    const ConfigItemMeta* metadata = GetConfigMetadata(&metaCount);
    
    for (int i = 0; i < metaCount; i++) {
        /* This is a simplified initialization - full implementation would map each field */
    }
    
    /* Set sensible defaults directly */
    snapshot->baseFontSize = DEFAULT_FONT_SIZE;
    snapshot->windowPosX = DEFAULT_WINDOW_POS_X;
    snapshot->windowPosY = DEFAULT_WINDOW_POS_Y;
    snapshot->windowScale = 1.62f;
    snapshot->pluginScale = 1.0f;
    snapshot->windowTopmost = TRUE;
    snapshot->moveStepSmall = DEFAULT_MOVE_STEP_SMALL;
    snapshot->moveStepLarge = DEFAULT_MOVE_STEP_LARGE;
    snapshot->opacityStepNormal = MIN_OPACITY;
    snapshot->opacityStepFast = 5;
    snapshot->scaleStepNormal = DEFAULT_SCALE_STEP_NORMAL;
    snapshot->scaleStepFast = DEFAULT_SCALE_STEP_FAST;
    snapshot->glowEffect = FALSE;
    snapshot->glassEffect = FALSE;
    snapshot->defaultStartTime = DEFAULT_START_TIME_SECONDS;
    snapshot->notificationTimeoutMs = DEFAULT_NOTIFICATION_TIMEOUT_MS;
    snapshot->notificationMaxOpacity = DEFAULT_NOTIFICATION_MAX_OPACITY;
    snapshot->notificationSoundVolume = DEFAULT_NOTIFICATION_VOLUME;
}

BOOL LoadConfigFromFile(const char* config_path, ConfigSnapshot* snapshot) {
    if (!config_path || !snapshot) return FALSE;
    
    /* Initialize with defaults */
    InitializeDefaultSnapshot(snapshot);
    
    /* Read General section */
    ReadIniString(INI_SECTION_GENERAL, "LANGUAGE", "English", 
                 snapshot->language, sizeof(snapshot->language), config_path);
    
    snapshot->fontLicenseAccepted = ReadIniBool(INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", 
                                                FALSE, config_path);
    
    ReadIniString(INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", 
                 snapshot->fontLicenseVersion, sizeof(snapshot->fontLicenseVersion), config_path);
    
    /* Read Display section */
    ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", DEFAULT_TEXT_COLOR,
                 snapshot->textColor, sizeof(snapshot->textColor), config_path);
    
    snapshot->baseFontSize = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 
                                       DEFAULT_FONT_SIZE, config_path);
    
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", 
                 FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                 snapshot->fontFileName, sizeof(snapshot->fontFileName), config_path);
    
    LOG_INFO("ConfigLoader: Read FONT_FILE_NAME from INI: '%s'", snapshot->fontFileName);
    
    ProcessFontPath(snapshot, config_path);
    
    LOG_INFO("ConfigLoader: After ProcessFontPath, fontFileName = '%s'", snapshot->fontFileName);
    
    snapshot->windowPosX = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 
                                     DEFAULT_WINDOW_POS_X, config_path);
    snapshot->windowPosY = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", 
                                     DEFAULT_WINDOW_POS_Y, config_path);
    
    char scaleStr[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE,
                 scaleStr, sizeof(scaleStr), config_path);
    snapshot->windowScale = (float)atof(scaleStr);
    
    char pluginScaleStr[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "PLUGIN_SCALE", DEFAULT_PLUGIN_SCALE,
                 pluginScaleStr, sizeof(pluginScaleStr), config_path);
    snapshot->pluginScale = (float)atof(pluginScaleStr);
    
    snapshot->windowTopmost = ReadIniBool(INI_SECTION_DISPLAY, "WINDOW_TOPMOST",
                                         TRUE, config_path);

    snapshot->windowOpacity = ReadIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY",
                                        100, config_path);

    snapshot->moveStepSmall = ReadIniInt(INI_SECTION_DISPLAY, "MOVE_STEP_SMALL",
                                        DEFAULT_MOVE_STEP_SMALL, config_path);
    snapshot->moveStepLarge = ReadIniInt(INI_SECTION_DISPLAY, "MOVE_STEP_LARGE",
                                        DEFAULT_MOVE_STEP_LARGE, config_path);

    snapshot->opacityStepNormal = ReadIniInt(INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL",
                                            1, config_path);
    snapshot->opacityStepFast = ReadIniInt(INI_SECTION_DISPLAY, "OPACITY_STEP_FAST",
                                          5, config_path);
    snapshot->scaleStepNormal = ReadIniInt(INI_SECTION_DISPLAY, "SCALE_STEP_NORMAL",
                                          DEFAULT_SCALE_STEP_NORMAL, config_path);
    snapshot->scaleStepFast = ReadIniInt(INI_SECTION_DISPLAY, "SCALE_STEP_FAST",
                                        DEFAULT_SCALE_STEP_FAST, config_path);
    snapshot->glowEffect = ReadIniBool(INI_SECTION_DISPLAY, "TEXT_GLOW_EFFECT",
                                       FALSE, config_path);
    snapshot->glassEffect = ReadIniBool(INI_SECTION_DISPLAY, "TEXT_GLASS_EFFECT",
                                       FALSE, config_path);
    
    /* Read Timer section */
    snapshot->defaultStartTime = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 
                                           1500, config_path);
    
    snapshot->use24Hour = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", 
                                     FALSE, config_path);
    
    snapshot->showSeconds = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", 
                                       FALSE, config_path);
    
    char timeFormatStr[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT",
                 timeFormatStr, sizeof(timeFormatStr), config_path);
    snapshot->timeFormat = StringToEnum(TIME_FORMAT_MAP, timeFormatStr, TIME_FORMAT_DEFAULT);
    
    snapshot->showMilliseconds = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", 
                                            FALSE, config_path);
    
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0",
                 snapshot->timeoutText, sizeof(snapshot->timeoutText), config_path);
    
    char timeoutActionStr[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE",
                 timeoutActionStr, sizeof(timeoutActionStr), config_path);
    snapshot->timeoutAction = StringToEnum(TIMEOUT_ACTION_MAP, timeoutActionStr, 
                                          TIMEOUT_ACTION_MESSAGE);
    
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "",
                 snapshot->timeoutFilePath, MAX_PATH, config_path);
    
    char tempWebsiteUrl[MAX_PATH] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "",
                 tempWebsiteUrl, MAX_PATH, config_path);
    if (tempWebsiteUrl[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, tempWebsiteUrl, -1, 
                          snapshot->timeoutWebsiteUrl, MAX_PATH);
    } else {
        snapshot->timeoutWebsiteUrl[0] = L'\0';
    }
    
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN",
                 snapshot->startupMode, sizeof(snapshot->startupMode), config_path);
    
    /* Parse time options */
    char timeOptionsStr[256] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300",
                 timeOptionsStr, sizeof(timeOptionsStr), config_path);
    
    snapshot->timeOptionsCount = 0;
    char* token = strtok(timeOptionsStr, ",");
    while (token && snapshot->timeOptionsCount < MAX_TIME_OPTIONS) {
        while (*token == ' ') token++;
        snapshot->timeOptions[snapshot->timeOptionsCount++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    /* Read Pomodoro section */
    char pomodoroTimeOptions[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600",
                 pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    
    snapshot->pomodoroTimesCount = 0;
    token = strtok(pomodoroTimeOptions, ",");
    while (token && snapshot->pomodoroTimesCount < 10) {
        snapshot->pomodoroTimes[snapshot->pomodoroTimesCount++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    snapshot->pomodoroLoopCount = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 
                                            1, config_path);
    
    /* Read Notification section */
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", 
                 DEFAULT_TIMEOUT_MESSAGE,
                 snapshot->timeoutMessage, sizeof(snapshot->timeoutMessage), config_path);
    
    snapshot->notificationTimeoutMs = ReadIniInt(INI_SECTION_NOTIFICATION, 
                                                "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    
    snapshot->notificationMaxOpacity = ReadIniInt(INI_SECTION_NOTIFICATION, 
                                                 "NOTIFICATION_MAX_OPACITY", 95, config_path);
    
    char notificationTypeStr[32] = {0};
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME",
                 notificationTypeStr, sizeof(notificationTypeStr), config_path);
    snapshot->notificationType = StringToEnum(NOTIFICATION_TYPE_MAP, notificationTypeStr,
                                             NOTIFICATION_TYPE_CATIME);
    
    snapshot->notificationWindowX = ReadIniInt(INI_SECTION_NOTIFICATION,
                                              "NOTIFICATION_WINDOW_X", -1, config_path);
    snapshot->notificationWindowY = ReadIniInt(INI_SECTION_NOTIFICATION,
                                              "NOTIFICATION_WINDOW_Y", -1, config_path);
    snapshot->notificationWindowWidth = ReadIniInt(INI_SECTION_NOTIFICATION,
                                                  "NOTIFICATION_WINDOW_WIDTH", 0, config_path);
    snapshot->notificationWindowHeight = ReadIniInt(INI_SECTION_NOTIFICATION,
                                                   "NOTIFICATION_WINDOW_HEIGHT", 0, config_path);
    
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "",
                 snapshot->notificationSoundFile, MAX_PATH, config_path);
    
    snapshot->notificationSoundVolume = ReadIniInt(INI_SECTION_NOTIFICATION,
                                                  "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    
    snapshot->notificationDisabled = ReadIniBool(INI_SECTION_NOTIFICATION,
                                                "NOTIFICATION_DISABLED", FALSE, config_path);
    
    /* Read Colors section */
    ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI,
                 snapshot->colorOptions, sizeof(snapshot->colorOptions), config_path);
    
    /* Read Hotkeys section */
    char hotkeyStr[32] = {0};
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyShowTime = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyCountUp = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyCountdown = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyQuickCountdown1 = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyQuickCountdown2 = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyQuickCountdown3 = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyPomodoro = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyToggleVisibility = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyEditMode = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyPauseResume = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyRestartTimer = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None",
                 hotkeyStr, sizeof(hotkeyStr), config_path);
    snapshot->hotkeyCustomCountdown = StringToHotkey(hotkeyStr);
    
    /* Read Recent Files section */
    snapshot->recentFilesCount = 0;
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        ReadIniString(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path);
        
        if (strlen(filePath) > 0 && FileExistsUtf8(filePath)) {
            strncpy(snapshot->recentFiles[snapshot->recentFilesCount].path, 
                   filePath, MAX_PATH - 1);
            snapshot->recentFiles[snapshot->recentFilesCount].path[MAX_PATH - 1] = '\0';
            ExtractFileName(filePath, 
                          snapshot->recentFiles[snapshot->recentFilesCount].name, 
                          MAX_PATH);
            snapshot->recentFilesCount++;
        }
    }
    
    return TRUE;
}

/**
 * @brief Backward compatibility wrapper for configuration validation
 * @param snapshot Configuration snapshot to validate
 * @return TRUE if any corrections were made
 * 
 * @note This function now delegates to the modular config_recovery system
 * @see config_recovery.h for the full validation implementation
 */
BOOL ValidateConfigSnapshot(ConfigSnapshot* snapshot) {
    /* Delegate to the dedicated recovery module */
    return ValidateAndRecoverConfig(snapshot);
}
