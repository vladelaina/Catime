/**
 * @file config_misc.c
 * @brief Enum conversion helpers for miscellaneous configuration
 */

#include "config_misc_internal.h"

static const ConfigMiscEnumStrMap TIME_FORMAT_MAP[] = {
    {TIME_FORMAT_DEFAULT, "DEFAULT"},
    {TIME_FORMAT_ZERO_PADDED, "ZERO_PADDED"},
    {TIME_FORMAT_FULL_PADDED, "FULL_PADDED"},
    {-1, NULL}
};

static const ConfigMiscEnumStrMap TIMEOUT_ACTION_MAP[] = {
    {TIMEOUT_ACTION_MESSAGE, "MESSAGE"},
    {TIMEOUT_ACTION_LOCK, "LOCK"},
    {TIMEOUT_ACTION_OPEN_FILE, "OPEN_FILE"},
    {TIMEOUT_ACTION_SHOW_TIME, "SHOW_TIME"},
    {TIMEOUT_ACTION_COUNT_UP, "COUNT_UP"},
    {TIMEOUT_ACTION_OPEN_WEBSITE, "OPEN_WEBSITE"},
    {TIMEOUT_ACTION_SLEEP, "SLEEP"},
    {TIMEOUT_ACTION_SHUTDOWN, "SHUTDOWN"},
    {TIMEOUT_ACTION_RESTART, "RESTART"},
    {-1, NULL}
};

const char* ConfigMisc_EnumToString(
    const ConfigMiscEnumStrMap* map, int value, const char* defaultValue) {
    if (!map) {
        return defaultValue;
    }
    for (int i = 0; map[i].str != NULL; i++) {
        if (map[i].value == value) {
            return map[i].str;
        }
    }
    return defaultValue;
}

int ConfigMisc_StringToEnum(
    const ConfigMiscEnumStrMap* map, const char* value, int defaultValue) {
    if (!map || !value) {
        return defaultValue;
    }
    for (int i = 0; map[i].str != NULL; i++) {
        if (_stricmp(map[i].str, value) == 0) {
            return map[i].value;
        }
    }
    return defaultValue;
}

TimeFormatType TimeFormatType_FromStr(const char* value) {
    return (TimeFormatType)ConfigMisc_StringToEnum(
        TIME_FORMAT_MAP, value, TIME_FORMAT_DEFAULT);
}

const char* TimeFormatType_ToStr(TimeFormatType value) {
    return ConfigMisc_EnumToString(TIME_FORMAT_MAP, value, "DEFAULT");
}

TimeoutActionType TimeoutActionType_FromStr(const char* value) {
    return (TimeoutActionType)ConfigMisc_StringToEnum(
        TIMEOUT_ACTION_MAP, value, TIMEOUT_ACTION_MESSAGE);
}

const char* TimeoutActionType_ToStr(TimeoutActionType value) {
    return ConfigMisc_EnumToString(
        TIMEOUT_ACTION_MAP, value, "MESSAGE");
}
