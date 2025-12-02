/**
 * @file tray_menu_font.c
 * @brief Font scanning and menu construction logic
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include "log.h"
#include "language.h"
#include "tray/tray_menu.h"
#include "tray/tray_menu_font.h"
#include "config.h"
#include "../resource/resource.h"
#include "utils/string_convert.h"
#include "utils/natural_sort.h"
#include "utils/string_format.h"
#include "cache/resource_cache.h"
#include "cache/font_cache.h"
#include "font/font_path_manager.h"

/* External dependencies */
extern char FONT_FILE_NAME[MAX_PATH];
extern void GetConfigPath(char* path, size_t size);
extern BOOL NeedsFontLicenseVersionAcceptance(void);
extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/**
 * @brief Find or create a submenu with the given name
 * @param hParent Parent menu handle
 * @param name Submenu name
 * @param shouldCheck Whether to add MF_CHECKED flag (for parent directories of selected fonts)
 * @note Modified to support checking parent directories
 */
static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name, BOOL shouldCheck) {
    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_STATE;
    wchar_t buffer[MAX_PATH];
    
    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && wcscmp(buffer, name) == 0) {
                /* Submenu exists - update checked state if needed */
                if (shouldCheck && !(mii.fState & MFS_CHECKED)) {
                    mii.fState |= MFS_CHECKED;
                    SetMenuItemInfoW(hParent, i, TRUE, &mii);
                }
                return mii.hSubMenu;
            }
        }
    }
    
    /* Not found, create new */
    HMENU hSub = CreatePopupMenu();
    if (!hSub) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create popup menu for: %S", name);
        return NULL;
    }
    
    UINT flags = MF_POPUP;
    if (shouldCheck) {
        flags |= MF_CHECKED;
    }
    AppendMenuW(hParent, flags, (UINT_PTR)hSub, name);
    return hSub;
}

/**
 * @brief Comparator for font cache entries (by relative path)
 */
static int CompareFontCacheEntries(const void* a, const void* b) {
    const FontCacheEntry* ea = *(const FontCacheEntry**)a;
    const FontCacheEntry* eb = *(const FontCacheEntry**)b;
    return NaturalCompareW(ea->relativePath, eb->relativePath);
}

/**
 * @brief Build font menu from cached entries
 */
