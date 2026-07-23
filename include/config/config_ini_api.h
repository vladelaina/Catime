/**
 * @file config_ini_api.h
 * @brief Low-level UTF-8 INI cache and atomic-write API.
 */

#ifndef CATIME_CONFIG_INI_API_H
#define CATIME_CONFIG_INI_API_H

#include <stddef.h>
#include <windows.h>

typedef struct {
    const char* section;
    const char* key;
    const char* value;
} IniKeyValue;

DWORD ReadIniString(const char* section, const char* key,
                    const char* defaultValue, char* returnValue,
                    DWORD returnSize, const char* filePath);
BOOL ReadIniStringExact(const char* section, const char* key,
                        const char* defaultValue, char* returnValue,
                        DWORD returnSize, const char* filePath);
BOOL WriteIniString(const char* section, const char* key, const char* value,
                    const char* filePath);
int ReadIniInt(const char* section, const char* key, int defaultValue,
               const char* filePath);
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue,
                 const char* filePath);
BOOL WriteIniInt(const char* section, const char* key, int value,
                 const char* filePath);
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
                  const char* filePath);
BOOL WriteIniMultipleAtomic(const char* filePath,
                            const IniKeyValue* updates, size_t count);
void InvalidateIniCache(void);
BOOL FlushConfigToDisk(void);
void ShutdownIniCache(void);

#endif /* CATIME_CONFIG_INI_API_H */
