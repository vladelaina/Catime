/**
 * @file tray_animation_menu.c
 * @brief Animation menu implementation
 */

#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "log.h"
#include "cache/animation_cache.h"
#include "cache/resource_cache.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
BOOL SetCurrentAnimationName(const char* name);

/**
 * @brief Find or create a submenu with the given name
 */
static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name) {
    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
    mii.fMask = MIIM_STRING | MIIM_SUBMENU;
    wchar_t buffer[MAX_PATH];
    
    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && wcscmp(buffer, name) == 0) {
                return mii.hSubMenu;
            }
        }
    }
    
    // Not found, create new
    HMENU hSub = CreatePopupMenu();
    AppendMenuW(hParent, MF_POPUP, (UINT_PTR)hSub, name);
    return hSub;
}

/**
 * @brief Comparator for animation cache entries (by relative path, files before folders)
 */
static int CompareAnimationCacheEntries(const void* a, const void* b) {
    const AnimationCacheEntry* ea = *(const AnimationCacheEntry**)a;
    const AnimationCacheEntry* eb = *(const AnimationCacheEntry**)b;
    return NaturalPathCompareA(ea->relativePath, eb->relativePath);
}

/**
 * @brief Build menu from cached entries
 */
static void BuildAnimationMenuFromCache(HMENU hRootMenu, const AnimationCacheEntry* entries, int count, const char* currentAnim) {
    // We need to process entries in a way that respects hierarchy.
    // Since cache is flat, we parse relative paths.
    
    // First, we create a temporary array of pointers to sort them
    const AnimationCacheEntry** sortedEntries = (const AnimationCacheEntry**)malloc(count * sizeof(AnimationCacheEntry*));
    if (!sortedEntries) return;
    
    for (int i = 0; i < count; i++) {
        sortedEntries[i] = &entries[i];
    }
    
    qsort(sortedEntries, count, sizeof(AnimationCacheEntry*), CompareAnimationCacheEntries);

    UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;

    for (int i = 0; i < count; i++) {
        const AnimationCacheEntry* entry = sortedEntries[i];
        
        if (entry->isSpecial) continue; // Skip special entries (handled manually)
        
        // Parse relative path to find parent menu
        // Example: "Folder/Sub/Image.gif"
        
        char pathCopy[MAX_PATH];
        strncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = '\0';
        
        char* context = NULL;
        char* token = strtok_s(pathCopy, "\\/", &context);
        
        HMENU hCurrent = hRootMenu;
        
        while (token) {
            char* nextToken = strtok_s(NULL, "\\/", &context);
            
            wchar_t wName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, token, -1, wName, MAX_PATH);
            
            if (nextToken) {
                // This is a directory component
                hCurrent = EnsureSubMenu(hCurrent, wName);
            } else {
                // This is the file component
                UINT flags = MF_STRING;
                if (currentAnim && strcmp(currentAnim, entry->relativePath) == 0) {
                    flags |= MF_CHECKED;
                }
                
                // Only add if we haven't exceeded ID range
                if (nextId < CLOCK_IDM_ANIMATIONS_END) {
                    AppendMenuW(hCurrent, flags, nextId++, wName);
                }
            }
            
            token = nextToken;
        }
    }
    
    free(sortedEntries);
}

/**
 * @brief Build animation menu
 */
