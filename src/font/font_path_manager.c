/**
 * @file font_path_manager.c
 * @brief Font path resolution and search implementation
 */

#include "font/font_path_manager.h"
#include "font/font_config.h"
#include "font/font_ttf_parser.h"
#include "utils/string_convert.h"
#include "utils/path_utils.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

/* External references (from font_manager.c) */
extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];

static SRWLOCK g_fontsFolderCacheLock = SRWLOCK_INIT;
static wchar_t g_cachedFontsFolderW[MAX_PATH] = {0};
static BOOL g_cachedFontsFolderValid = FALSE;
static BOOL g_fontsFolderCreateAttempted = FALSE;

#define FONT_SEARCH_ENTRY_BUDGET 4096u
#define FONT_SEARCH_DEPTH_LIMIT 16u

typedef struct {
    DWORD scannedEntries;
    BOOL truncated;
} FontSearchState;

static BOOL CopyStringExactA(const char* src, char* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = '\0';
    if (!src) return FALSE;

    size_t len = strlen(src);
    if (len >= outSize) return FALSE;

    memcpy(out, src, len + 1);
    return TRUE;
}

/* ============================================================================
 * Fonts Folder Resolution
 * ============================================================================ */

static BOOL BuildFontsFolderPathW(wchar_t* outW, size_t size) {
    if (!outW || size == 0) return FALSE;
    outW[0] = L'\0';

    /* Get config path */
    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);
    
    wchar_t configPathW[MAX_PATH] = {0};
    if (!Utf8ToWide(configPathUtf8, configPathW, MAX_PATH)) return FALSE;
    
    /* Extract directory (remove config.ini) */
    const wchar_t* lastSep = wcsrchr(configPathW, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - configPathW);
    if (dirLen + 1 >= size) return FALSE;
    
    wcsncpy(outW, configPathW, dirLen);
    outW[dirLen] = L'\0';
    
    /* Append fonts subfolder path */
    if (wcslen(outW) + 1 + wcslen(L"resources\\fonts") + 1 >= size) return FALSE;
    wcscat_s(outW, size, L"\\resources\\fonts");
    return TRUE;
}

BOOL GetFontsFolderW(wchar_t* outW, size_t size, BOOL ensureCreate) {
    if (!outW || size == 0) return FALSE;
    outW[0] = L'\0';

    AcquireSRWLockExclusive(&g_fontsFolderCacheLock);

    if (!g_cachedFontsFolderValid) {
        if (!BuildFontsFolderPathW(g_cachedFontsFolderW, MAX_PATH)) {
            ReleaseSRWLockExclusive(&g_fontsFolderCacheLock);
            return FALSE;
        }
        g_cachedFontsFolderValid = TRUE;
        g_fontsFolderCreateAttempted = FALSE;
    }

    if (wcslen(g_cachedFontsFolderW) + 1 > size) {
        ReleaseSRWLockExclusive(&g_fontsFolderCacheLock);
        return FALSE;
    }

    wcscpy_s(outW, size, g_cachedFontsFolderW);

    if (ensureCreate && !g_fontsFolderCreateAttempted) {
        int createResult = SHCreateDirectoryExW(NULL, g_cachedFontsFolderW, NULL);
        if (createResult != ERROR_SUCCESS && createResult != ERROR_ALREADY_EXISTS) {
            LOG_WARNING("Failed to create fonts folder: %ls (error=%d)",
                        g_cachedFontsFolderW, createResult);
            ReleaseSRWLockExclusive(&g_fontsFolderCacheLock);
            return FALSE;
        }
        g_fontsFolderCreateAttempted = TRUE;
    }

    ReleaseSRWLockExclusive(&g_fontsFolderCacheLock);
    return TRUE;
}

/* ============================================================================
 * Path Building
 * ============================================================================ */

static BOOL IsSafeRelativeFontPathUtf8(const char* path);

