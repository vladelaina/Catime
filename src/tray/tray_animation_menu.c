/**
 * @file tray_animation_menu.c
 * @brief Animation menu implementation
 */

#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "log.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
BOOL SetCurrentAnimationName(const char* name);

/**
 * @brief Animation menu entry
 */
typedef struct {
    wchar_t name[MAX_PATH];
    char rel_path_utf8[MAX_PATH];
    BOOL is_dir;
} AnimationEntry;

/**
 * @brief Comparator for animation entries (folders first, then natural sort)
 */
static int CompareAnimationEntries(const void* a, const void* b) {
    const AnimationEntry* entryA = (const AnimationEntry*)a;
    const AnimationEntry* entryB = (const AnimationEntry*)b;
    
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir;
    }
    
    return NaturalCompareW(entryA->name, entryB->name);
}

/**
 * @brief Check if folder is a leaf (no subfolders or animated images)
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

/**
 * @brief Build menu recursively
 */
static UINT BuildAnimationMenuRecursive(HMENU hMenu, const wchar_t* folderPathW,
                                       const char* folderPathUtf8, UINT* nextId,
                                       const char* currentAnimationName) {
    /* Use larger limit to match window_menus.c and avoid ID mismatch */
    const int MAX_ENTRIES = 4096;
    AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_ENTRIES);
    if (!entries) return 0;
    
    int entryCount = 0;
    
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
            if (entryCount >= MAX_ENTRIES) break;
            
            AnimationEntry* e = &entries[entryCount];
            e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
            e->name[MAX_PATH - 1] = L'\0';
            
            char itemUtf8[MAX_PATH] = {0};
            WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
            
            if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
            } else {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
            }
            
            if (e->is_dir) {
                entryCount++;
            } else {
                wchar_t* ext = wcsrchr(e->name, L'.');
                if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                           _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                           _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                           _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                           _wcsicmp(ext, L".tiff") == 0)) {
                    entryCount++;
                }
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }
    
    if (entryCount == 0) {
        free(entries);
        return 0;
    }
    
    qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);
    
    UINT itemsAdded = 0;
    for (int i = 0; i < entryCount; ++i) {
        AnimationEntry* e = &entries[i];
        
        if (e->is_dir) {
            wchar_t wSubFolderPath[MAX_PATH] = {0};
            _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);
            
            if (IsAnimationLeafFolderW(wSubFolderPath)) {
                /* Leaf folder - clickable item */
                UINT flags = MF_STRING;
                if (currentAnimationName && _stricmp(currentAnimationName, e->rel_path_utf8) == 0) {
                    flags |= MF_CHECKED;
                }
                AppendMenuW(hMenu, flags, *nextId, e->name);
                (*nextId)++;
                itemsAdded++;
            } else {
                /* Branch folder - submenu */
                HMENU hSubMenu = CreatePopupMenu();
                UINT subItems = BuildAnimationMenuRecursive(hSubMenu, wSubFolderPath, 
                                                           e->rel_path_utf8, nextId,
                                                           currentAnimationName);
                if (subItems > 0) {
                    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, e->name);
                    itemsAdded++;
                } else {
                    DestroyMenu(hSubMenu);
                }
            }
        } else {
            /* File - clickable item */
            UINT flags = MF_STRING;
            if (currentAnimationName && _stricmp(currentAnimationName, e->rel_path_utf8) == 0) {
                flags |= MF_CHECKED;
            }
            AppendMenuW(hMenu, flags, *nextId, e->name);
            (*nextId)++;
            itemsAdded++;
        }
    }
    
    free(entries);
    return itemsAdded;
}

/**
 * @brief Build animation menu
 */
void BuildAnimationMenu(HMENU hMenu, const char* currentAnimationName) {
    if (!hMenu) return;

    /* Builtin options */
    UINT flags = MF_STRING;
    if (currentAnimationName && _stricmp(currentAnimationName, "__logo__") == 0) {
        flags |= MF_CHECKED;
    }
    AppendMenuW(hMenu, flags, CLOCK_IDM_ANIMATIONS_USE_LOGO, L"Logo");

    flags = MF_STRING;
    if (currentAnimationName && _stricmp(currentAnimationName, "__cpu__") == 0) {
        flags |= MF_CHECKED;
    }
    AppendMenuW(hMenu, flags, CLOCK_IDM_ANIMATIONS_USE_CPU, L"CPU %");

    flags = MF_STRING;
    if (currentAnimationName && _stricmp(currentAnimationName, "__mem__") == 0) {
        flags |= MF_CHECKED;
    }
    AppendMenuW(hMenu, flags, CLOCK_IDM_ANIMATIONS_USE_MEM, L"Memory %");
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    /* Custom animations */
    char animRootUtf8[MAX_PATH] = {0};
    GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
    
    wchar_t wRoot[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);
    
    UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
    UINT customItems = BuildAnimationMenuRecursive(hMenu, wRoot, "", &nextId, currentAnimationName);
}

