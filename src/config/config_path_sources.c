#include "config_path_internal.h"
#include "config.h"
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static BOOL EnsureDirectoryExistsW(const wchar_t* path) {
    if (!path || !*path) return FALSE;

    if (CreateDirectoryW(path, NULL)) {
        return TRUE;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL ConfigPath_IsDirectoryCreateResultOk(int result, const wchar_t* path) {
    if (result == ERROR_SUCCESS) {
        return TRUE;
    }

    if (result != ERROR_ALREADY_EXISTS && result != ERROR_FILE_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL ConfigPath_BuildFromLocalAppData(const wchar_t* root,
                                      char* output,
                                      size_t outputSize) {
    if (!output || outputSize == 0) {
        return FALSE;
    }
    output[0] = '\0';
    if (!root || !*root || outputSize > INT_MAX) {
        return FALSE;
    }

    wchar_t wDir[MAX_PATH] = {0};
    if (_snwprintf_s(wDir, MAX_PATH, _TRUNCATE, L"%s\\Catime", root) < 0 ||
        !EnsureDirectoryExistsW(wDir)) {
        return FALSE;
    }

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (_snwprintf_s(wConfigPath, MAX_PATH, _TRUNCATE,
                     L"%s\\Catime\\config.ini", root) < 0) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wConfigPath, -1,
                            output, (int)outputSize, NULL, NULL) <= 0) {
        output[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL ConfigPath_ResolveCiRootW(wchar_t* output, size_t outputSize) {
    if (!output || outputSize == 0 || outputSize > INT_MAX) return FALSE;
    output[0] = L'\0';

    const wchar_t* commandLine = GetCommandLineW();
    if (!commandLine) return FALSE;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(commandLine, &argc);
    if (!argv) return FALSE;

    BOOL ciSmoke = FALSE;
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--ci-smoke") == 0) {
            ciSmoke = TRUE;
            break;
        }
    }
    if (!ciSmoke) {
        LocalFree(argv);
        return FALSE;
    }

    BOOL resolved = FALSE;
    static const wchar_t prefix[] = L"--ci-config-dir=";
    for (int i = 1; i < argc; i++) {
        if (wcsncmp(argv[i], prefix, _countof(prefix) - 1) != 0 ||
            argv[i][(_countof(prefix) - 1)] == L'\0') {
            continue;
        }

        wchar_t fullPath[MAX_PATH] = {0};
        DWORD length = GetFullPathNameW(
            argv[i] + (_countof(prefix) - 1),
            _countof(fullPath), fullPath, NULL);
        if (length == 0 || length >= _countof(fullPath)) break;

        int createResult = SHCreateDirectoryExW(NULL, fullPath, NULL);
        if (createResult != ERROR_SUCCESS &&
            createResult != ERROR_ALREADY_EXISTS &&
            createResult != ERROR_FILE_EXISTS) {
            break;
        }

        DWORD attributes = GetFileAttributesW(fullPath);
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            wcslen(fullPath) >= outputSize) {
            break;
        }

        wcscpy_s(output, outputSize, fullPath);
        resolved = TRUE;
        break;
    }

    LocalFree(argv);
    return resolved;
}

BOOL ConfigPath_BuildFromUserProfile(char* output, size_t outputSize) {
    if (!output || outputSize == 0) {
        return FALSE;
    }
    output[0] = '\0';
    if (outputSize > INT_MAX) {
        return FALSE;
    }

    wchar_t wUserProfile[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", wUserProfile, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return FALSE;
    }

    wchar_t wDir[MAX_PATH] = {0};
    if (_snwprintf_s(wDir, MAX_PATH, _TRUNCATE,
                     L"%s\\AppData\\Local\\Catime", wUserProfile) < 0 ||
        !EnsureDirectoryExistsW(wDir)) {
        return FALSE;
    }

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (_snwprintf_s(wConfigPath, MAX_PATH, _TRUNCATE,
                     L"%s\\AppData\\Local\\Catime\\config.ini", wUserProfile) < 0) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wConfigPath, -1,
                            output, (int)outputSize, NULL, NULL) <= 0) {
        output[0] = '\0';
        return FALSE;
    }
    return TRUE;
}
