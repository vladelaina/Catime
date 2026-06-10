/**
 * @file config_loader.c
 * @brief Configuration loading implementation
 */

#include "config/config_loader.h"
#include "config/config_recovery.h"
#include "config/config_defaults.h"
#include "config.h"
#include "window/window_core.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path || !*utf8Path) return FALSE;
    
    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) == 0) {
        return FALSE;
    }
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

/* Helper to get pointer to field in ConfigSnapshot using offset */
static inline void* GetFieldPtr(ConfigSnapshot* snapshot, size_t offset) {
    return (char*)snapshot + offset;
}

static BOOL ParsePositiveSecondsToken(const char* token, int* seconds) {
    if (!token || !seconds) return FALSE;

    while (isspace((unsigned char)*token)) token++;
    if (*token == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(token, &end, 10);
    if (end == token || errno == ERANGE ||
        parsed <= 0 || parsed > MAX_TIME_OPTION_SECONDS) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *seconds = (int)parsed;
    return TRUE;
}

static BOOL ParseConfigFloatStrict(const char* text, float* value) {
    if (!text || !value) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed)) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *value = parsed;
    return TRUE;
}

static void LoadConfigStringExactOrDefault(const char* section, const char* key,
                                           const char* defaultValue,
                                           char* dest, DWORD destSize,
                                           const char* config_path) {
    if (!dest || destSize == 0) return;

    if (!ReadIniStringExact(section, key, defaultValue ? defaultValue : "",
                            dest, destSize, config_path)) {
        LOG_WARNING("Config value too long for %s.%s, using default",
                    section ? section : "(null)",
                    key ? key : "(null)");
        if (defaultValue && strlen(defaultValue) < destSize) {
            memcpy(dest, defaultValue, strlen(defaultValue) + 1);
        } else {
            dest[0] = '\0';
        }
    }
}

static void SetDefaultQuickCountdownOptions(ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot->timeOptions, 0, sizeof(snapshot->timeOptions));
    snapshot->timeOptionsCount = DEFAULT_QUICK_COUNTDOWN_COUNT;
    snapshot->timeOptions[0] = DEFAULT_QUICK_COUNTDOWN_1;
    snapshot->timeOptions[1] = DEFAULT_QUICK_COUNTDOWN_2;
    snapshot->timeOptions[2] = DEFAULT_QUICK_COUNTDOWN_3;
}

static BOOL ParseQuickCountdownOptions(char* optionsStr, int* parsedOptions,
                                       int* parsedCount) {
    if (!optionsStr || !parsedOptions || !parsedCount) {
        return FALSE;
    }

    *parsedCount = 0;
    char* cursor = optionsStr;
    while (cursor) {
        char* next = strchr(cursor, ',');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*parsedCount >= MAX_TIME_OPTIONS) {
            LOG_WARNING("Too many countdown presets in config; maximum is %d",
                        MAX_TIME_OPTIONS);
            return FALSE;
        }

        int seconds = 0;
        if (!ParsePositiveSecondsToken(cursor, &seconds)) {
            LOG_WARNING("Invalid countdown preset in config: '%s'", cursor);
            return FALSE;
        }

        parsedOptions[*parsedCount] = seconds;
        (*parsedCount)++;
        cursor = next;
    }

    return *parsedCount > 0;
}

static void SetDefaultPomodoroTimeOptions(ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot->pomodoroTimes, 0, sizeof(snapshot->pomodoroTimes));
    snapshot->pomodoroTimesCount = DEFAULT_POMODORO_TIMES_COUNT;
    snapshot->pomodoroTimes[0] = DEFAULT_POMODORO_WORK;
    snapshot->pomodoroTimes[1] = DEFAULT_POMODORO_SHORT_BREAK;
    snapshot->pomodoroTimes[2] = DEFAULT_POMODORO_WORK;
    snapshot->pomodoroTimes[3] = DEFAULT_POMODORO_LONG_BREAK;
}

static BOOL ParsePomodoroTimeOptions(char* optionsStr, int* parsedOptions,
                                     int* parsedCount) {
    if (!optionsStr || !parsedOptions || !parsedCount) {
        return FALSE;
    }

    *parsedCount = 0;
    char* cursor = optionsStr;
    while (cursor) {
        char* next = strchr(cursor, ',');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*parsedCount >= MAX_POMODORO_TIMES) {
            LOG_WARNING("Too many Pomodoro intervals in config; maximum is %d",
                        MAX_POMODORO_TIMES);
            return FALSE;
        }

        int seconds = 0;
        if (!ParsePositiveSecondsToken(cursor, &seconds)) {
            LOG_WARNING("Invalid Pomodoro interval in config: '%s'", cursor);
            return FALSE;
        }

        parsedOptions[*parsedCount] = seconds;
        (*parsedCount)++;
        cursor = next;
    }

    return *parsedCount > 0;
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

