/**
 * @file config_ini_internal.h
 * @brief Shared UTF-8 INI parser, cache, and writer implementation details.
 */

#ifndef CATIME_CONFIG_INI_INTERNAL_H
#define CATIME_CONFIG_INI_INTERNAL_H

#include "config.h"
#include "log.h"
#include "utils/string_safe.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define INI_MAX_LINE_LENGTH 4096
#define INI_MAX_PARSE_LINES 8192
#define INI_MAX_PARSE_ENTRIES 2048
#define INI_CACHE_STAT_THROTTLE_MS 100u
#define INI_MAX_FILE_BYTES (1024ull * 1024ull)
#define INI_CS_UNINITIALIZED 0
#define INI_CS_INITIALIZING 1
#define INI_CS_INITIALIZED 2
#define INI_WAIT_SPIN_LIMIT 64u
#define CONFIG_WRITE_LOCK_TIMEOUT_MS 2000u

typedef struct IniEntry {
    char* key;
    char* value;
    struct IniEntry* next;
} IniEntry;

typedef struct IniSection {
    char* name;
    IniEntry* entries;
    IniEntry* lastEntry;
    struct IniSection* next;
} IniSection;

typedef struct {
    IniSection* sections;
    IniSection* lastSection;
    char filePath[MAX_PATH];
    BOOL dirty;
    FILETIME lastWriteTime;
    ULONGLONG lastStatCheckTick;
} IniFile;

extern IniFile* g_ConfigIni;
extern CRITICAL_SECTION g_IniCriticalSection;
extern volatile LONG g_IniCriticalSectionInitialized;
extern HANDLE g_ConfigWriteMutex;

void AcquireIniLock(void);
void ReleaseIniLock(void);
BOOL AcquireConfigWriteLock(void);
void ReleaseConfigWriteAndIniLocks(void);

char* StrDup(const char* value);
char* TrimWhitespace(char* value);
BOOL StrEqualNoCase(const char* left, const char* right);
BOOL CreateTempFilePathForTargetUtf8(const char* targetPath,
                                     char* tempPath, size_t tempPathSize);
FILE* OpenFileUtf8(const char* path, const wchar_t* mode);
BOOL GetFileTimeUtf8(const char* path, FILETIME* time);
BOOL GetFileSizeUtf8(const char* path, ULONGLONG* size);
ULONGLONG GetIniCacheTickMs(void);
BOOL DeleteFileUtf8(const char* path);
BOOL MoveFileUtf8(const char* from, const char* to);
BOOL FileExistsUtf8(const char* path);

void FreeIniFile(IniFile* ini);
IniSection* FindSection(IniFile* ini, const char* name);
IniSection* CreateSection(IniFile* ini, const char* name);
IniEntry* FindEntry(IniSection* section, const char* key);
IniEntry* CreateEntry(IniSection* section, const char* key,
                      const char* value);
IniFile* CloneIniFile(const IniFile* source);

IniFile* ParseIniFile(const char* filePath);
BOOL WriteIniAtomically(IniFile* ini);
BOOL RefreshCleanIniCacheIfChanged(const char* filePath);
BOOL RefreshCleanIniCacheForWrite(const char* filePath);
const char* GetIniValue(const char* section, const char* key,
                        const char* filePath);
BOOL SetIniValueInMemory(IniFile* ini, const char* section,
                         const char* key, const char* value);
BOOL IniValueMatches(IniFile* ini, const char* section,
                     const char* key, const char* value);
BOOL IniUpdatesMatch(IniFile* ini, const IniKeyValue* updates, size_t count);

#endif /* CATIME_CONFIG_INI_INTERNAL_H */
