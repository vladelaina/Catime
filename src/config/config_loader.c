#include "config/config_loader.h"
#include "config/config_recovery.h"
#include "config/config_defaults.h"
#include "config_loader_internal.h"
#include "config.h"
#include "text_effect.h"
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
static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path || !*utf8Path) return FALSE;
    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) == 0) {
        return FALSE;
    }
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}
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
    if (end == token || errno == ERANGE || parsed <= 0 || parsed > MAX_TIME_OPTION_SECONDS) {
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
static void LoadConfigStringExactOrDefault(const char* section, const char* key, const char* defaultValue, char* dest, DWORD destSize, const char* config_path) {
    if (!dest || destSize == 0) return;
    if (!ReadIniStringExact(section, key, defaultValue ? defaultValue : "", dest, destSize, config_path)) {
        LOG_WARNING("Config value too long for %s.%s, using default", section ? section : "(null)", key ? key : "(null)");
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
static BOOL ParseQuickCountdownOptions(char* optionsStr, int* parsedOptions, int* parsedCount) {
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
            LOG_WARNING("Too many countdown presets in config; maximum is %d", MAX_TIME_OPTIONS);
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
static BOOL ParsePomodoroTimeOptions(char* optionsStr, int* parsedOptions, int* parsedCount) {
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
            LOG_WARNING("Too many Pomodoro intervals in config; maximum is %d", MAX_POMODORO_TIMES);
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
        if (_stricmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}
static const EnumStrMap* GetEnumMapForKey(const char* key) {
    if (strcmp(key, "CLOCK_TIME_FORMAT") == 0) return TIME_FORMAT_MAP;
    if (strcmp(key, "CLOCK_TIMEOUT_ACTION") == 0) return TIMEOUT_ACTION_MAP;
    if (strcmp(key, "NOTIFICATION_TYPE") == 0) return NOTIFICATION_TYPE_MAP;
    return NULL;
}
static void LoadConfigItem(const ConfigItemMeta* meta, const char* config_path, ConfigSnapshot* snapshot) {
    if (!meta || !config_path || !snapshot) return;
    if (meta->offset == SIZE_MAX) return;
    if (meta->type == CONFIG_TYPE_CUSTOM) return;
    void* fieldPtr = GetFieldPtr(snapshot, meta->offset);
    char buffer[2048] = {0};
    switch (meta->type) {
        case CONFIG_TYPE_STRING:
            LoadConfigStringExactOrDefault(meta->section, meta->key, meta->defaultValue, (char*)fieldPtr, (DWORD)meta->size, config_path);
            break;
        case CONFIG_TYPE_INT:
            *(int*)fieldPtr = ReadIniInt(meta->section, meta->key, atoi(meta->defaultValue), config_path);
            break;
        case CONFIG_TYPE_BOOL:
            *(BOOL*)fieldPtr = ReadIniBool(meta->section, meta->key, _stricmp(meta->defaultValue, "TRUE") == 0, config_path);
            break;
        case CONFIG_TYPE_FLOAT:
            ReadIniString(meta->section, meta->key, meta->defaultValue, buffer, sizeof(buffer), config_path);
            if (!ParseConfigFloatStrict(buffer, (float*)fieldPtr)) {
                float defaultValue = 0.0f;
                if (ParseConfigFloatStrict(meta->defaultValue, &defaultValue)) {
                    *(float*)fieldPtr = defaultValue;
                }
            }
            break;
        case CONFIG_TYPE_ENUM: {
            if (strcmp(meta->key, "TEXT_EFFECT") == 0) {
                ReadIniString(meta->section, meta->key, meta->defaultValue, buffer, sizeof(buffer), config_path);
                *(int*)fieldPtr = TextEffect_FromConfigString(buffer);
                break;
            }
            const EnumStrMap* enumMap = GetEnumMapForKey(meta->key);
            if (enumMap) {
                ReadIniString(meta->section, meta->key, meta->defaultValue, buffer, sizeof(buffer), config_path);
                *(int*)fieldPtr = StringToEnum(enumMap, buffer, StringToEnum(enumMap, meta->defaultValue, 0));
            }
            break;
        }
        case CONFIG_TYPE_HOTKEY:
            ReadIniString(meta->section, meta->key, meta->defaultValue, buffer, sizeof(buffer), config_path);
            *(WORD*)fieldPtr = StringToHotkey(buffer);
            break;
        default:
            break;
    }
}
static void LoadConfigFromMetadata(const char* config_path, ConfigSnapshot* snapshot) {
    int metaCount = 0;
    const ConfigItemMeta* metadata = GetConfigMetadata(&metaCount);
    for (int i = 0; i < metaCount; i++) {
        LoadConfigItem(&metadata[i], config_path, snapshot);
    }
}
BOOL LoadConfigFromFile(const char* config_path, ConfigSnapshot* snapshot) {
    if (!config_path || !snapshot) return FALSE;
    InitializeDefaultSnapshot(snapshot);
    LoadConfigFromMetadata(config_path, snapshot);
    LoadConfigStringExactOrDefault(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONTS_PATH_PREFIX DEFAULT_FONT_NAME, snapshot->fontFileName, sizeof(snapshot->fontFileName), config_path);
    ProcessConfigFontPath(snapshot, config_path);
    LoadConfigStringExactOrDefault(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", snapshot->timeoutWebsiteUrl, MAX_PATH, config_path);
    char timeOptionsStr[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL timeOptionsComplete = ReadIniStringExact(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", DEFAULT_TIME_OPTIONS_INI, timeOptionsStr, sizeof(timeOptionsStr), config_path);
    int parsedTimeOptions[MAX_TIME_OPTIONS] = {0};
    int parsedTimeOptionsCount = 0;
    if (timeOptionsComplete && ParseQuickCountdownOptions(timeOptionsStr, parsedTimeOptions, &parsedTimeOptionsCount)) {
        snapshot->timeOptionsCount = parsedTimeOptionsCount;
        memcpy(snapshot->timeOptions, parsedTimeOptions, (size_t)parsedTimeOptionsCount * sizeof(parsedTimeOptions[0]));
    } else {
        if (!timeOptionsComplete) {
            LOG_WARNING("Countdown presets config is too long, using defaults");
        }
        SetDefaultQuickCountdownOptions(snapshot);
    }
    char pomodoroTimeOptions[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL pomodoroOptionsComplete = ReadIniStringExact(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", DEFAULT_POMODORO_OPTIONS_INI, pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    int parsedPomodoroTimes[MAX_POMODORO_TIMES] = {0};
    int parsedPomodoroTimesCount = 0;
    if (pomodoroOptionsComplete && ParsePomodoroTimeOptions(pomodoroTimeOptions, parsedPomodoroTimes, &parsedPomodoroTimesCount)) {
        snapshot->pomodoroTimesCount = parsedPomodoroTimesCount;
        memcpy(snapshot->pomodoroTimes, parsedPomodoroTimes, (size_t)parsedPomodoroTimesCount * sizeof(parsedPomodoroTimes[0]));
    } else {
        if (!pomodoroOptionsComplete) {
            LOG_WARNING("Pomodoro intervals config is too long, using defaults");
        }
        SetDefaultPomodoroTimeOptions(snapshot);
    }
    snapshot->recentFilesCount = 0;
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        char filePath[MAX_PATH] = {0};
        if (!ReadIniStringExact(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path)) {
            LOG_WARNING("Ignoring recent file snapshot entry %d because the config value is too long", i);
            continue;
        }
        if (strlen(filePath) > 0 && FileExistsUtf8(filePath)) {
            strncpy(snapshot->recentFiles[snapshot->recentFilesCount].path, filePath, MAX_PATH - 1);
            snapshot->recentFiles[snapshot->recentFilesCount].path[MAX_PATH - 1] = '\0';
            ExtractFileName(filePath, snapshot->recentFiles[snapshot->recentFilesCount].name, MAX_PATH);
            snapshot->recentFilesCount++;
        }
    }
    return TRUE;
}
BOOL ValidateConfigSnapshot(ConfigSnapshot* snapshot) {
    return ValidateAndRecoverConfig(snapshot);
}
