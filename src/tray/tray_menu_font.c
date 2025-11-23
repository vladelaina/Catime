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
#include "font/font_path_manager.h"

/* External dependencies */
extern char FONT_FILE_NAME[MAX_PATH];
extern void GetConfigPath(char* path, size_t size);
extern BOOL NeedsFontLicenseVersionAcceptance(void);
extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* Constants */
#define FONTS_PATH_PREFIX "fonts\\"

/** @brief Font menu entry with submenu tracking */
typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t fullPath[MAX_PATH];
    wchar_t displayName[MAX_PATH];
    BOOL is_dir;
    BOOL isCurrentFont;
    int subFolderStatus;
    HMENU hSubMenu;
} FontEntry;

/** @brief qsort comparator for FontEntry - directories first, then natural sort */
static int CompareFontEntries(const void* a, const void* b) {
    const FontEntry* entryA = (const FontEntry*)a;
    const FontEntry* entryB = (const FontEntry*)b;
    
    /* Directories first */
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir;
    }
    
    /* Then natural sort by name */
    return NaturalCompareW(entryA->name, entryB->name);
}

/**
 * @brief Get fonts folder path (%LOCALAPPDATA%\Catime\resources\fonts)
 * @param out Wide-char buffer
 * @param size Buffer size
 * @return TRUE on success
 */
static BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size) {
    if (!out || size == 0) return FALSE;
    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);
    if (configPathUtf8[0] == '\0') return FALSE;
    wchar_t wconfigPath[MAX_PATH] = {0};
    Utf8ToWide(configPathUtf8, wconfigPath, MAX_PATH);
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 1 >= size) return FALSE;
    wcsncpy(out, wconfigPath, dirLen);
    out[dirLen] = L'\0';
    const wchar_t* tail = L"\\resources\\fonts";
    if (wcslen(out) + wcslen(tail) + 1 >= size) return FALSE;
    wcscat(out, tail);
    return TRUE;
}

/**
 * @brief Recursively scan font folder and build submenu
 * @return 0=no content, 1=has content, 2=contains current font
 * @note Uses heap allocation to prevent stack overflow
 */