static const EnumStrMap TEXT_EFFECT_MAP[] = {
    {TEXT_EFFECT_NONE,        "NONE"},
    {TEXT_EFFECT_GLOW,        "GLOW"},
    {TEXT_EFFECT_GLASS,       "GLASS"},
    {TEXT_EFFECT_NEON,        "NEON"},
    {TEXT_EFFECT_HOLOGRAPHIC, "HOLOGRAPHIC"},
    {TEXT_EFFECT_LIQUID,      "LIQUID"},
    {-1, NULL}
};

static int StringToEnum(const EnumStrMap* map, const char* str, int defaultVal) {
    if (!map || !str) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (_stricmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}

/* ============================================================================
 * Metadata-driven configuration loading
 * ============================================================================ */

/**
 * @brief Get enum map for a specific key
 */
static const EnumStrMap* GetEnumMapForKey(const char* key) {
    if (strcmp(key, "CLOCK_TIME_FORMAT") == 0) return TIME_FORMAT_MAP;
    if (strcmp(key, "CLOCK_TIMEOUT_ACTION") == 0) return TIMEOUT_ACTION_MAP;
    if (strcmp(key, "NOTIFICATION_TYPE") == 0) return NOTIFICATION_TYPE_MAP;
    if (strcmp(key, "TEXT_EFFECT") == 0) return TEXT_EFFECT_MAP;
    return NULL;
}

/**
 * @brief Load a single config item from INI file into ConfigSnapshot
 */
static void LoadConfigItem(const ConfigItemMeta* meta, const char* config_path, ConfigSnapshot* snapshot) {
    if (!meta || !config_path || !snapshot) return;
    
    /* Skip items that don't map to ConfigSnapshot */
    if (meta->offset == SIZE_MAX) return;
    
    /* Skip custom items - they need special handling */
    if (meta->type == CONFIG_TYPE_CUSTOM) return;
    
    void* fieldPtr = GetFieldPtr(snapshot, meta->offset);
    char buffer[2048] = {0};
    
    switch (meta->type) {
        case CONFIG_TYPE_STRING:
            LoadConfigStringExactOrDefault(meta->section, meta->key,
                                           meta->defaultValue,
                                           (char*)fieldPtr,
                                           (DWORD)meta->size,
                                           config_path);
            break;
            
        case CONFIG_TYPE_INT:
            *(int*)fieldPtr = ReadIniInt(meta->section, meta->key, 
                                         atoi(meta->defaultValue), config_path);
            break;
            
        case CONFIG_TYPE_BOOL:
            *(BOOL*)fieldPtr = ReadIniBool(meta->section, meta->key,
                                           _stricmp(meta->defaultValue, "TRUE") == 0,
                                           config_path);
            break;
            
        case CONFIG_TYPE_FLOAT:
            ReadIniString(meta->section, meta->key, meta->defaultValue,
                         buffer, sizeof(buffer), config_path);
            if (!ParseConfigFloatStrict(buffer, (float*)fieldPtr)) {
                float defaultValue = 0.0f;
                if (ParseConfigFloatStrict(meta->defaultValue, &defaultValue)) {
                    *(float*)fieldPtr = defaultValue;
                }
            }
            break;
            
        case CONFIG_TYPE_ENUM: {
            const EnumStrMap* enumMap = GetEnumMapForKey(meta->key);
            if (enumMap) {
                ReadIniString(meta->section, meta->key, meta->defaultValue,
                             buffer, sizeof(buffer), config_path);
                *(int*)fieldPtr = StringToEnum(enumMap, buffer, 
                                              StringToEnum(enumMap, meta->defaultValue, 0));
            }
            break;
        }
            
        case CONFIG_TYPE_HOTKEY:
            ReadIniString(meta->section, meta->key, meta->defaultValue,
                         buffer, sizeof(buffer), config_path);
            *(WORD*)fieldPtr = StringToHotkey(buffer);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Load all config items using metadata table
 */
static void LoadConfigFromMetadata(const char* config_path, ConfigSnapshot* snapshot) {
    int metaCount = 0;
    const ConfigItemMeta* metadata = GetConfigMetadata(&metaCount);
    
    for (int i = 0; i < metaCount; i++) {
        LoadConfigItem(&metadata[i], config_path, snapshot);
    }
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
        BOOL resolvedFontName = FALSE;
        wchar_t wConfigPath[MAX_PATH] = {0};

        if (MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH) > 0) {
            wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
            if (lastSep) {
                *lastSep = L'\0';

                wchar_t wActualFontFileName[MAX_PATH] = {0};
                if (MultiByteToWideChar(CP_UTF8, 0, actualFontFileName, -1,
                                        wActualFontFileName, MAX_PATH) > 0) {
                    wchar_t wFontPath[MAX_PATH] = {0};
                    int fontPathWritten = _snwprintf_s(wFontPath, MAX_PATH, _TRUNCATE,
                                                       L"%s\\resources\\fonts\\%s",
                                                       wConfigPath, wActualFontFileName);

                    if (fontPathWritten >= 0 &&
                        GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
                        char fontPath[MAX_PATH] = {0};
                        if (WideCharToMultiByte(CP_UTF8, 0, wFontPath, -1,
                                                fontPath, MAX_PATH, NULL, NULL) > 0 &&
                            GetFontNameFromFile(fontPath, snapshot->fontInternalName,
                                                sizeof(snapshot->fontInternalName))) {
                            resolvedFontName = TRUE;
                        }
                    }
                }
            }
        }

        if (!resolvedFontName) {
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
    snapshot->textEffect = TEXT_EFFECT_NONE;
    snapshot->defaultStartTime = DEFAULT_START_TIME_SECONDS;
    snapshot->notificationTimeoutMs = DEFAULT_NOTIFICATION_TIMEOUT_MS;
    snapshot->notificationMaxOpacity = DEFAULT_NOTIFICATION_MAX_OPACITY;
    snapshot->notificationSoundVolume = DEFAULT_NOTIFICATION_VOLUME;
}

BOOL LoadConfigFromFile(const char* config_path, ConfigSnapshot* snapshot) {
    if (!config_path || !snapshot) return FALSE;
    
    /* Initialize with defaults */
    InitializeDefaultSnapshot(snapshot);
    
    /* Load all standard config items using metadata-driven approach */
    LoadConfigFromMetadata(config_path, snapshot);
    
    /* Handle custom items that need special processing */
    
    /* Font file name needs ProcessFontPath */
    LoadConfigStringExactOrDefault(INI_SECTION_DISPLAY, "FONT_FILE_NAME",
                                   FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                                   snapshot->fontFileName,
                                   sizeof(snapshot->fontFileName),
                                   config_path);
    ProcessFontPath(snapshot, config_path);
    
    /* Website URL - now stored as UTF-8 char */
    LoadConfigStringExactOrDefault(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "",
                                   snapshot->timeoutWebsiteUrl,
                                   MAX_PATH, config_path);
    
    /* Parse time options (comma-separated array) */
    char timeOptionsStr[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL timeOptionsComplete = ReadIniStringExact(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS",
                                                  DEFAULT_TIME_OPTIONS_INI,
                                                  timeOptionsStr,
                                                  sizeof(timeOptionsStr), config_path);
    int parsedTimeOptions[MAX_TIME_OPTIONS] = {0};
    int parsedTimeOptionsCount = 0;
    if (timeOptionsComplete &&
        ParseQuickCountdownOptions(timeOptionsStr, parsedTimeOptions,
                                   &parsedTimeOptionsCount)) {
        snapshot->timeOptionsCount = parsedTimeOptionsCount;
        memcpy(snapshot->timeOptions, parsedTimeOptions,
               (size_t)parsedTimeOptionsCount * sizeof(parsedTimeOptions[0]));
    } else {
        if (!timeOptionsComplete) {
            LOG_WARNING("Countdown presets config is too long, using defaults");
        }
        SetDefaultQuickCountdownOptions(snapshot);
    }
    
    /* Parse Pomodoro time options (comma-separated array) */
    char pomodoroTimeOptions[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL pomodoroOptionsComplete = ReadIniStringExact(INI_SECTION_POMODORO,
                                                      "POMODORO_TIME_OPTIONS",
                                                      DEFAULT_POMODORO_OPTIONS_INI,
                                                      pomodoroTimeOptions,
                                                      sizeof(pomodoroTimeOptions),
                                                      config_path);
    int parsedPomodoroTimes[MAX_POMODORO_TIMES] = {0};
    int parsedPomodoroTimesCount = 0;
    if (pomodoroOptionsComplete &&
        ParsePomodoroTimeOptions(pomodoroTimeOptions, parsedPomodoroTimes,
                                 &parsedPomodoroTimesCount)) {
        snapshot->pomodoroTimesCount = parsedPomodoroTimesCount;
        memcpy(snapshot->pomodoroTimes, parsedPomodoroTimes,
               (size_t)parsedPomodoroTimesCount * sizeof(parsedPomodoroTimes[0]));
    } else {
        if (!pomodoroOptionsComplete) {
            LOG_WARNING("Pomodoro intervals config is too long, using defaults");
        }
        SetDefaultPomodoroTimeOptions(snapshot);
    }
    
    /* Read Recent Files section (dynamic keys) */
    snapshot->recentFilesCount = 0;
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        if (!ReadIniStringExact(INI_SECTION_RECENTFILES, key, "", filePath,
                                MAX_PATH, config_path)) {
            LOG_WARNING("Ignoring recent file snapshot entry %d because the config value is too long", i);
            continue;
        }
        
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
 * @brief Validate and recover configuration
 * @param snapshot Configuration snapshot to validate
 * @return TRUE if any corrections were made
 */
BOOL ValidateConfigSnapshot(ConfigSnapshot* snapshot) {
    /* Delegate to the dedicated recovery module */
    return ValidateAndRecoverConfig(snapshot);
}
