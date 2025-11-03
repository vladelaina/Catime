/**
 * @file font_path_manager.c
 * @brief Font path resolution and search implementation
 */

#include "../../include/font/font_path_manager.h"
#include "../../include/font/font_ttf_parser.h"
#include "../../include/utils/string_convert.h"
#include "../../include/utils/path_utils.h"
#include "../../include/config.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

/* External references (from font_manager.c) */
extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];

/* ============================================================================
 * Fonts Folder Resolution
 * ============================================================================ */

BOOL GetFontsFolderW(wchar_t* outW, size_t size, BOOL ensureCreate) {
    if (!outW || size == 0) return FALSE;
    
    /* Get config path */
    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);
    
    wchar_t configPathW[MAX_PATH] = {0};
    if (!Utf8ToWide(configPathUtf8, configPathW, MAX_PATH)) return FALSE;
    
    /* Extract directory (remove config.ini) */
    wchar_t* lastSep = wcsrchr(configPathW, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - configPathW);
    if (dirLen + 1 >= size) return FALSE;
    
    wcsncpy(outW, configPathW, dirLen);
    outW[dirLen] = L'\0';
    
    /* Append fonts subfolder path */
    if (wcslen(outW) + 1 + wcslen(L"resources\\fonts") + 1 >= size) return FALSE;
    wcscat(outW, L"\\resources\\fonts");
    
    /* Create directory if requested */
    if (ensureCreate) {
        SHCreateDirectoryExW(NULL, outW, NULL);
    }
    
    return TRUE;
}

/* ============================================================================
 * Path Building
 * ============================================================================ */

BOOL BuildFontConfigPath(const char* relativePath, char* outBuffer, size_t bufferSize) {
    if (!relativePath || !outBuffer || bufferSize == 0) return FALSE;
    
    int result = snprintf(outBuffer, bufferSize, "%s%s", FONT_FOLDER_PREFIX, relativePath);
    return result > 0 && result < (int)bufferSize;
}

BOOL BuildFullFontPath(const char* relativePath, char* outAbsolutePath, size_t bufferSize) {
    if (!relativePath || !outAbsolutePath || bufferSize == 0) return FALSE;
    
    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderW(fontsFolderW, MAX_PATH, TRUE)) return FALSE;
    
    wchar_t relativeW[MAX_PATH] = {0};
    if (!Utf8ToWide(relativePath, relativeW, MAX_PATH)) return FALSE;
    
    wchar_t fullW[MAX_PATH] = {0};
    _snwprintf_s(fullW, MAX_PATH, _TRUNCATE, L"%s\\%s", fontsFolderW, relativeW);
    
    return WideToUtf8(fullW, outAbsolutePath, bufferSize);
}

BOOL CalculateRelativePath(const char* absolutePath, char* outRelativePath, size_t bufferSize) {
    if (!absolutePath || !outRelativePath || bufferSize == 0) return FALSE;
    
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
    strncpy(outRelativePath, relativePart, bufferSize - 1);
    outRelativePath[bufferSize - 1] = '\0';
    
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
                                 wchar_t* resultPathW, size_t resultCapacity) {
    if (!folderPathW || !targetFileW || !resultPathW) return FALSE;
    
    wchar_t searchPathW[MAX_PATH] = {0};
    _snwprintf_s(searchPathW, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
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
        
        wchar_t fullItemPathW[MAX_PATH] = {0};
        _snwprintf_s(fullItemPathW, MAX_PATH, _TRUNCATE, L"%s\\%s", 
                    folderPathW, findDataW.cFileName);
        
        if (!(findDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            /* File: check if name matches */
            if (_wcsicmp(findDataW.cFileName, targetFileW) == 0) {
                wcsncpy(resultPathW, fullItemPathW, resultCapacity - 1);
                resultPathW[resultCapacity - 1] = L'\0';
                found = TRUE;
                break;
            }
        } else {
            /* Directory: recurse */
            if (SearchFontRecursiveW(fullItemPathW, targetFileW, resultPathW, resultCapacity)) {
                found = TRUE;
                break;
            }
        }
    } while (FindNextFileW(hFind, &findDataW));
    
    FindClose(hFind);
    return found;
}

BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize) {
    if (!fontFileName || !foundPath || foundPathSize == 0) return FALSE;
    
    /* Get fonts folder */
    wchar_t fontsFolderW[MAX_PATH] = {0};
    if (!GetFontsFolderW(fontsFolderW, MAX_PATH, TRUE)) return FALSE;
    
    /* Convert filename to wide */
    wchar_t targetFileW[MAX_PATH] = {0};
    if (!Utf8ToWide(fontFileName, targetFileW, MAX_PATH)) return FALSE;
    
    /* Search recursively */
    wchar_t resultPathW[MAX_PATH] = {0};
    if (!SearchFontRecursiveW(fontsFolderW, targetFileW, resultPathW, MAX_PATH)) {
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
    strncpy(pathInfo->fileName, GetFileNameU8(fontFileName), sizeof(pathInfo->fileName) - 1);
    
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
    
    /* Update global FONT_FILE_NAME */
    strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    /* Update internal name from TTF */
    GetFontNameFromFile(pathInfo.absolutePath, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    
    /* Write to config */
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString("Display", "FONT_FILE_NAME", pathInfo.configPath, config_path);
    
    return TRUE;
}