void BuildAnimationMenu(HMENU hMenu, const char* currentAnimationName) {
    if (!hMenu) return;

    /* Builtin options from registry */
    int builtinCount = 0;
    const BuiltinAnimDef* builtins = GetBuiltinAnims(&builtinCount);
    
    for (int i = 0; i < builtinCount; i++) {
        UINT flags = MF_STRING;
        if (currentAnimationName && _stricmp(currentAnimationName, builtins[i].name) == 0) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hMenu, flags, builtins[i].menuId, builtins[i].menuLabel);
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    /* Custom animations from Cache */
    AnimationCacheEntry* cachedEntries = NULL;
    int cachedCount = 0;
    AnimationCacheStatus status = AnimationCache_GetEntries(&cachedEntries, &cachedCount);
    
    /* Never block UI - if cache invalid, show loading and trigger async refresh */
    if (status != ANIM_CACHE_OK && status != ANIM_CACHE_EXPIRED) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(Loading...)");
        ResourceCache_RequestRefresh();
        return;
    }
    
    /* Cache is valid - build menu from cached entries */
    BuildAnimationMenuFromCache(hMenu, cachedEntries, cachedCount, currentAnimationName);
    
    if (cachedEntries) {
        free(cachedEntries);
    }
    
    /* If expired, trigger async refresh for next time */
    if (status == ANIM_CACHE_EXPIRED) {
        ResourceCache_RequestRefresh();
    }
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

    /* Check builtin animations first */
    const BuiltinAnimDef* def = GetBuiltinAnimDefById(id);
    if (def) {
        return SetCurrentAnimationName(def->name);
    }

    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
        // Find entry by index logic
        // Since we sorted entries in BuildAnimationMenu, we need to reproduce the order
        // or simpler: Re-sort is fast enough for a click handler (microseconds)
        
        AnimationCacheEntry* cachedEntries = NULL;
        int cachedCount = 0;
        AnimationCacheStatus status = AnimationCache_GetEntries(&cachedEntries, &cachedCount);
        
        if (status != ANIM_CACHE_OK && status != ANIM_CACHE_EXPIRED) {
            if (cachedEntries) free(cachedEntries);
            return FALSE;
        }
        
        // Reconstruct sorted list to match IDs
        const AnimationCacheEntry** sortedEntries = (const AnimationCacheEntry**)malloc(cachedCount * sizeof(AnimationCacheEntry*));
        if (!sortedEntries) {
            free(cachedEntries);
            return FALSE;
        }
        
        for (int i = 0; i < cachedCount; i++) {
            sortedEntries[i] = &cachedEntries[i];
        }
        
        qsort(sortedEntries, cachedCount, sizeof(AnimationCacheEntry*), CompareAnimationCacheEntries);
        
        // Map ID to entry
        UINT currentId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL found = FALSE;
        
        for (int i = 0; i < cachedCount; i++) {
            if (sortedEntries[i]->isSpecial) continue;
            
            if (currentId == id) {
                SetCurrentAnimationName(sortedEntries[i]->relativePath);
                found = TRUE;
                break;
            }
            currentId++;
        }
        
        free(sortedEntries);
        free(cachedEntries);
        return found;
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
    
    /* Check builtin animations */
    const BuiltinAnimDef* def = GetBuiltinAnimDefById(id);
    if (def) {
        strncpy(outPath, def->name, outPathSize - 1);
        outPath[outPathSize - 1] = '\0';
        return TRUE;
    }
    
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
         AnimationCacheEntry* cachedEntries = NULL;
        int cachedCount = 0;
        AnimationCacheStatus status = AnimationCache_GetEntries(&cachedEntries, &cachedCount);
        
        if (status != ANIM_CACHE_OK && status != ANIM_CACHE_EXPIRED) {
            if (cachedEntries) free(cachedEntries);
            return FALSE;
        }
        
        AnimationCacheEntry** sortedEntries = (AnimationCacheEntry**)malloc(cachedCount * sizeof(AnimationCacheEntry*));
        if (!sortedEntries) {
            free(cachedEntries);
            return FALSE;
        }
        
        for (int i = 0; i < cachedCount; i++) {
            sortedEntries[i] = &cachedEntries[i];
        }
        
        qsort(sortedEntries, cachedCount, sizeof(AnimationCacheEntry*), CompareAnimationCacheEntries);
        
        UINT currentId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL found = FALSE;
        
        for (int i = 0; i < cachedCount; i++) {
            if (sortedEntries[i]->isSpecial) continue;
            
            if (currentId == id) {
                strncpy(outPath, sortedEntries[i]->relativePath, outPathSize - 1);
                outPath[outPathSize - 1] = '\0';
                found = TRUE;
                break;
            }
            currentId++;
        }
        
        free(sortedEntries);
        free(cachedEntries);
        return found;
    }
    
    return FALSE;
}