static int ScanFontFolder(const char* folderPath, HMENU parentMenu, int* fontId, const wchar_t* targetFontPath) {
    wchar_t* wFolderPath = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    wchar_t* wSearchPath = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    WIN32_FIND_DATAW* findData = (WIN32_FIND_DATAW*)malloc(sizeof(WIN32_FIND_DATAW));

    if (!wFolderPath || !wSearchPath || !findData) {
        if (wFolderPath) free(wFolderPath);
        if (wSearchPath) free(wSearchPath);
        if (findData) free(findData);
        return 0;
    }

    Utf8ToWide(folderPath, wFolderPath, MAX_PATH);
    _snwprintf_s(wSearchPath, MAX_PATH, _TRUNCATE, L"%s\\*", wFolderPath);
    
    FontEntry* entries = NULL;
    int entryCount = 0;
    int entryCapacity = 0;
    
    HANDLE hFind = FindFirstFileW(wSearchPath, findData);
    int folderStatus = 0;
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData->cFileName, L".") == 0 || wcscmp(findData->cFileName, L"..") == 0) {
                continue;
            }
            
            if (entryCount >= entryCapacity) {
                entryCapacity = entryCapacity == 0 ? 16 : entryCapacity * 2;
                FontEntry* newEntries = (FontEntry*)realloc(entries, entryCapacity * sizeof(FontEntry));
                if (!newEntries) {
                    free(entries);
                    free(wFolderPath);
                    free(wSearchPath);
                    free(findData);
                    FindClose(hFind);
                    return 0;
                }
                entries = newEntries;
            }
            
            FontEntry* entry = &entries[entryCount];
            wcsncpy(entry->name, findData->cFileName, MAX_PATH - 1);
            entry->name[MAX_PATH - 1] = L'\0';
            _snwprintf_s(entry->fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolderPath, findData->cFileName);
            entry->is_dir = (findData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entry->isCurrentFont = FALSE;
            entry->subFolderStatus = 0;
            entry->hSubMenu = NULL;
            
            if (!entry->is_dir) {
                wchar_t* ext = wcsrchr(findData->cFileName, L'.');
                if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                    wcsncpy(entry->displayName, findData->cFileName, MAX_PATH - 1);
                    entry->displayName[MAX_PATH - 1] = L'\0';
                    wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
                    if (dotPos) *dotPos = L'\0';
                    
                    /* Compare absolute paths using passed target path */
                    if (targetFontPath && targetFontPath[0] != L'\0') {
                        entry->isCurrentFont = (_wcsicmp(entry->fullPath, targetFontPath) == 0);
                    }
                    
                    entryCount++;
                    
                    if (entry->isCurrentFont) {
                        folderStatus = 2;
                    } else if (folderStatus == 0) {
                        folderStatus = 1;
                    }
                } else {
                    continue;
                }
            } else {
                entry->hSubMenu = CreatePopupMenu();
                
                char fullItemPathUtf8[MAX_PATH];
                WideToUtf8(entry->fullPath, fullItemPathUtf8, MAX_PATH);
                
                entry->subFolderStatus = ScanFontFolder(fullItemPathUtf8, entry->hSubMenu, fontId, targetFontPath);
                
                entryCount++;
                
                if (entry->subFolderStatus == 2) {
                    folderStatus = 2;
                } else if (entry->subFolderStatus == 1 && folderStatus == 0) {
                    folderStatus = 1;
                }
            }
        } while (FindNextFileW(hFind, findData));
        FindClose(hFind);
    }
    
    if (entryCount > 0) {
        qsort(entries, entryCount, sizeof(FontEntry), CompareFontEntries);
        
        for (int i = 0; i < entryCount; i++) {
            FontEntry* entry = &entries[i];
            
            if (!entry->is_dir) {
                AppendMenuW(parentMenu, MF_STRING | (entry->isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                          (*fontId)++, entry->displayName);
            } else {
                if (entry->subFolderStatus == 0) {
                    AppendMenuW(entry->hSubMenu, MF_STRING | MF_GRAYED, 0, L"(Empty folder)");
                    AppendMenuW(parentMenu, MF_POPUP, (UINT_PTR)entry->hSubMenu, entry->name);
                } else {
                    UINT folderFlags = MF_POPUP;
                    if (entry->subFolderStatus == 2) {
                        folderFlags |= MF_CHECKED;
                    }
                    AppendMenuW(parentMenu, folderFlags, (UINT_PTR)entry->hSubMenu, entry->name);
                }
            }
        }
    }
    
    free(entries);
    free(wFolderPath);
    free(wSearchPath);
    free(findData);

    return folderStatus;
}

/**
 * @brief Build font submenu with recursive folder scanning
 * @param hMenu Parent menu handle
 */
