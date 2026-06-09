/**
 * @file window_utils.c
 * @brief Window procedure utility functions implementation
 */

#include "window_procedure/window_utils.h"
#include "config.h"
#include "language.h"
#include "log.h"
#include "utils/string_convert.h"
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * String Constants
 * ============================================================================ */

const char* const STR_TRUE = "TRUE";
const char* const STR_FALSE = "FALSE";
const char* const STR_NONE = "None";
const char* const STR_DEFAULT = "DEFAULT";
const char* const STR_MESSAGE = "MESSAGE";
const char* const STR_OK = "OK";

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

WideString ToWide(const char* utf8) {
    WideString ws = {{0}, FALSE};
    if (utf8) {
        ws.valid = Utf8ToWide(utf8, ws.buf, MAX_PATH);
    }
    return ws;
}

Utf8String ToUtf8(const wchar_t* wide) {
    Utf8String us = {{0}, FALSE};
    if (wide) {
        us.valid = WideToUtf8(wide, us.buf, MAX_PATH);
    }
    return us;
}

/* ============================================================================
 * Path Operations Implementation
 * ============================================================================ */

/* PathJoinW and GetRelativePathW are defined in src/utils/path_utils.c */

/* ============================================================================
 * Error Handling Implementation
 * ============================================================================ */

void ShowError(HWND hwnd, ErrorCode errorCode) {
    UNREFERENCED_PARAMETER(hwnd);
    const wchar_t* message;
    
    switch (errorCode) {
        case ERR_FILE_NOT_FOUND:
            message = GetLocalizedString(NULL, L"Selected file does not exist");
            break;
        case ERR_INVALID_INPUT:
            message = GetLocalizedString(NULL, L"Invalid input format");
            break;
        case ERR_BUFFER_TOO_SMALL:
            message = GetLocalizedString(NULL, L"Buffer too small");
            break;
        case ERR_OPERATION_FAILED:
            message = GetLocalizedString(NULL, L"Operation failed");
            break;
        default:
            message = GetLocalizedString(NULL, L"Unknown error");
    }
    
    /* Log error instead of showing MessageBox for better UX
     * Convert wide string to UTF-8 for logging */
    char errorMsg[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, message, -1, errorMsg, sizeof(errorMsg), NULL, NULL);
    LOG_ERROR("%s (ErrorCode: %d)", errorMsg, errorCode);
    
    /* Optionally show non-blocking notification instead of blocking MessageBox
     * Uncomment if you want to show toast notification:
     * extern void ShowNotification(HWND hwnd, const wchar_t* message);
     * ShowNotification(hwnd, message);
     */
}

/* ============================================================================
 * Configuration Access Implementation
 * ============================================================================ */

#define CONFIG_PATH_CACHE_UNINITIALIZED 0
#define CONFIG_PATH_CACHE_INITIALIZING  1
#define CONFIG_PATH_CACHE_READY         2
#define CONFIG_PATH_CACHE_WAIT_SPIN_LIMIT 64

static char g_configPathCache[MAX_PATH] = {0};
static volatile LONG g_configPathCacheState = CONFIG_PATH_CACHE_UNINITIALIZED;

static void WaitWhileConfigPathCacheInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_configPathCacheState, 0, 0) ==
           CONFIG_PATH_CACHE_INITIALIZING) {
        Sleep(spins++ < CONFIG_PATH_CACHE_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

const char* GetCachedConfigPath(void) {
    for (;;) {
        LONG state = InterlockedCompareExchange(&g_configPathCacheState, 0, 0);
        if (state == CONFIG_PATH_CACHE_READY) {
            return g_configPathCache;
        }

        if (InterlockedCompareExchange(&g_configPathCacheState,
                                       CONFIG_PATH_CACHE_INITIALIZING,
                                       CONFIG_PATH_CACHE_UNINITIALIZED) == CONFIG_PATH_CACHE_UNINITIALIZED) {
            char tempPath[MAX_PATH] = {0};
            GetConfigPath(tempPath, MAX_PATH);
            if (tempPath[0] != '\0') {
                memcpy(g_configPathCache, tempPath, MAX_PATH);
                InterlockedExchange(&g_configPathCacheState, CONFIG_PATH_CACHE_READY);
            } else {
                g_configPathCache[0] = '\0';
                InterlockedExchange(&g_configPathCacheState, CONFIG_PATH_CACHE_UNINITIALIZED);
            }
            return g_configPathCache;
        }

        WaitWhileConfigPathCacheInitializing();
    }
}

void ReadConfigStr(const char* section, const char* key, 
                   const char* defaultVal, char* out, size_t size) {
    ReadIniString(section, key, defaultVal, out, (int)size, GetCachedConfigPath());
}

int ReadConfigInt(const char* section, const char* key, int defaultVal) {
    return ReadIniInt(section, key, defaultVal, GetCachedConfigPath());
}

BOOL ReadConfigBool(const char* section, const char* key, BOOL defaultVal) {
    return ReadIniBool(section, key, defaultVal, GetCachedConfigPath());
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

BOOL isAllSpacesOnly(const wchar_t* str) {
    if (!str || str[0] == L'\0') return TRUE;
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) return FALSE;
    }
    return TRUE;
}

void ClearInputBuffer(wchar_t* buffer, size_t size) {
    memset(buffer, 0, size);
}

void UpdateConfigWithRefresh(HWND hwnd, const char* key, const char* value) {
    if (WriteConfigKeyValue(key, value) && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void ToggleConfigBool(HWND hwnd, const char* key, bool* currentValue, bool needsRedraw) {
    if (!key || !currentValue) {
        return;
    }

    bool newValue = !(*currentValue);
    if (!WriteConfigKeyValue(key, newValue ? "TRUE" : "FALSE")) {
        return;
    }

    *currentValue = newValue;
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void WriteConfigAndRedraw(HWND hwnd, const char* key, const char* value) {
    if (WriteConfigKeyValue(key, value) && hwnd) InvalidateRect(hwnd, NULL, TRUE);
}

