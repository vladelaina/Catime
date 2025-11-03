/**
 * @file config_ini.c
 * @brief INI file basic read/write operations with UTF-8 support
 * 
 * Provides low-level INI file access functions with UTF-8 encoding support,
 * atomic write operations, and mutex synchronization for thread safety.
 */
#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* UTF8_TO_WIDE(utf8Path, wPath) → wchar_t wPath[MAX_PATH]; */
#define UTF8_TO_WIDE(utf8, wide) \
    wchar_t wide[MAX_PATH] = {0}; \
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, MAX_PATH)

#define UTF8_TO_WIDE_N(utf8, wide, size) \
    wchar_t wide[size] = {0}; \
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, size)

#define WIDE_TO_UTF8(wide, utf8, size) \
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, (int)(size), NULL, NULL)

#define FOPEN_UTF8(utf8Path, mode, filePtr) \
    wchar_t _w##filePtr[MAX_PATH] = {0}; \
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, _w##filePtr, MAX_PATH); \
    FILE* filePtr = _wfopen(_w##filePtr, mode)

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;
    UTF8_TO_WIDE(utf8Path, wPath);
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

/**
 * @brief Global mutex for serializing config writes across threads/processes
 */
static HANDLE GetConfigWriteMutex(void) {
    static HANDLE hMutex = NULL;
    if (hMutex == NULL) {
        hMutex = CreateMutexW(NULL, FALSE, L"CatimeConfigWriteMutex");
    }
    return hMutex;
}

/**
 * @brief Acquire global config write lock
 */
static void AcquireConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (h) {
        WaitForSingleObject(h, INFINITE);
    }
}

/**
 * @brief Release global config write lock
 */
static void ReleaseConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (h) {
        ReleaseMutex(h);
    }
}

/**
 * @brief Read string value from INI file with Unicode support
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath) {

    /** Convert all parameters to wide char */
    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE_N(defaultValue, wdefaultValue, 1024);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    wchar_t wreturnValue[1024];
    DWORD result = GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wreturnValue, 1024, wfilePath);
    
    /** Convert result back to UTF-8 */
    WIDE_TO_UTF8(wreturnValue, returnValue, returnSize);
    
    return result;
}


/**
 * @brief Write string value to INI file with Unicode support
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath) {

    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE_N(value, wvalue, 1024);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Read integer value from INI file with Unicode support
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath) {

    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    return GetPrivateProfileIntW(wsection, wkey, defaultValue, wfilePath);
}


/**
 * @brief Write integer value to INI file with Unicode support
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    
    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE_N(valueStr, wvalue, 32);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Write boolean value to INI file with Unicode support
 */
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
               const char* filePath) {
    const char* valueStr = value ? "TRUE" : "FALSE";
    
    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE_N(valueStr, wvalue, 8);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Unified atomic configuration update function (thread-safe, single key-value)
 */
BOOL UpdateConfigKeyValueAtomic(const char* section, const char* key, const char* value) {
    if (!section || !key || !value) return FALSE;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    AcquireConfigWriteLock();
    BOOL result = WriteIniString(section, key, value, config_path);
    ReleaseConfigWriteLock();
    
    return result;
}


/**
 * @brief Unified atomic configuration update function for integer values
 */
BOOL UpdateConfigIntAtomic(const char* section, const char* key, int value) {
    if (!section || !key) return FALSE;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    AcquireConfigWriteLock();
    BOOL result = WriteIniInt(section, key, value, config_path);
    ReleaseConfigWriteLock();
    
    return result;
}


/**
 * @brief Unified atomic configuration update function for boolean values
 */
BOOL UpdateConfigBoolAtomic(const char* section, const char* key, BOOL value) {
    if (!section || !key) return FALSE;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    AcquireConfigWriteLock();
    BOOL result = WriteIniBool(section, key, value, config_path);
    ReleaseConfigWriteLock();
    
    return result;
}


/**
 * @brief Read boolean value from INI file with Unicode support
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath) {
    char value[8];
    const char* defaultStr = defaultValue ? "TRUE" : "FALSE";
    
    UTF8_TO_WIDE_N(section, wsection, 256);
    UTF8_TO_WIDE_N(key, wkey, 256);
    UTF8_TO_WIDE_N(defaultStr, wdefaultValue, 8);
    UTF8_TO_WIDE(filePath, wfilePath);
    
    wchar_t wvalue[8];
    GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wvalue, 8, wfilePath);
    
    /** Convert result back to UTF-8 and compare */
    WIDE_TO_UTF8(wvalue, value, sizeof(value));
    
    return _stricmp(value, "TRUE") == 0;
}


/**
 * @brief Check if file exists with Unicode support
 */
BOOL FileExists(const char* filePath) {
    return FileExistsUtf8(filePath);
}


/**
 * @brief Force flush configuration changes to disk immediately
 */
void FlushConfigToDisk(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert to wide character for Windows API */
    UTF8_TO_WIDE(config_path, wconfig_path);
    
    /** Force flush file system buffers to ensure immediate disk write */
    HANDLE hFile = CreateFileW(wconfig_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
}