/**
 * @brief Find animation by ID (recursive)
 */
static BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* folderPathUtf8,
                                       UINT* nextIdPtr, UINT targetId, char* foundPath, size_t foundPathSize) {
    /* Use larger limit to match window_menus.c and avoid ID mismatch */
    const int MAX_ENTRIES = 4096;
    AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_ENTRIES);
    if (!entries) return FALSE;
    
    int entryCount = 0;
    
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
            if (entryCount >= MAX_ENTRIES) break;
            
            AnimationEntry* e = &entries[entryCount];
            e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
            e->name[MAX_PATH - 1] = L'\0';
            
            char itemUtf8[MAX_PATH] = {0};
            WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
            
            if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
            } else {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
            }
            
            if (e->is_dir) {
                entryCount++;
            } else {
                wchar_t* ext = wcsrchr(e->name, L'.');
                if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                           _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                           _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                           _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                           _wcsicmp(ext, L".tiff") == 0)) {
                    entryCount++;
                }
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }
    
    if (entryCount == 0) {
        free(entries);
        return FALSE;
    }
    
    qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);
    
    for (int i = 0; i < entryCount; ++i) {
        AnimationEntry* e = &entries[i];
        
        if (e->is_dir) {
            wchar_t wSubFolderPath[MAX_PATH] = {0};
            _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);
            
            if (IsAnimationLeafFolderW(wSubFolderPath)) {
                if (*nextIdPtr == targetId) {
                    strncpy(foundPath, e->rel_path_utf8, foundPathSize - 1);
                    foundPath[foundPathSize - 1] = '\0';
                    free(entries);
                    return TRUE;
                }
                (*nextIdPtr)++;
            } else {
                if (FindAnimationByIdRecursive(wSubFolderPath, e->rel_path_utf8, nextIdPtr, targetId, foundPath, foundPathSize)) {
                    free(entries);
                    return TRUE;
                }
            }
        } else {
            if (*nextIdPtr == targetId) {
                strncpy(foundPath, e->rel_path_utf8, foundPathSize - 1);
                foundPath[foundPathSize - 1] = '\0';
                free(entries);
                return TRUE;
            }
            (*nextIdPtr)++;
        }
    }
    
    free(entries);
    return FALSE;
}

/**
 * @brief Handle animation menu command
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    (void)hwnd;

    if (id == CLOCK_IDM_ANIM_SPEED_MEMORY || id == CLOCK_IDM_ANIM_SPEED_CPU || id == CLOCK_IDM_ANIM_SPEED_TIMER) {
        return FALSE;
    }

    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }

    if (id == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        return SetCurrentAnimationName("__logo__");
    }

    if (id == CLOCK_IDM_ANIMATIONS_USE_CPU) {
        return SetCurrentAnimationName("__cpu__");
    }

    if (id == CLOCK_IDM_ANIMATIONS_USE_MEM) {
        return SetCurrentAnimationName("__mem__");
    }

    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));

        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);

        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        char foundPath[MAX_PATH] = {0};

        if (FindAnimationByIdRecursive(wRoot, "", &nextId, id, foundPath, sizeof(foundPath))) {
            return SetCurrentAnimationName(foundPath);
        }

        return FALSE;
    }

    return FALSE;
}

/**
 * @brief Open animations folder in Explorer
 */
void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH);
    
    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Get animation name from menu ID
 */
BOOL GetAnimationNameFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    
    if (id == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        strncpy(outPath, "__logo__", outPathSize - 1);
        outPath[outPathSize - 1] = '\0';
        return TRUE;
    }
    
    if (id == CLOCK_IDM_ANIMATIONS_USE_CPU) {
        strncpy(outPath, "__cpu__", outPathSize - 1);
        outPath[outPathSize - 1] = '\0';
        return TRUE;
    }
    
    if (id == CLOCK_IDM_ANIMATIONS_USE_MEM) {
        strncpy(outPath, "__mem__", outPathSize - 1);
        outPath[outPathSize - 1] = '\0';
        return TRUE;
    }
    
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);
        
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        char foundPath[MAX_PATH] = {0};
        
        if (FindAnimationByIdRecursive(wRoot, "", &nextId, id, foundPath, sizeof(foundPath))) {
            strncpy(outPath, foundPath, outPathSize - 1);
            outPath[outPathSize - 1] = '\0';
            return TRUE;
        }
    }
    
    return FALSE;
}

