/**
 * @file config_ini_utils.c
 * @brief String helpers and Unicode-safe filesystem operations for INI files.
 */

#include "config_ini_internal.h"

char* StrDup(const char* value) {
    if (!value) return NULL;
    size_t length = strlen(value) + 1;
    char* copy = (char*)malloc(length);
    if (copy) memcpy(copy, value, length);
    return copy;
}

char* TrimWhitespace(char* value) {
    if (!value) return NULL;
    while (*value && isspace((unsigned char)*value)) ++value;
    if (!*value) return value;
    char* end = value + strlen(value) - 1;
    while (end > value && isspace((unsigned char)*end)) --end;
    end[1] = '\0';
    return value;
}

BOOL StrEqualNoCase(const char* left, const char* right) {
    if (!left || !right) return left == right;
    return _stricmp(left, right) == 0;
}

static BOOL Utf8PathToWide(const char* path, wchar_t* wide,
                           size_t wideCount) {
    if (!wide || !wideCount) return FALSE;
    wide[0] = L'\0';
    if (!path || wideCount > INT_MAX) return FALSE;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1,
                            wide, (int)wideCount) <= 0) {
        wide[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL WidePathToUtf8(const wchar_t* path, char* utf8,
                           size_t utf8Count) {
    if (!utf8 || !utf8Count) return FALSE;
    utf8[0] = '\0';
    if (!path || utf8Count > INT_MAX) return FALSE;
    if (WideCharToMultiByte(CP_UTF8, 0, path, -1,
                            utf8, (int)utf8Count, NULL, NULL) <= 0) {
        utf8[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL CreateTempFilePathForTargetUtf8(const char* targetPath,
                                     char* tempPath,
                                     size_t tempPathSize) {
    if (!targetPath || !tempPath || !tempPathSize) return FALSE;
    tempPath[0] = '\0';
    wchar_t target[MAX_PATH] = {0};
    if (!Utf8PathToWide(targetPath, target, MAX_PATH)) return FALSE;

    wchar_t directory[MAX_PATH] = {0};
    safe_wcsncpy(directory, target, MAX_PATH);
    wchar_t* separator = wcsrchr(directory, L'\\');
    wchar_t* forward = wcsrchr(directory, L'/');
    if (!separator || (forward && forward > separator)) separator = forward;
    if (separator) *separator = L'\0';
    else wcscpy_s(directory, MAX_PATH, L".");

    wchar_t temp[MAX_PATH] = {0};
    if (GetTempFileNameW(directory, L"cti", 0, temp) == 0) {
        LOG_ERROR("Failed to create config temp file in directory for: %s (error=%lu)",
                  targetPath, GetLastError());
        return FALSE;
    }
    if (!WidePathToUtf8(temp, tempPath, tempPathSize)) {
        DeleteFileW(temp);
        return FALSE;
    }
    return TRUE;
}

FILE* OpenFileUtf8(const char* path, const wchar_t* mode) {
    if (!path || !mode) return NULL;
    wchar_t wide[MAX_PATH] = {0};
    return Utf8PathToWide(path, wide, MAX_PATH) ? _wfopen(wide, mode) : NULL;
}

BOOL GetFileTimeUtf8(const char* path, FILETIME* time) {
    if (!path || !time) return FALSE;
    wchar_t wide[MAX_PATH] = {0};
    if (!Utf8PathToWide(path, wide, MAX_PATH)) return FALSE;
    HANDLE file = CreateFileW(
        wide, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    BOOL result = GetFileTime(file, NULL, NULL, time);
    CloseHandle(file);
    return result;
}

BOOL GetFileSizeUtf8(const char* path, ULONGLONG* size) {
    if (!path || !size) return FALSE;
    *size = 0;
    wchar_t wide[MAX_PATH] = {0};
    if (!Utf8PathToWide(path, wide, MAX_PATH)) return FALSE;
    HANDLE file = CreateFileW(
        wide, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER length;
    BOOL result = GetFileSizeEx(file, &length);
    CloseHandle(file);
    if (!result || length.QuadPart < 0) return FALSE;
    *size = (ULONGLONG)length.QuadPart;
    return TRUE;
}

ULONGLONG GetIniCacheTickMs(void) {
    return GetTickCount64();
}

static BOOL ConvertSinglePath(const char* path, wchar_t wide[MAX_PATH]) {
    return path && Utf8PathToWide(path, wide, MAX_PATH);
}

BOOL DeleteFileUtf8(const char* path) {
    wchar_t wide[MAX_PATH] = {0};
    return ConvertSinglePath(path, wide) && DeleteFileW(wide);
}

BOOL MoveFileUtf8(const char* from, const char* to) {
    wchar_t wideFrom[MAX_PATH] = {0};
    wchar_t wideTo[MAX_PATH] = {0};
    return ConvertSinglePath(from, wideFrom) && ConvertSinglePath(to, wideTo) &&
           MoveFileExW(wideFrom, wideTo,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

BOOL FileExistsUtf8(const char* path) {
    wchar_t wide[MAX_PATH] = {0};
    return ConvertSinglePath(path, wide) &&
           GetFileAttributesW(wide) != INVALID_FILE_ATTRIBUTES;
}
