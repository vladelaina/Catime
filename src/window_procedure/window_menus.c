/**
 * @file window_menus.c
 * @brief Menu construction and preview dispatch implementation
 */

#include "window_procedure/window_menus.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "menu_preview.h"
#include "font.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_core.h"
#include "utils/natural_sort.h"
#include "color/color_state.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Large limit for menu display to accommodate folder-based animations with many frames */
#define MAX_SCAN_ENTRIES 4096

/* ============================================================================
 * File Extension Matching
 * ============================================================================ */

static BOOL MatchExtension(const wchar_t* filename, const wchar_t** exts, size_t count) {
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (!ext) return FALSE;
    for (size_t i = 0; i < count; i++) {
        if (_wcsicmp(ext, exts[i]) == 0) return TRUE;
    }
    return FALSE;
}

static const wchar_t* ANIMATION_EXTS[] = {
    L".gif", L".webp", L".ico", L".png", L".bmp", 
    L".jpg", L".jpeg", L".tif", L".tiff"
};
static const wchar_t* FONT_EXTS[] = {L".ttf", L".otf"};

BOOL IsAnimationFile(const wchar_t* filename) {
    return MatchExtension(filename, ANIMATION_EXTS, sizeof(ANIMATION_EXTS) / sizeof(ANIMATION_EXTS[0]));
}

BOOL IsFontFile(const wchar_t* filename) {
    return MatchExtension(filename, FONT_EXTS, sizeof(FONT_EXTS) / sizeof(FONT_EXTS[0]));
}

/* ============================================================================
 * File Entry System
 * ============================================================================ */

typedef struct {
    wchar_t name[MAX_PATH];
    char relPathUtf8[MAX_PATH];
    BOOL isDir;
} FileEntry;

static int CompareFileEntries(const void* a, const void* b) {
    const FileEntry* ea = (const FileEntry*)a;
    const FileEntry* eb = (const FileEntry*)b;
    if (ea->isDir != eb->isDir) return eb->isDir - ea->isDir;
    return NaturalCompareW(ea->name, eb->name);
}

/* ============================================================================
 * Recursive File Finder
 * ============================================================================ */

/**
 * @brief Check if folder is a leaf (no subfolders or animated images)
 * @note Copied from tray_animation_menu.c to ensure consistent ID mapping
 */
static BOOL IsAnimationLeafFolderW(const wchar_t* folderPathW) {
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return TRUE;

    BOOL hasSubItems = FALSE;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            hasSubItems = TRUE;
            break;
        }
        
        wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
        if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
            hasSubItems = TRUE;
            break;
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    return !hasSubItems;
}

typedef BOOL (*IsLeafFolderFunc)(const wchar_t* path);

BOOL RecursiveFindFile(const wchar_t* rootPathW, const char* relPathUtf8,
                       FileFilterFunc filter, IsLeafFolderFunc leafFolderFunc,
                       UINT targetId, UINT* currentId,
                       FileActionFunc action, void* userData) {
    FileEntry* entries = (FileEntry*)malloc(sizeof(FileEntry) * MAX_SCAN_ENTRIES);
    if (!entries) return FALSE;
    
    int count = 0;
    wchar_t searchPath[MAX_PATH];
    wcscpy_s(searchPath, MAX_PATH, rootPathW);
    PathJoinW(searchPath, MAX_PATH, L"*");
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(entries);
        return FALSE;
    }
    
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        if (count >= MAX_SCAN_ENTRIES) break;
        
        FileEntry* e = &entries[count];
        e->isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        wcsncpy_s(e->name, MAX_PATH, ffd.cFileName, _TRUNCATE);
        
        char nameUtf8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, nameUtf8, MAX_PATH, NULL, NULL);
        
        if (relPathUtf8 && relPathUtf8[0]) {
            snprintf(e->relPathUtf8, MAX_PATH, "%s\\%s", relPathUtf8, nameUtf8);
        } else {
            strncpy_s(e->relPathUtf8, MAX_PATH, nameUtf8, _TRUNCATE);
        }
        
        if (!e->isDir && filter && !filter(ffd.cFileName)) continue;
        count++;
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    if (count == 0) {
        free(entries);
        return FALSE;
    }
    
    qsort(entries, count, sizeof(FileEntry), CompareFileEntries);
    
    for (int i = 0; i < count; i++) {
        FileEntry* e = &entries[i];
        
        if (e->isDir) {
            wchar_t subPath[MAX_PATH];
            wcscpy_s(subPath, MAX_PATH, rootPathW);
            PathJoinW(subPath, MAX_PATH, e->name);
            
            // Check if this directory is a "Leaf Folder" (item) or a "Branch Folder" (container)
            BOOL isLeaf = leafFolderFunc ? leafFolderFunc(subPath) : FALSE;
            
            if (isLeaf) {
                // Treat folder as an item
                if (*currentId == targetId) {
                    BOOL result = action(e->relPathUtf8, userData);
                    free(entries);
                    return result;
                }
                (*currentId)++;
            } else {
                // Treat as container and recurse
                if (RecursiveFindFile(subPath, e->relPathUtf8, filter, leafFolderFunc, 
                                    targetId, currentId, action, userData)) {
                    free(entries);
                    return TRUE;
                }
            }
        } else {
            if (*currentId == targetId) {
                BOOL result = action(e->relPathUtf8, userData);
                free(entries);
                return result;
            }
            (*currentId)++;
        }
    }
    
    free(entries);
    return FALSE;
}

