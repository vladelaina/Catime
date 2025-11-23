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
 * @note Copied from tray_animation_menu.c (should be util but keeping localized for now)
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
    
    for (int i = 0; i < count; i++) {
        const FontCacheEntry* entry = sortedEntries[i];
        
        // Parse relative path to find parent menu
        wchar_t pathCopy[MAX_PATH];
        wcsncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = L'\0';
        
        wchar_t* context = NULL;
        wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
        
        HMENU hCurrent = hRootMenu;
        
        while (token) {
            wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
            
            if (nextToken) {
                // This is a directory component
                hCurrent = EnsureSubMenu(hCurrent, token);
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
    
    int g_advancedFontId = 2000;
    
    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        // Try to use cached font list
        FontCacheEntry* cachedFonts = NULL;
        int cachedCount = 0;
        FontCacheStatus cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
        
        // If cache is invalid/empty, force a synchronous scan now
        if (cacheStatus != FONT_CACHE_OK && cacheStatus != FONT_CACHE_EXPIRED) {
            WriteLog(LOG_LEVEL_INFO, "Font cache not ready, forcing synchronous scan");
            if (FontCache_Scan()) {
                cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
            } else {
                // Scan failed, maybe try extracting embedded fonts
                HINSTANCE hInst = GetModuleHandle(NULL);
                if (ExtractEmbeddedFontsToFolder(hInst)) {
                    if (FontCache_Scan()) {
                        cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
                    }
                }
            }
        }
        
        if (cacheStatus == FONT_CACHE_OK || cacheStatus == FONT_CACHE_EXPIRED) {
            if (cachedCount == 0) {
                AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, 
                           GetLocalizedString(NULL, L"No font files found"));
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            } else {
                BuildFontMenuFromCache(hFontSubMenu, cachedFonts, cachedCount, &g_advancedFontId);
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
            
            // Free the copy returned by GetEntries
            // This is critical because GetEntries returns a deep copy to ensure thread safety
            if (cachedFonts) {
                free(cachedFonts);
            }

            if (cacheStatus == FONT_CACHE_EXPIRED) {
                ResourceCache_RequestRefresh();
            }
        } else {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, L"(Loading...)");
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            ResourceCache_RequestRefresh();
        }
        
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED, 
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(NULL, L"Font"));
}
