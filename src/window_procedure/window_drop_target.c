/**
 * @file window_drop_target.c
 * @brief Handles drag and drop operations for resource import
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include "window_procedure/window_drop_target.h"
#include "config.h"
#include "log.h"
#include "font.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_procedure.h"

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
 */
static BOOL MoveResourceFile(const wchar_t* srcPath, ResourceType type, wchar_t* outNewPath, size_t size) {
    wchar_t targetDir[MAX_PATH];
    if (!GetTargetFolderPath(type, targetDir, MAX_PATH)) return FALSE;
    
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

/**
 * @brief Handle dropped files
 */
void HandleDropFiles(HWND hwnd, HDROP hDrop) {
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    
    ResourceType lastType = RESOURCE_TYPE_UNKNOWN;
    wchar_t lastMovedFile[MAX_PATH] = {0};
    int movedCount = 0;
    
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t filePath[MAX_PATH];
        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
            ResourceType type = GetResourceType(filePath);
            
            if (type != RESOURCE_TYPE_UNKNOWN) {
                wchar_t newPath[MAX_PATH];
                if (MoveResourceFile(filePath, type, newPath, MAX_PATH)) {
                    lastType = type;
                    wcscpy_s(lastMovedFile, MAX_PATH, newPath);
                    movedCount++;
                }
            }
        }
    }
    
    DragFinish(hDrop);
    
    /* Auto-apply if single file dropped */
    if (fileCount == 1 && movedCount == 1 && lastType != RESOURCE_TYPE_UNKNOWN) {
        if (lastType == RESOURCE_TYPE_FONT) {
            char utf8Path[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, lastMovedFile, -1, utf8Path, MAX_PATH, NULL, NULL);
            
            /* Convert absolute path to relative if possible (starts with %LOCALAPPDATA%) */
            /* But for simplicity, we can just store the path relative to fonts folder prefix */
            
            /* Extract filename */
            wchar_t* fileNameW = wcsrchr(lastMovedFile, L'\\');
            if (fileNameW) fileNameW++;
            else fileNameW = lastMovedFile;
            
            char fileNameA[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, fileNameW, -1, fileNameA, MAX_PATH, NULL, NULL);
            
            char configValue[MAX_PATH];
            snprintf(configValue, sizeof(configValue), "%s%s", FONTS_PATH_PREFIX, fileNameA);
            
            WriteConfigFont(configValue, TRUE);
            LOG_INFO("Auto-applied font: %s", fileNameA);
            
        } else if (lastType == RESOURCE_TYPE_ANIMATION) {
            /* Extract filename or dir name */
            wchar_t* fileNameW = wcsrchr(lastMovedFile, L'\\');
            if (fileNameW) fileNameW++;
            else fileNameW = lastMovedFile;
            
            char fileNameA[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, fileNameW, -1, fileNameA, MAX_PATH, NULL, NULL);
            
            SetCurrentAnimationName(fileNameA);
            LOG_INFO("Auto-applied animation: %s", fileNameA);
        }
    } else if (movedCount > 0) {
        LOG_INFO("Imported %d resources", movedCount);
        MessageBoxW(hwnd, L"Resources imported successfully!", L"Import", MB_ICONINFORMATION | MB_OK);
    }
}