/* ============================================================================
 * Animation Preview Action
 * ============================================================================ */

static BOOL AnimationPreviewAction(const char* relPath, void* userData) {
    (void)userData;
    StartAnimationPreview(relPath);
    return TRUE;
}

BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* relPathUtf8, 
                                UINT* nextIdPtr, UINT targetId) {
    return RecursiveFindFile(folderPathW, relPathUtf8, IsAnimationFile, IsAnimationLeafFolderW,
                           targetId, nextIdPtr, AnimationPreviewAction, NULL);
}

/* ============================================================================
 * Font Preview Action
 * ============================================================================ */

typedef struct {
    wchar_t relPath[MAX_PATH];
    HWND hwnd;
} FontFindData;

static BOOL FontPreviewAction(const char* relPath, void* userData) {
    if (!userData) return FALSE;
    wchar_t relPathW[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, relPath, -1, relPathW, MAX_PATH);
    wcsncpy_s(((FontFindData*)userData)->relPath, MAX_PATH, relPathW, _TRUNCATE);
    return TRUE;
}

BOOL FindFontByIdRecursiveW(const wchar_t* folderPathW, int targetId, int* currentId,
                            wchar_t* foundRelativePathW, const wchar_t* fontsFolderRootW) {
    (void)fontsFolderRootW;
    
    FontFindData data = {0};
    UINT id = (UINT)*currentId;
    
    // Fonts generally don't use leaf-folder logic, passing NULL
    if (RecursiveFindFile(folderPathW, "", IsFontFile, NULL, 
                        (UINT)targetId, &id, FontPreviewAction, &data)) {
        wcsncpy_s(foundRelativePathW, MAX_PATH, data.relPath, _TRUNCATE);
        *currentId = (int)id;
        return TRUE;
    }
    
    *currentId = (int)id;
    return FALSE;
}

/* ============================================================================
 * Preview Dispatch
 * ============================================================================ */

BOOL DispatchMenuPreview(HWND hwnd, UINT menuId) {
    if (menuId == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        StartAnimationPreview("__logo__");
        return TRUE;
    }

    if (menuId == CLOCK_IDM_ANIMATIONS_USE_CPU) {
        StartAnimationPreview("__cpu__");
        return TRUE;
    }

    if (menuId == CLOCK_IDM_ANIMATIONS_USE_MEM) {
        StartAnimationPreview("__mem__");
        return TRUE;
    }

    if (menuId >= CLOCK_IDM_ANIMATIONS_BASE && menuId < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        wchar_t animFolderW[MAX_PATH];
        WideString ws = ToWide(GetCachedConfigPath());
        if (!ws.valid) return FALSE;
        wchar_t wConfigPath[MAX_PATH];
        wcscpy_s(wConfigPath, MAX_PATH, ws.buf);

        wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
        if (lastSep) {
            *lastSep = L'\0';
            _snwprintf_s(animFolderW, MAX_PATH, _TRUNCATE, L"%s\\resources\\animations", wConfigPath);
            UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
            return FindAnimationByIdRecursive(animFolderW, "", &nextId, menuId);
        }
    }

    if (menuId >= 2000 && menuId < 3000) {
        wchar_t fontsFolderW[MAX_PATH];
        if (!GetFontsFolderWideFromConfig(fontsFolderW, MAX_PATH)) return FALSE;

        int currentIndex = 2000;
        wchar_t foundRelPath[MAX_PATH];
        if (FindFontByIdRecursiveW(fontsFolderW, menuId, &currentIndex, foundRelPath, fontsFolderW)) {
            char fontPathUtf8[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, foundRelPath, -1, fontPathUtf8, MAX_PATH, NULL, NULL);
            StartPreview(PREVIEW_TYPE_FONT, fontPathUtf8, hwnd);
            return TRUE;
        }
    }

    int colorIndex = menuId - 201;
    if (colorIndex >= 0 && colorIndex < (int)COLOR_OPTIONS_COUNT) {
        StartPreview(PREVIEW_TYPE_COLOR, COLOR_OPTIONS[colorIndex].hexColor, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_DEFAULT) {
        TimeFormatType format = TIME_FORMAT_DEFAULT;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) {
        TimeFormatType format = TIME_FORMAT_ZERO_PADDED;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) {
        TimeFormatType format = TIME_FORMAT_FULL_PADDED;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
        BOOL showMilliseconds = !g_AppConfig.display.time_format.show_milliseconds;
        StartPreview(PREVIEW_TYPE_MILLISECONDS, &showMilliseconds, hwnd);
        return TRUE;
    }

    return FALSE;
}

