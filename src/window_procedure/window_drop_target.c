/**
 * @file window_drop_target.c
 * @brief Handles drag and drop operations for resource import
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <shlwapi.h>
#include "window_procedure/window_drop_target.h"
#include "config.h"
#include "log.h"
#include "font.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_procedure.h"

#pragma comment(lib, "shlwapi.lib")

extern char FONT_FILE_NAME[MAX_PATH];
extern char CLOCK_STARTUP_MODE[20];

/**
 * @brief Check file extension
 */
static BOOL HasExtension(const wchar_t* filename, const wchar_t* ext) {
    const wchar_t* dot = wcsrchr(filename, L'.');
    if (!dot) return FALSE;
    return _wcsicmp(dot, ext) == 0;
}

/**
 * @brief Determine resource type from file extension
 */
static ResourceType GetResourceType(const wchar_t* filename) {
    if (HasExtension(filename, L".ttf") || 
        HasExtension(filename, L".otf") || 
        HasExtension(filename, L".ttc")) {
        return RESOURCE_TYPE_FONT;
    }
    
    if (HasExtension(filename, L".gif") || 
        HasExtension(filename, L".webp") ||
        HasExtension(filename, L".png") || 
        HasExtension(filename, L".jpg") || 
        HasExtension(filename, L".jpeg") || 
        HasExtension(filename, L".bmp") || 
        HasExtension(filename, L".ico") ||
        HasExtension(filename, L".tif") || 
        HasExtension(filename, L".tiff")) {
        return RESOURCE_TYPE_ANIMATION;
    }
    
    return RESOURCE_TYPE_UNKNOWN;
}

/**
 * @brief Get target folder path based on resource type
 */
