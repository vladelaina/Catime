/**
 * @file config_ini_read.c
 * @brief Thread-safe string, integer, and boolean INI reads.
 */

#include "config_ini_internal.h"

static const char* ReadValueLocked(const char* section, const char* key,
                                   const char* filePath) {
    return RefreshCleanIniCacheIfChanged(filePath)
        ? GetIniValue(section, key, filePath) : NULL;
}

DWORD ReadIniString(const char* section, const char* key,
                    const char* defaultValue, char* returnValue,
                    DWORD returnSize, const char* filePath) {
    if (!returnValue || returnSize == 0) return 0;
    AcquireIniLock();
    const char* value = ReadValueLocked(section, key, filePath);
    const char* result = value ? value : (defaultValue ? defaultValue : "");
    strncpy(returnValue, result, returnSize - 1);
    returnValue[returnSize - 1] = '\0';
    DWORD length = (DWORD)strlen(returnValue);
    ReleaseIniLock();
    return length;
}

BOOL ReadIniStringExact(const char* section, const char* key,
                        const char* defaultValue, char* returnValue,
                        DWORD returnSize, const char* filePath) {
    if (!returnValue || returnSize == 0) return FALSE;
    AcquireIniLock();
    const char* value = ReadValueLocked(section, key, filePath);
    const char* result = value ? value : (defaultValue ? defaultValue : "");
    size_t length = strlen(result);
    if (length >= returnSize) {
        returnValue[0] = '\0';
        ReleaseIniLock();
        return FALSE;
    }
    memcpy(returnValue, result, length + 1);
    ReleaseIniLock();
    return TRUE;
}

int ReadIniInt(const char* section, const char* key, int defaultValue,
               const char* filePath) {
    AcquireIniLock();
    const char* value = ReadValueLocked(section, key, filePath);
    int result = defaultValue;
    if (value && *value) {
        char* end = NULL;
        errno = 0;
        long parsed = strtol(value, &end, 10);
        if (end != value && errno != ERANGE &&
            parsed >= (long)INT_MIN && parsed <= (long)INT_MAX) {
            result = (int)parsed;
        }
    }
    ReleaseIniLock();
    return result;
}

BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue,
                 const char* filePath) {
    AcquireIniLock();
    const char* value = ReadValueLocked(section, key, filePath);
    BOOL result = defaultValue;
    if (value && *value) {
        result = StrEqualNoCase(value, "TRUE") ||
                 StrEqualNoCase(value, "1") ||
                 StrEqualNoCase(value, "YES");
    }
    ReleaseIniLock();
    return result;
}
