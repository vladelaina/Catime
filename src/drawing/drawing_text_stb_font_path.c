/**
 * @file drawing_text_stb_font_path.c
 * @brief Font-tag path resolution.
 */

#include "drawing_text_stb_internal.h"

BOOL ResolveFontTagPath(const wchar_t* fontPath, wchar_t* outPath, size_t pathSize) {
    if (!fontPath || !outPath || pathSize == 0) return FALSE;
    outPath[0] = L'\0';

    wchar_t expandedPath[MAX_PATH];
    wchar_t resolvedPath[MAX_PATH];

    /* Step 1: Expand environment variables if present. Package-aware
     * LocalAppData expansion keeps Store font tags inside the MSIX data root. */
    static const wchar_t localAppDataToken[] = L"%LOCALAPPDATA%";
    size_t localAppDataTokenLen = _countof(localAppDataToken) - 1;
    if (_wcsnicmp(fontPath, localAppDataToken, localAppDataTokenLen) == 0) {
        char fontPathUtf8[MAX_PATH] = {0};
        char expandedPathUtf8[MAX_PATH] = {0};
        if (!WideToUtf8(fontPath, fontPathUtf8, MAX_PATH) ||
            !ExpandEffectiveLocalAppDataPath(fontPathUtf8,
                                             expandedPathUtf8,
                                             sizeof(expandedPathUtf8)) ||
            !Utf8ToWide(expandedPathUtf8, expandedPath, MAX_PATH)) {
            LOG_WARNING("Failed to expand effective LocalAppData font path: %ls", fontPath);
            return FALSE;
        }
    } else if (wcschr(fontPath, L'%') != NULL) {
        DWORD result = ExpandEnvironmentStringsW(fontPath, expandedPath, MAX_PATH);
        if (result == 0 || result > MAX_PATH) {
            LOG_WARNING("Failed to expand environment variables in font path: %ls", fontPath);
            return FALSE;
        }
    } else {
        if (wcslen(fontPath) >= MAX_PATH) {
            LOG_WARNING("Font path is too long: %ls", fontPath);
            return FALSE;
        }
        wcsncpy(expandedPath, fontPath, MAX_PATH - 1);
        expandedPath[MAX_PATH - 1] = L'\0';
    }

    /* Step 2: Check if absolute path (has drive letter or UNC path) */
    BOOL isAbsolute = FALSE;
    if (wcslen(expandedPath) >= 2) {
        /* Check for drive letter (e.g., C:\) */
        if (expandedPath[1] == L':') {
            isAbsolute = TRUE;
        }
        /* Check for UNC path (e.g., \\server\share) */
        else if (expandedPath[0] == L'\\' && expandedPath[1] == L'\\') {
            isAbsolute = TRUE;
        }
    }

    if (isAbsolute) {
        if (wcslen(expandedPath) >= MAX_PATH) {
            LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
            return FALSE;
        }
        wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
        resolvedPath[MAX_PATH - 1] = L'\0';
    } else {
        /* Step 3: Resolve relative path against plugins directory */
        wchar_t pluginsDir[MAX_PATH];
        char pluginsDirUtf8[MAX_PATH] = {0};
        GetPluginsFolderPath(pluginsDirUtf8, MAX_PATH);
        if (pluginsDirUtf8[0] == '\0' ||
            MultiByteToWideChar(CP_UTF8, 0, pluginsDirUtf8, -1,
                                pluginsDir, MAX_PATH) <= 0) {
            /* Fallback: try current directory */
            if (wcslen(expandedPath) >= MAX_PATH) {
                LOG_WARNING("Font path is too long: %ls", expandedPath);
                return FALSE;
            }
            wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
            resolvedPath[MAX_PATH - 1] = L'\0';
        } else {
            int written = _snwprintf_s(resolvedPath, MAX_PATH, _TRUNCATE,
                                       L"%ls\\%ls",
                                       pluginsDir, expandedPath);
            if (written < 0) {
                LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
                return FALSE;
            }
        }
    }

    /* Step 4: Check if file exists */
    DWORD attrs = GetFileAttributesW(resolvedPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_WARNING("Font file not found: %ls", resolvedPath);
        return FALSE;
    }

    /* Step 5: Return wide path directly to avoid UTF-8 round trip before CreateFileW. */
    if (wcslen(resolvedPath) >= pathSize) {
        LOG_WARNING("Resolved font path is too long: %ls", resolvedPath);
        return FALSE;
    }
    wcsncpy(outPath, resolvedPath, pathSize - 1);
    outPath[pathSize - 1] = L'\0';

    return TRUE;
}