BOOL BuildFontConfigPath(const char* relativePath, char* outBuffer, size_t bufferSize) {
    if (!relativePath || !outBuffer || bufferSize == 0) return FALSE;
    outBuffer[0] = '\0';
    if (!IsSafeRelativeFontPathUtf8(relativePath)) return FALSE;

    int result = snprintf(outBuffer, bufferSize, "%s%s", FONT_FOLDER_PREFIX, relativePath);
    if (result <= 0 || result >= (int)bufferSize) {
        outBuffer[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL IsAbsoluteFontPathUtf8(const char* path) {
    if (!path || !*path) return FALSE;

    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return TRUE;
    }

    return path[0] == '\\' && path[1] == '\\';
}

static BOOL IsSafeRelativeFontPathUtf8(const char* path) {
    if (!path || !*path) return FALSE;
    if (path[0] == '\\' || path[0] == '/') return FALSE;

    const char* segment = path;
    while (*segment) {
        const char* end = segment;
        while (*end && *end != '\\' && *end != '/') {
            end++;
        }

        size_t len = (size_t)(end - segment);
        if (len == 0 ||
            (len == 1 && segment[0] == '.') ||
            (len == 2 && segment[0] == '.' && segment[1] == '.')) {
            return FALSE;
        }

        segment = *end ? end + 1 : end;
    }

    return TRUE;
}

BOOL BuildFullFontPath(const char* relativePath, char* outAbsolutePath, size_t bufferSize) {
    if (!relativePath || !outAbsolutePath || bufferSize == 0) return FALSE;
    outAbsolutePath[0] = '\0';

    /* Check if path is already absolute (contains drive letter or starts with UNC) */
    if (IsAbsoluteFontPathUtf8(relativePath)) {
        if (strlen(relativePath) >= bufferSize) return FALSE;
        strncpy(outAbsolutePath, relativePath, bufferSize - 1);
        outAbsolutePath[bufferSize - 1] = '\0';
        return TRUE;
    }

    if (!IsSafeRelativeFontPathUtf8(relativePath)) return FALSE;

    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderW(fontsFolderW, MAX_PATH, TRUE)) return FALSE;

    wchar_t relativeW[MAX_PATH] = {0};
    if (!Utf8ToWide(relativePath, relativeW, MAX_PATH)) return FALSE;

    wchar_t fullW[MAX_PATH] = {0};
    int written = _snwprintf_s(fullW, MAX_PATH, _TRUNCATE, L"%s\\%s", fontsFolderW, relativeW);
    if (written < 0) return FALSE;

    return WideToUtf8(fullW, outAbsolutePath, bufferSize);
}

BOOL CalculateRelativePath(const char* absolutePath, char* outRelativePath, size_t bufferSize) {
    if (!absolutePath || !outRelativePath || bufferSize == 0) return FALSE;
    outRelativePath[0] = '\0';
    
    /* Get fonts folder */
    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderW(fontsFolderW, MAX_PATH, TRUE)) return FALSE;
    
    char fontsFolderUtf8[MAX_PATH];
    if (!WideToUtf8(fontsFolderW, fontsFolderUtf8, MAX_PATH)) return FALSE;
    
    /* Ensure trailing separator */
    size_t prefixLen = strlen(fontsFolderUtf8);
    if (prefixLen > 0 && fontsFolderUtf8[prefixLen - 1] != '\\') {
        if (prefixLen + 1 < MAX_PATH) {
            fontsFolderUtf8[prefixLen] = '\\';
            fontsFolderUtf8[prefixLen + 1] = '\0';
            prefixLen += 1;
        }
    }
    
    /* Check if absolute path starts with fonts folder */
    if (_strnicmp(absolutePath, fontsFolderUtf8, prefixLen) != 0) {
        return FALSE;
    }
    
    /* Extract relative portion */
    const char* relativePart = absolutePath + prefixLen;
    if (!CopyStringExactA(relativePart, outRelativePath, bufferSize)) {
        LOG_WARNING("Font relative path too long: %s", relativePart);
        return FALSE;
    }
    if (!IsSafeRelativeFontPathUtf8(outRelativePath)) {
        outRelativePath[0] = '\0';
        return FALSE;
    }

    return TRUE;
}

/* ============================================================================
 * Path Validation
 * ============================================================================ */

BOOL IsFontsFolderPath(const char* path) {
    if (!path) return FALSE;
    return _strnicmp(path, FONT_FOLDER_PREFIX, strlen(FONT_FOLDER_PREFIX)) == 0;
}

const char* ExtractRelativePath(const char* fullConfigPath) {
    if (!IsFontsFolderPath(fullConfigPath)) return NULL;
    return fullConfigPath + strlen(FONT_FOLDER_PREFIX);
}

/* ============================================================================
 * Recursive Font Search
 * ============================================================================ */

/**
 * @brief Recursively search for font file (wide character version)
 * @param folderPathW Folder to search
 * @param targetFileW Target filename
 * @param resultPathW Output buffer for full path
 * @param resultCapacity Buffer size in wide characters
 * @return TRUE on first match
 */
