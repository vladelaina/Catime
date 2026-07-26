/**
 * @file config_plugin_security_paths.c
 * @brief Plugin trust path normalization and validation helpers
 */
#include "config_plugin_security_internal.h"

#include "log.h"
#include "utils/string_convert.h"
#include "utils/string_safe.h"

#include <string.h>
#include <wchar.h>

int PluginTrust_ClampCount(int count) {
    if (count < 0) return 0;
    if (count > MAX_TRUSTED_PLUGINS) return MAX_TRUSTED_PLUGINS;
    return count;
}

BOOL PluginTrust_IsValidHash(const char* hash) {
    if (!hash || strlen(hash) != 64) return FALSE;
    for (int i = 0; i < 64; ++i) {
        char value = hash[i];
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f') ||
              (value >= 'A' && value <= 'F'))) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL PluginTrust_PathsEqual(const char* first, const char* second) {
    wchar_t firstWide[MAX_PATH];
    wchar_t secondWide[MAX_PATH];

    if (!first || !second) return FALSE;
    if (Utf8ToWide(first, firstWide, _countof(firstWide)) &&
        Utf8ToWide(second, secondWide, _countof(secondWide))) {
        return _wcsicmp(firstWide, secondWide) == 0;
    }
    return _stricmp(first, second) == 0;
}

static BOOL IsPathBelow(const wchar_t* path, const wchar_t* directory,
                        size_t directoryLength) {
    wchar_t boundary;

    if (_wcsnicmp(path, directory, directoryLength) != 0) return FALSE;
    boundary = path[directoryLength];
    return boundary == L'\0' || boundary == L'\\' || boundary == L'/';
}

BOOL PluginTrust_EncodePath(const char* fullPath, char* storedPath,
                            size_t storedPathSize) {
    wchar_t fullPathWide[MAX_PATH];
    wchar_t localAppDataWide[MAX_PATH];
    char localAppData[MAX_PATH] = {0};

    if (!fullPath || !storedPath || storedPathSize == 0) return FALSE;
    storedPath[0] = '\0';
    if (Utf8ToWide(fullPath, fullPathWide, _countof(fullPathWide)) &&
        GetEffectiveLocalAppDataPath(localAppData, sizeof(localAppData)) &&
        Utf8ToWide(localAppData, localAppDataWide,
                   _countof(localAppDataWide))) {
        size_t prefixLength = wcslen(localAppDataWide);
        if (IsPathBelow(fullPathWide, localAppDataWide, prefixLength)) {
            wchar_t compressedWide[MAX_PATH];
            int written = _snwprintf_s(compressedWide,
                                       _countof(compressedWide), _TRUNCATE,
                                       L"%%LOCALAPPDATA%%%ls",
                                       fullPathWide + prefixLength);
            if (written >= 0 &&
                WideToUtf8(compressedWide, storedPath, storedPathSize)) {
                return TRUE;
            }
            storedPath[0] = '\0';
        }
    }

    if (strlen(fullPath) >= storedPathSize) {
        LOG_ERROR("Plugin trust path too long to store: %s", fullPath);
        return FALSE;
    }
    safe_strncpy(storedPath, fullPath, storedPathSize);
    return TRUE;
}

static void CopyPathFallback(const char* source, char* destination,
                             size_t destinationSize) {
    if (!destination || destinationSize == 0) return;
    safe_strncpy(destination, source ? source : "", destinationSize);
}

void PluginTrust_ExpandPath(const char* storedPath, char* expandedPath,
                            size_t expandedPathSize) {
    wchar_t storedWide[MAX_PATH];
    wchar_t expandedWide[MAX_PATH];
    DWORD result;

    if (!storedPath || !expandedPath || expandedPathSize == 0) return;
    expandedPath[0] = '\0';
    if (_strnicmp(storedPath, "%LOCALAPPDATA%",
                  strlen("%LOCALAPPDATA%")) == 0) {
        if (!ExpandEffectiveLocalAppDataPath(storedPath, expandedPath,
                                             expandedPathSize)) {
            CopyPathFallback(storedPath, expandedPath, expandedPathSize);
        }
        return;
    }
    if (!Utf8ToWide(storedPath, storedWide, _countof(storedWide))) {
        CopyPathFallback(storedPath, expandedPath, expandedPathSize);
        return;
    }

    result = ExpandEnvironmentStringsW(storedWide, expandedWide,
                                       _countof(expandedWide));
    if (result == 0 || result > _countof(expandedWide) ||
        !WideToUtf8(expandedWide, expandedPath, expandedPathSize)) {
        if (result == 0) {
            LOG_ERROR("Failed to expand plugin trust path (error=%lu)",
                      GetLastError());
        }
        CopyPathFallback(storedPath, expandedPath, expandedPathSize);
    }
}
