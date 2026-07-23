#include "utils/path_utils.h"

#include <wchar.h>

static BOOL IsExistingFile(const wchar_t* path) {
    if (!path || !*path) return FALSE;
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL GetShortcutExecutablePathW(wchar_t* path, size_t pathSize) {
    if (!path || pathSize == 0 || pathSize > MAXDWORD) return FALSE;
    path[0] = L'\0';

    wchar_t modulePath[MAX_PATH] = {0};
    DWORD modulePathLength = GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    if (modulePathLength == 0 || modulePathLength >= MAX_PATH) return FALSE;

    const wchar_t* selectedPath = modulePath;
    wchar_t versionDir[MAX_PATH] = {0};
    wchar_t packageDir[MAX_PATH] = {0};
    wchar_t appsDir[MAX_PATH] = {0};
    wchar_t stablePath[MAX_PATH] = {0};
    if (ExtractDirectoryW(modulePath, versionDir, MAX_PATH) &&
        ExtractDirectoryW(versionDir, packageDir, MAX_PATH) &&
        ExtractDirectoryW(packageDir, appsDir, MAX_PATH) &&
        _wcsicmp(GetFileNameW(appsDir), L"apps") == 0 &&
        wcscpy_s(stablePath, MAX_PATH, packageDir) == 0 &&
        PathJoinW(stablePath, MAX_PATH, L"current") &&
        PathJoinW(stablePath, MAX_PATH, GetFileNameW(modulePath)) &&
        IsExistingFile(stablePath)) {
        selectedPath = stablePath;
    }

    if (wcslen(selectedPath) >= pathSize) return FALSE;
    return wcscpy_s(path, pathSize, selectedPath) == 0;
}