void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    
    int g_advancedFontId = 2000;
    
    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        // Try to use cached font list (FAST PATH - 5-10ms)
        const FontCacheEntry* cachedFonts = NULL;
        int cachedCount = 0;
        FontCacheStatus cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
        
        if (cacheStatus == FONT_CACHE_OK || cacheStatus == FONT_CACHE_EXPIRED) {
            // Check if we have any subfolders (depth > 0)
            BOOL hasSubfolders = FALSE;
            for (int i = 0; i < cachedCount; i++) {
                if (cachedFonts[i].depth > 0) {
                    hasSubfolders = TRUE;
                    break;
                }
            }

            // If we have subfolders, we MUST use the slow path to preserve hierarchy
            // because the cache is flattened.
            if (!hasSubfolders) {
                // Use cached data to build menu quickly (Fast Path)
                WriteLog(LOG_LEVEL_INFO, "Using cached font list (%d fonts, status=%d)", cachedCount, cacheStatus);
                
                g_advancedFontId = 2000;
                for (int i = 0; i < cachedCount; i++) {
                    UINT flags = MF_STRING;
                    if (cachedFonts[i].isCurrentFont) {
                        flags |= MF_CHECKED;
                    }
                    AppendMenuW(hFontSubMenu, flags, g_advancedFontId++, cachedFonts[i].displayName);
                }
                
                if (cachedCount > 0) {
                    AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
                }
                
                // Trigger async refresh if cache expired
                if (cacheStatus == FONT_CACHE_EXPIRED) {
                    ResourceCache_RequestRefresh();
                }
                
                goto menu_built;
            } else {
                WriteLog(LOG_LEVEL_INFO, "Subfolders detected in cache, falling back to sync scan to preserve hierarchy");
            }
        } 
        
        // Fallback: use original synchronous scan (SLOW PATH - 100-300ms)
        if (cacheStatus != FONT_CACHE_OK && cacheStatus != FONT_CACHE_EXPIRED) {
             WriteLog(LOG_LEVEL_INFO, "Font cache not ready (status=%d), using fallback sync scan", cacheStatus);
        }
            
        wchar_t wFontsFolder[MAX_PATH] = {0};
        if (GetFontsFolderWideFromConfig(wFontsFolder, MAX_PATH)) {
            char fontsFolderPathUtf8[MAX_PATH];
            WideToUtf8(wFontsFolder, fontsFolderPathUtf8, MAX_PATH);
            
            g_advancedFontId = 2000;
            
            /* Resolve current font path ONCE */
            wchar_t wCurrentFontAbsPath[MAX_PATH] = {0};
            char currentFontAbsPathUtf8[MAX_PATH] = {0};
            const char* relPath = ExtractRelativePath(FONT_FILE_NAME);
            if (relPath) {
                BuildFullFontPath(relPath, currentFontAbsPathUtf8, MAX_PATH);
            } else {
                ExpandEnvironmentStringsA(FONT_FILE_NAME, currentFontAbsPathUtf8, MAX_PATH);
                /* If result is not absolute path (no drive separator), assume it is in fonts folder */
                if (!strchr(currentFontAbsPathUtf8, ':')) {
                    char tempPath[MAX_PATH];
                    strncpy(tempPath, currentFontAbsPathUtf8, MAX_PATH - 1);
                    BuildFullFontPath(tempPath, currentFontAbsPathUtf8, MAX_PATH);
                }
            }
            Utf8ToWide(currentFontAbsPathUtf8, wCurrentFontAbsPath, MAX_PATH);
            
            int fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId, wCurrentFontAbsPath);
            if (fontFolderStatus == 0) {
                WriteLog(LOG_LEVEL_INFO, "Font scan failed, checking known fonts...");
                wchar_t wTestFontPath[MAX_PATH];
                Utf8ToWide(fontsFolderPathUtf8, wTestFontPath, MAX_PATH - 32);
                wcscat(wTestFontPath, L"\\Wallpoet Essence.ttf");
                DWORD attribs = GetFileAttributesW(wTestFontPath);
                if (attribs != INVALID_FILE_ATTRIBUTES) {
                    WriteLog(LOG_LEVEL_WARNING, "Wallpoet Essence.ttf exists but scan failed!");
                } else {
                    WriteLog(LOG_LEVEL_INFO, "Wallpoet Essence.ttf does not exist");
                }
            }
            WriteLog(LOG_LEVEL_INFO, "Font folder scan result: %d", fontFolderStatus);
            
            if (fontFolderStatus == 0) {
                HINSTANCE hInst = GetModuleHandle(NULL);
                if (ExtractEmbeddedFontsToFolder(hInst)) {
                    fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId, wCurrentFontAbsPath);
                }
            }

            if (fontFolderStatus == 0) {
                AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, 
                           GetLocalizedString(NULL, L"No font files found"));
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            } else {
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
        }
        
menu_built:
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED, 
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(NULL, L"Font"));
}