static void BuildFontMenuFromCache(HMENU hRootMenu, const FontCacheEntry* entries, int count, int* fontId) {
    // Sort entries to ensure consistent ID assignment matching window_menus.c
    const FontCacheEntry** sortedEntries = (const FontCacheEntry**)malloc(count * sizeof(FontCacheEntry*));
    if (!sortedEntries) return;
    
    for (int i = 0; i < count; i++) {
        sortedEntries[i] = &entries[i];
    }
    
    qsort(sortedEntries, count, sizeof(FontCacheEntry*), CompareFontCacheEntries);
    
    /* STEP 1: Collect all parent directories of selected fonts */
    #define MAX_PARENT_DIRS 100
    wchar_t parentDirs[MAX_PARENT_DIRS][MAX_PATH];
    int parentDirCount = 0;
    
    for (int i = 0; i < count; i++) {
        if (sortedEntries[i]->isCurrentFont) {
            /* Found a selected font - extract all its parent directories */
            wchar_t pathCopy[MAX_PATH];
            wcsncpy(pathCopy, sortedEntries[i]->relativePath, MAX_PATH - 1);
            pathCopy[MAX_PATH - 1] = L'\0';
            
            wchar_t currentPath[MAX_PATH] = L"";
            wchar_t* context = NULL;
            wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
            
            while (token) {
                wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
                if (nextToken) {
                    /* This is a directory component */
                    size_t currentLen = wcslen(currentPath);
                    size_t tokenLen = wcslen(token);
                    
                    /* Check for buffer overflow before concatenating */
                    if (currentLen + tokenLen + 2 < MAX_PATH) {  /* +2 for '\' and '\0' */
                        if (currentLen > 0) {
                            wcscat(currentPath, L"\\");
                        }
                        wcscat(currentPath, token);
                    }
                    
                    /* Add to parent dirs list if not already present */
                    BOOL found = FALSE;
                    for (int j = 0; j < parentDirCount; j++) {
                        if (wcscmp(parentDirs[j], currentPath) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found && parentDirCount < MAX_PARENT_DIRS) {
                        wcsncpy(parentDirs[parentDirCount], currentPath, MAX_PATH - 1);
                        parentDirs[parentDirCount][MAX_PATH - 1] = L'\0';
                        parentDirCount++;
                    }
                }
                token = nextToken;
            }
        }
    }
    
    /* STEP 2: Build menu tree, marking parent directories as checked */
    for (int i = 0; i < count; i++) {
        const FontCacheEntry* entry = sortedEntries[i];
        
        // Parse relative path to find parent menu
        wchar_t pathCopy[MAX_PATH];
        wcsncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = L'\0';
        
        wchar_t* context = NULL;
        wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
        
        HMENU hCurrent = hRootMenu;
        wchar_t currentPath[MAX_PATH] = L"";
        int depth = 0;
        
        while (token) {
            wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
            
            if (nextToken) {
                /* This is a directory component */
                size_t currentLen = wcslen(currentPath);
                size_t tokenLen = wcslen(token);
                
                /* Check for buffer overflow before concatenating */
                if (currentLen + tokenLen + 2 < MAX_PATH) {  /* +2 for '\' and '\0' */
                    if (currentLen > 0) {
                        wcscat(currentPath, L"\\");
                    }
                    wcscat(currentPath, token);
                }
                
                /* Check if this directory should be marked as checked */
                BOOL shouldCheck = FALSE;
                for (int j = 0; j < parentDirCount; j++) {
                    if (wcscmp(parentDirs[j], currentPath) == 0) {
                        shouldCheck = TRUE;
                        break;
                    }
                }
                
                hCurrent = EnsureSubMenu(hCurrent, token, shouldCheck);
                if (!hCurrent) {
                    /* Failed to create submenu, skip this entry */
                    break;
                }
                depth++;
            } else {
                // This is the file component
                UINT flags = MF_STRING;
                if (entry->isCurrentFont) {
                    flags |= MF_CHECKED;
                }
                
                AppendMenuW(hCurrent, flags, (*fontId)++, entry->displayName);
            }
            
            token = nextToken;
        }
    }
    
    free(sortedEntries);
}

/**
 * @brief Build font submenu with cache support
 * @param hMenu Parent menu handle
 */
void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    if (!hFontSubMenu) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create font submenu");
        return;
    }
    
    int g_advancedFontId = CMD_FONT_SELECTION_BASE;
    
    /* Define prefix once for the entire function */
    const char* prefix = FONTS_PATH_PREFIX;
    size_t prefixLen = strlen(prefix);
    
    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        /* CRITICAL: Update font cache before getting entries to ensure isCurrentFont is correct
         * This handles cases where FONT_FILE_NAME was modified by preview */
        extern void FontCache_UpdateCurrent(const char* fontRelativePath);
        
        /* First get current cache to find the correct relative path */
        FontCacheEntry* tempCache = NULL;
        int tempCount = 0;
        FontCacheStatus tempStatus = FontCache_GetEntries(&tempCache, &tempCount);
        
        char relativePathForUpdate[MAX_PATH] = "";
        BOOL needsUpdate = FALSE;
        
        if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
            /* Custom font - extract relative path */
            strncpy(relativePathForUpdate, FONT_FILE_NAME + prefixLen, sizeof(relativePathForUpdate) - 1);
            relativePathForUpdate[sizeof(relativePathForUpdate) - 1] = '\0';
            needsUpdate = TRUE;
        } else if (strchr(FONT_FILE_NAME, '\\') == NULL && strchr(FONT_FILE_NAME, '/') == NULL) {
            /* Just filename - search in cache for EXACT filename match (not substring!)
             * CRITICAL: Must match filename exactly to avoid matching wrong path like:
             *   User selected: 'Font.ttf'
             *   Wrong match:   'fonts\Font.ttf' (substring match)
             *   Correct:       'Font.ttf' (exact filename match)
             * 
             * ALSO: If multiple files with same name exist (e.g., 'Font.ttf' and 'fonts\Font.ttf'),
             *       prefer the one with SHORTEST path (top-level) to match user expectation.
             */
            if (tempStatus == FONT_CACHE_OK || tempStatus == FONT_CACHE_EXPIRED) {
                wchar_t wFileName[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFileName, MAX_PATH);
                
                size_t shortestPathLen = (size_t)-1;
                int bestMatchIndex = -1;
                
                for (int i = 0; i < tempCount; i++) {
                    /* Extract filename from relative path */
                    const wchar_t* lastSlash = wcsrchr(tempCache[i].relativePath, L'\\');
                    if (!lastSlash) lastSlash = wcsrchr(tempCache[i].relativePath, L'/');
                    const wchar_t* entryFileName = lastSlash ? (lastSlash + 1) : tempCache[i].relativePath;
                    
                    /* Match ONLY if filename is exactly the same */
                    if (_wcsicmp(entryFileName, wFileName) == 0) {
                        size_t pathLen = wcslen(tempCache[i].relativePath);
                        if (pathLen < shortestPathLen) {
                            shortestPathLen = pathLen;
                            bestMatchIndex = i;
                        }
                    }
                }
                
                if (bestMatchIndex >= 0) {
                    WideCharToMultiByte(CP_UTF8, 0, tempCache[bestMatchIndex].relativePath, -1, 
                                      relativePathForUpdate, sizeof(relativePathForUpdate), NULL, NULL);
                    needsUpdate = TRUE;
                }
            }
        } else if (strchr(FONT_FILE_NAME, ':') != NULL && 
                  (strstr(FONT_FILE_NAME, "Windows\\Fonts") != NULL || strstr(FONT_FILE_NAME, "WINDOWS\\Fonts") != NULL)) {
            /* System font path - clear marker */
        } else if (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL) {
            /* CRITICAL FIX: Relative path (contains separators but not drive letter)
             * Examples: 'fonts\Font.ttf', 'subdir\another\Font.ttf'
             * These are already relative paths within the custom fonts folder!
             * Use them directly for cache update.
             */
            strncpy(relativePathForUpdate, FONT_FILE_NAME, sizeof(relativePathForUpdate) - 1);
            relativePathForUpdate[sizeof(relativePathForUpdate) - 1] = '\0';
            needsUpdate = TRUE;
        }
        
        /* Free temporary cache */
        if (tempCache) {
            free(tempCache);
        }
        
        /* Now update the cache */
        FontCache_UpdateCurrent(relativePathForUpdate);
        
        // Try to use cached font list
        FontCacheEntry* cachedFonts = NULL;
        int cachedCount = 0;
        FontCacheStatus cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
        
        /* Never block UI - if cache invalid, show loading and trigger async refresh */
        if (cacheStatus != FONT_CACHE_OK && cacheStatus != FONT_CACHE_EXPIRED) {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, L"(Loading...)");
            ResourceCache_RequestRefresh();
        } else if (cacheStatus == FONT_CACHE_OK || cacheStatus == FONT_CACHE_EXPIRED) {
            if (cachedCount == 0) {
                AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, 
                           GetLocalizedString(NULL, L"No font files found"));
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            } else {
                BuildFontMenuFromCache(hFontSubMenu, cachedFonts, cachedCount, &g_advancedFontId);
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
            
            /* Check if current font is a system font BEFORE freeing cache */
            BOOL isSystemFont = TRUE;  /* Default: assume system font */
            
            /* Determine if system font by checking:
             * 1. Starts with FONTS_PATH_PREFIX → custom font (full path)
             * 2. Contains ":\\" → absolute path (check if Windows fonts)
             * 3. Contains path separators → relative path → custom font
             * 4. Just a filename (no separators) → check cache
             */
            if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
                /* Starts with custom fonts prefix */
                isSystemFont = FALSE;
            } else if (strchr(FONT_FILE_NAME, ':') != NULL) {
                /* Contains drive letter → absolute path, likely system font */
                isSystemFont = (strstr(FONT_FILE_NAME, "Windows\\Fonts") != NULL || 
                               strstr(FONT_FILE_NAME, "WINDOWS\\Fonts") != NULL);
            } else if (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL) {
                /* CRITICAL FIX: Contains path separators but no drive letter → relative path → custom font!
                 * Examples: 'fonts\Font.ttf', 'subdir\Font.ttf'
                 * These are relative paths within the custom fonts folder, NOT system fonts!
                 */
                isSystemFont = FALSE;
            } else {
                /* No path separators at all → just filename, check if it's in custom fonts cache */
                isSystemFont = TRUE;  /* Default to system */
                
                /* Convert once outside loop for efficiency */
                wchar_t wFontName[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFontName, MAX_PATH);
                
                for (int i = 0; i < cachedCount; i++) {
                    if (wcsstr(cachedFonts[i].relativePath, wFontName) != NULL) {
                        isSystemFont = FALSE;  /* Found in custom fonts cache */
                        break;
                    }
                }
            }
            
            UINT systemFontFlags = MF_STRING;
            if (isSystemFont) {
                systemFontFlags |= MF_CHECKED;
            }
            
            // Free the copy returned by GetEntries
            // This is critical because GetEntries returns a deep copy to ensure thread safety
            if (cachedFonts) {
                free(cachedFonts);
            }

            if (cacheStatus == FONT_CACHE_EXPIRED) {
                ResourceCache_RequestRefresh();
            }
            
            /* Now append System Fonts menu item with correct checked state */
            AppendMenuW(hFontSubMenu, systemFontFlags, CLOCK_IDM_SYSTEM_FONT_PICKER,
                       GetLocalizedString(NULL, L"System Fonts..."));
        } else {
            /* Cache not ready, add unchecked System Fonts item */
            AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDM_SYSTEM_FONT_PICKER,
                       GetLocalizedString(NULL, L"System Fonts..."));
        }
        
        AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED, 
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(NULL, L"Font"));
}