static BOOL SearchFontRecursiveW(const wchar_t* folderPathW, const wchar_t* targetFileW,
                                 wchar_t* resultPathW, size_t resultCapacity,
                                 FontSearchState* state, unsigned depth) {
    if (!folderPathW || !targetFileW || !resultPathW || !state || state->truncated) return FALSE;
    if (depth >= FONT_SEARCH_DEPTH_LIMIT) {
        state->truncated = TRUE;
        return FALSE;
    }

    wchar_t searchPathW[MAX_PATH] = {0};
    int searchWritten = _snwprintf_s(searchPathW, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    if (searchWritten < 0) return FALSE;
    
    WIN32_FIND_DATAW findDataW;
    HANDLE hFind = FindFirstFileW(searchPathW, &findDataW);
    if (hFind == INVALID_HANDLE_VALUE) return FALSE;
    
    BOOL found = FALSE;
    
    do {
        /* Skip . and .. */
        if (wcscmp(findDataW.cFileName, L".") == 0 || 
            wcscmp(findDataW.cFileName, L"..") == 0) {
            continue;
        }

        if (state->scannedEntries >= FONT_SEARCH_ENTRY_BUDGET) {
            state->truncated = TRUE;
            break;
        }
        state->scannedEntries++;

        wchar_t fullItemPathW[MAX_PATH] = {0};
        int itemWritten = _snwprintf_s(fullItemPathW, MAX_PATH, _TRUNCATE, L"%s\\%s",
                                       folderPathW, findDataW.cFileName);
        if (itemWritten < 0) continue;
        
        if (!(findDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            /* File: check if name matches */
            if (_wcsicmp(findDataW.cFileName, targetFileW) == 0) {
                wcsncpy(resultPathW, fullItemPathW, resultCapacity - 1);
                resultPathW[resultCapacity - 1] = L'\0';
                found = TRUE;
                break;
            }
        } else if (!(findDataW.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            /* Directory: recurse */
            if (SearchFontRecursiveW(fullItemPathW, targetFileW, resultPathW,
                                     resultCapacity, state, depth + 1)) {
                found = TRUE;
                break;
            }
        }
    } while (!state->truncated && FindNextFileW(hFind, &findDataW));
    
    FindClose(hFind);
    return found;
}

BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize) {
    if (!fontFileName || !foundPath || foundPathSize == 0) return FALSE;
    foundPath[0] = '\0';
    
    /* Get fonts folder */
    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderW(fontsFolderW, MAX_PATH, TRUE)) return FALSE;
    
    /* Convert filename to wide */
    wchar_t targetFileW[MAX_PATH] = {0};
    if (!Utf8ToWide(fontFileName, targetFileW, MAX_PATH)) return FALSE;
    
    /* Search recursively */
    wchar_t resultPathW[MAX_PATH] = {0};
    FontSearchState searchState = {0};
    if (!SearchFontRecursiveW(fontsFolderW, targetFileW, resultPathW, MAX_PATH,
                              &searchState, 0)) {
        return FALSE;
    }
    
    /* Convert result back to UTF-8 */
    return WideToUtf8(resultPathW, foundPath, foundPathSize);
}

/* ============================================================================
 * Auto-Recovery
 * ============================================================================ */

BOOL AutoFixFontPath(const char* fontFileName, FontPathInfo* pathInfo) {
    if (!fontFileName || !pathInfo) return FALSE;
    
    /* Initialize output structure */
    memset(pathInfo, 0, sizeof(FontPathInfo));

    /* Extract filename only (strip any directory prefix) */
    if (!CopyStringExactA(GetFileNameU8(fontFileName), pathInfo->fileName,
                          sizeof(pathInfo->fileName))) {
        LOG_WARNING("Font file name too long for auto-fix: %s", fontFileName);
        return FALSE;
    }
    
    /* Search for font in fonts folder */
    if (!FindFontInFontsFolder(pathInfo->fileName, pathInfo->absolutePath, 
                               sizeof(pathInfo->absolutePath))) {
        return FALSE;
    }
    
    /* Calculate relative path */
    if (!CalculateRelativePath(pathInfo->absolutePath, pathInfo->relativePath, 
                               sizeof(pathInfo->relativePath))) {
        return FALSE;
    }
    
    /* Build config path */
    if (!BuildFontConfigPath(pathInfo->relativePath, pathInfo->configPath, 
                            sizeof(pathInfo->configPath))) {
        return FALSE;
    }
    
    pathInfo->isValid = TRUE;
    return TRUE;
}

BOOL CheckAndFixFontPath(void) {
    /* Only check fonts folder paths */
    if (!IsFontsFolderPath(FONT_FILE_NAME)) {
        return FALSE;
    }
    
    /* Extract relative path */
    const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
    if (!relativePath) return FALSE;
    
    /* Build full path and check if exists */
    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(relativePath, fontPath, MAX_PATH)) return FALSE;
    
    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontPath, wFontPath, MAX_PATH)) return FALSE;
    
    /* If file exists, no fix needed */
    if (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    /* File missing: try auto-fix */
    FontPathInfo pathInfo;
    if (!AutoFixFontPath(relativePath, &pathInfo)) {
        return FALSE;
    }

    char previousFontName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    CopyStringExactA(FONT_INTERNAL_NAME, previousInternalName, sizeof(previousInternalName));

    /* Update global FONT_FILE_NAME */
    strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';

    /* Update internal name from TTF */
    if (!GetFontNameFromFile(pathInfo.absolutePath, FONT_INTERNAL_NAME,
                             sizeof(FONT_INTERNAL_NAME))) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }

    /* Write to config */
    if (!WriteConfigFont(pathInfo.configPath, FALSE)) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }

    return TRUE;
}

