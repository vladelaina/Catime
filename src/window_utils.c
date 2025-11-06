/**
 * @file window_utils.c
 * @brief Window procedure utility functions implementation
 */

#include "../include/window_utils.h"
#include "../include/config.h"
#include "../include/language.h"
#include "../include/utils/string_convert.h"
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
    const wchar_t* title = GetLocalizedString(L"错误", L"Error");
    const wchar_t* message;
    
    switch (errorCode) {
        case ERR_FILE_NOT_FOUND:
            message = GetLocalizedString(L"所选文件不存在", L"Selected file does not exist");
            break;
        case ERR_INVALID_INPUT:
            message = GetLocalizedString(L"输入格式不正确", L"Invalid input format");
            break;
        case ERR_BUFFER_TOO_SMALL:
            message = GetLocalizedString(L"缓冲区太小", L"Buffer too small");
            break;
        case ERR_OPERATION_FAILED:
            message = GetLocalizedString(L"操作失败", L"Operation failed");
            break;
        default:
            message = GetLocalizedString(L"未知错误", L"Unknown error");
    }
    
    MessageBoxW(hwnd, message, title, MB_ICONERROR);
}

/* ============================================================================
 * Configuration Access Implementation
 * ============================================================================ */

static __declspec(thread) char g_configPathCache[MAX_PATH] = {0};
static __declspec(thread) BOOL g_configPathCached = FALSE;

const char* GetCachedConfigPath(void) {
    if (!g_configPathCached) {
        GetConfigPath(g_configPathCache, MAX_PATH);
        g_configPathCached = TRUE;
    }
    return g_configPathCache;
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
    WriteConfigKeyValue(key, value);
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void ToggleConfigBool(HWND hwnd, const char* key, BOOL* currentValue, BOOL needsRedraw) {
    *currentValue = !(*currentValue);
    WriteConfigKeyValue(key, *currentValue ? "TRUE" : "FALSE");
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void WriteConfigAndRedraw(HWND hwnd, const char* key, const char* value) {
    WriteConfigKeyValue(key, value);
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
}