/**
 * @brief Get font path from menu ID
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    
    // Base ID check
    if (id < CMD_FONT_SELECTION_BASE) return FALSE;
    
    FontCacheEntry* cachedFonts = NULL;
    int cachedCount = 0;
    FontCacheStatus cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
    
    if (cacheStatus != FONT_CACHE_OK && cacheStatus != FONT_CACHE_EXPIRED) {
        return FALSE;
    }
    
    if (!cachedFonts || cachedCount == 0) {
        if (cachedFonts) free(cachedFonts);
        return FALSE;
    }
    
    // Sort to match menu order
    const FontCacheEntry** sortedEntries = (const FontCacheEntry**)malloc(cachedCount * sizeof(FontCacheEntry*));
    if (!sortedEntries) {
        free(cachedFonts);
        return FALSE;
    }
    
    for (int i = 0; i < cachedCount; i++) {
        sortedEntries[i] = &cachedFonts[i];
    }
    
    qsort(sortedEntries, cachedCount, sizeof(FontCacheEntry*), CompareFontCacheEntries);
    
    BOOL found = FALSE;
    int currentId = CMD_FONT_SELECTION_BASE;
    
    for (int i = 0; i < cachedCount; i++) {
        if (currentId == (int)id) {
            WideCharToMultiByte(CP_UTF8, 0, sortedEntries[i]->relativePath, -1, outPath, (int)outPathSize, NULL, NULL);
            found = TRUE;
            break;
        }
        currentId++;
    }
    
    free(sortedEntries);
    free(cachedFonts);
    
    return found;
}