static BOOL GetTargetFolderPath(ResourceType type, wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    
    if (type == RESOURCE_TYPE_FONT) {
        _snwprintf_s(outPath, size, _TRUNCATE, L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath);
        return TRUE;
    } else if (type == RESOURCE_TYPE_ANIMATION) {
        _snwprintf_s(outPath, size, _TRUNCATE, L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath);
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Move file to target directory and return new filename
 * @param relativeDir Optional relative directory structure to preserve (can be NULL)
 */
static BOOL MoveResourceFile(const wchar_t* srcPath, ResourceType type, const wchar_t* relativeDir, wchar_t* outNewPath, size_t size) {
    wchar_t targetDir[MAX_PATH];
    if (!GetTargetFolderPath(type, targetDir, MAX_PATH)) return FALSE;
    
    /* Append relative directory if present */
    if (relativeDir && *relativeDir) {
        wcscat_s(targetDir, MAX_PATH, L"\\");
        wcscat_s(targetDir, MAX_PATH, relativeDir);
    }
    
    /* Ensure directory exists */
    SHCreateDirectoryExW(NULL, targetDir, NULL);
    
    /* Extract filename */
    const wchar_t* fileName = wcsrchr(srcPath, L'\\');
    if (fileName) fileName++;
    else fileName = srcPath;
    
    /* Build destination path */
    _snwprintf_s(outNewPath, size, _TRUNCATE, L"%s\\%s", targetDir, fileName);
    
    /* Check if source and dest are the same */
    if (_wcsicmp(srcPath, outNewPath) == 0) {
        return TRUE; /* Already in place */
    }
    
    /* Move file (replace if exists) */
    if (MoveFileExW(srcPath, outNewPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        LOG_INFO("Moved resource file: %ls -> %ls", srcPath, outNewPath);
        return TRUE;
    }
    
    LOG_ERROR("Failed to move file. Error: %lu", GetLastError());
    return FALSE;
}

static void ProcessDirectoryRecursive(const wchar_t* dirPath, const wchar_t* rootDropPath, 
                                    wchar_t* lastFontPath, wchar_t* lastAnimPath,
                                    int* fontCount, int* animCount, int* movedCount) {
    WIN32_FIND_DATAW findData;
    wchar_t searchPath[MAX_PATH];
    
    swprintf_s(searchPath, MAX_PATH, L"%s\\*", dirPath);
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        wchar_t fullPath[MAX_PATH];
        swprintf_s(fullPath, MAX_PATH, L"%s\\%s", dirPath, findData.cFileName);
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ProcessDirectoryRecursive(fullPath, rootDropPath, lastFontPath, lastAnimPath, fontCount, animCount, movedCount);
        } else {
            ResourceType type = GetResourceType(fullPath);
            if (type != RESOURCE_TYPE_UNKNOWN) {
                /* Calculate relative path from root drop path to maintain structure */
                wchar_t relativeDir[MAX_PATH] = {0};
                
                /* Get parent directory of the dropped root to calculate relative path including the dropped folder name */
                size_t rootLen = wcslen(rootDropPath);
                
                /* Check if fullPath starts with rootDropPath */
                if (_wcsnicmp(fullPath, rootDropPath, rootLen) == 0) {
                    /* Extract folder name from rootDropPath */
                    const wchar_t* folderName = wcsrchr(rootDropPath, L'\\');
                    folderName = folderName ? folderName + 1 : rootDropPath;
                    
                    wchar_t subPath[MAX_PATH] = {0};
                    if (fullPath[rootLen] == L'\\') {
                        wcscpy_s(subPath, MAX_PATH, fullPath + rootLen + 1);
                    }
                    
                    /* Remove filename from subPath to get directory */
                    wchar_t* lastSlash = wcsrchr(subPath, L'\\');
                    if (lastSlash) *lastSlash = L'\0';
                    else subPath[0] = L'\0'; // File is directly in dropped folder
                    
                    /* Construct final relative dir: "FolderName\SubPath" */
                    swprintf_s(relativeDir, MAX_PATH, L"%s", folderName);
                    if (subPath[0]) {
                        wcscat_s(relativeDir, MAX_PATH, L"\\");
                        wcscat_s(relativeDir, MAX_PATH, subPath);
                    }
                }

                wchar_t newPath[MAX_PATH];
                if (MoveResourceFile(fullPath, type, relativeDir, newPath, MAX_PATH)) {
                    (*movedCount)++;
                    if (type == RESOURCE_TYPE_FONT) {
                        wcscpy_s(lastFontPath, MAX_PATH, newPath);
                        (*fontCount)++;
                    } else if (type == RESOURCE_TYPE_ANIMATION) {
                        wcscpy_s(lastAnimPath, MAX_PATH, newPath);
                        (*animCount)++;
                    }
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

/**
 * @brief Handle dropped files
 * Supports:
 * - Single files (Font/Anim)
 * - Multiple files
 * - Directories (Recursive scan)
 * - Mixed content (Files + Dirs)
 * - Preserves directory structure for dropped folders
 */
void HandleDropFiles(HWND hwnd, HDROP hDrop) {
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    
    if (fileCount == 0) {
        DragFinish(hDrop);
        return;
    }

    wchar_t lastFontPath[MAX_PATH] = {0};
    wchar_t lastAnimPath[MAX_PATH] = {0};
    int fontCount = 0;
    int animCount = 0;
    int movedCount = 0;
    
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t filePath[MAX_PATH];
        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
            DWORD attrs = GetFileAttributesW(filePath);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                /* It's a directory - process recursively to find all resources */
                ProcessDirectoryRecursive(filePath, filePath, lastFontPath, lastAnimPath, &fontCount, &animCount, &movedCount);
            } else {
                /* It's a file */
                ResourceType type = GetResourceType(filePath);
                if (type != RESOURCE_TYPE_UNKNOWN) {
                    wchar_t newPath[MAX_PATH];
                    /* NULL relativeDir means put in root of resources/type */
                    if (MoveResourceFile(filePath, type, NULL, newPath, MAX_PATH)) {
                        movedCount++;
                        if (type == RESOURCE_TYPE_FONT) {
                            wcscpy_s(lastFontPath, MAX_PATH, newPath);
                            fontCount++;
                        } else if (type == RESOURCE_TYPE_ANIMATION) {
                            wcscpy_s(lastAnimPath, MAX_PATH, newPath);
                            animCount++;
                        }
                    }
                }
            }
        }
    }
    
    DragFinish(hDrop);
    
    /* Smart Auto-Apply Logic */
    if (fontCount == 1) {
        wchar_t* fileNameW = wcsrchr(lastFontPath, L'\\');
        if (fileNameW) fileNameW++;
        else fileNameW = lastFontPath;
        
        char fileNameA[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, fileNameW, -1, fileNameA, MAX_PATH, NULL, NULL);
        
        char configValue[MAX_PATH];
        
        /* Re-calculate relative path from fonts/animations dir for the config */
        char configPath[MAX_PATH];
        GetConfigPath(configPath, MAX_PATH);
        wchar_t wRoot[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wRoot, MAX_PATH);
        wchar_t* p = wcsrchr(wRoot, L'\\');
        if (p) *p = L'\0';
        
        /* Construct fonts root: .../resources/fonts */
        wchar_t fontsRoot[MAX_PATH];
        swprintf_s(fontsRoot, MAX_PATH, L"%s\\resources\\fonts", wRoot);
        
        /* Check if lastFontPath starts with fontsRoot */
        size_t rootLen = wcslen(fontsRoot);
        char relPathA[MAX_PATH] = {0};
        
        if (_wcsnicmp(lastFontPath, fontsRoot, rootLen) == 0) {
             /* Skip root and leading slash */
             wchar_t* relPathW = lastFontPath + rootLen;
             if (*relPathW == L'\\') relPathW++;
             WideCharToMultiByte(CP_UTF8, 0, relPathW, -1, relPathA, MAX_PATH, NULL, NULL);
        } else {
             /* Fallback */
             strncpy(relPathA, fileNameA, MAX_PATH);
        }

        snprintf(configValue, sizeof(configValue), "%s%s", FONTS_PATH_PREFIX, relPathA);
        
        SwitchFont(GetModuleHandle(NULL), configValue);
        InvalidateRect(hwnd, NULL, TRUE);
        LOG_INFO("Auto-applied font: %s", relPathA);
    }
    
    if (animCount == 1) {
        /* Same logic for animations */
        char configPath[MAX_PATH];
        GetConfigPath(configPath, MAX_PATH);
        wchar_t wRoot[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wRoot, MAX_PATH);
        wchar_t* p = wcsrchr(wRoot, L'\\');
        if (p) *p = L'\0';
        
        wchar_t animsRoot[MAX_PATH];
        swprintf_s(animsRoot, MAX_PATH, L"%s\\resources\\animations", wRoot);
        
        size_t rootLen = wcslen(animsRoot);
        char relPathA[MAX_PATH] = {0};
        
        if (_wcsnicmp(lastAnimPath, animsRoot, rootLen) == 0) {
             wchar_t* relPathW = lastAnimPath + rootLen;
             if (*relPathW == L'\\') relPathW++;
             WideCharToMultiByte(CP_UTF8, 0, relPathW, -1, relPathA, MAX_PATH, NULL, NULL);
        } else {
             wchar_t* fileNameW = wcsrchr(lastAnimPath, L'\\');
             if (fileNameW) fileNameW++; else fileNameW = lastAnimPath;
             WideCharToMultiByte(CP_UTF8, 0, fileNameW, -1, relPathA, MAX_PATH, NULL, NULL);
        }
        
        SetCurrentAnimationName(relPathA);
        LOG_INFO("Auto-applied animation: %s", relPathA);
    }
    
    if (movedCount > 0) {
        LOG_INFO("Imported %d resources (Font: %d, Anim: %d)", movedCount, fontCount, animCount);
    }
}
