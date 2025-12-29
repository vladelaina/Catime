/**
 * @file tray_menu_font.c
 * @brief Font scanning and menu construction logic (direct scan, no cache)
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
#include "font/font_path_manager.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_FONT_ENTRIES 200
#define MAX_RECURSION_DEPTH 10
#define MAX_FONT_NAME_LENGTH 260

/* ============================================================================
 * External dependencies
 * ============================================================================ */

extern char FONT_FILE_NAME[MAX_PATH];
extern void GetConfigPath(char* path, size_t size);
extern BOOL NeedsFontLicenseVersionAcceptance(void);
extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Single font entry for menu building
 */
typedef struct {
    wchar_t fileName[MAX_FONT_NAME_LENGTH];
    wchar_t relativePath[MAX_PATH];
    wchar_t displayName[MAX_FONT_NAME_LENGTH];
    BOOL isCurrentFont;
} FontEntry;

/**
 * @brief Font scan context
 */
typedef struct {
    FontEntry* entries;
    int count;
    int capacity;
    wchar_t currentFontRelPath[MAX_PATH];
} FontScanContext;

/* ============================================================================
 * Path Helpers
 * ============================================================================ */

/**
 * @brief Get fonts folder path
 */
static BOOL GetFontsFolderPath(wchar_t* outPath, size_t size) {
    return GetFontsFolderW(outPath, size, FALSE);
}

/**
 * @brief Get current font relative path for matching
 */
static void GetCurrentFontRelativePath(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0) return;
    outPath[0] = L'\0';
    
    const char* prefix = FONTS_PATH_PREFIX;
    size_t prefixLen = strlen(prefix);
    
    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        /* Custom font - extract relative path */
        MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME + prefixLen, -1, outPath, (int)size);
    } else if (strchr(FONT_FILE_NAME, ':') == NULL && 
               (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL)) {
        /* Relative path without prefix */
        MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, outPath, (int)size);
    }
    /* System fonts or just filename - leave empty */
}

/* ============================================================================
 * Font Entry Management
 * ============================================================================ */

/**
 * @brief Add font entry to context
 */
static BOOL AddFontEntry(FontScanContext* ctx, const wchar_t* fileName,
                         const wchar_t* relativePath) {
    if (ctx->count >= ctx->capacity) {
        WriteLog(LOG_LEVEL_WARNING, "Font list capacity reached (%d), skipping: %ls",
                 ctx->capacity, fileName);
        return FALSE;
    }
    
    FontEntry* entry = &ctx->entries[ctx->count];
    wcsncpy(entry->fileName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->fileName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    
    wcsncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = L'\0';
    
    /* Display name = filename without extension */
    wcsncpy(entry->displayName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->displayName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
    if (dotPos) *dotPos = L'\0';
    
    /* Check if this is the current font */
    entry->isCurrentFont = (_wcsicmp(relativePath, ctx->currentFontRelPath) == 0);
    
    ctx->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan font folder
 */
static void ScanFontFolderRecursive(const wchar_t* folderPath,
                                    const wchar_t* relativePath,
                                    FontScanContext* ctx,
                                    int depth) {
    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }
    
    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) return;
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        wchar_t fullPath[MAX_PATH];
        int len1 = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0 || len1 >= MAX_PATH) continue;
        
        wchar_t newRelativePath[MAX_PATH];
        if (!relativePath || relativePath[0] == L'\0') {
            wcsncpy(newRelativePath, findData.cFileName, MAX_PATH - 1);
            newRelativePath[MAX_PATH - 1] = L'\0';
        } else {
            int len2 = _snwprintf_s(newRelativePath, MAX_PATH, _TRUNCATE, L"%s\\%s", relativePath, findData.cFileName);
            if (len2 < 0 || len2 >= MAX_PATH) continue;
        }
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanFontFolderRecursive(fullPath, newRelativePath, ctx, depth + 1);
        } else {
            wchar_t* ext = wcsrchr(findData.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                AddFontEntry(ctx, findData.cFileName, newRelativePath);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

/**
 * @brief Scan fonts folder and return entries
 */
static int ScanFontsFolder(FontEntry* entries, int capacity, const wchar_t* currentFontRelPath) {
    FontScanContext ctx = {0};
    ctx.entries = entries;
    ctx.count = 0;
    ctx.capacity = capacity;
    wcsncpy(ctx.currentFontRelPath, currentFontRelPath, MAX_PATH - 1);
    ctx.currentFontRelPath[MAX_PATH - 1] = L'\0';
    
    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderPath(fontsPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get fonts folder path");
        return 0;
    }
    
    DWORD attribs = GetFileAttributesW(fontsPath);
    if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Fonts folder does not exist: %ls", fontsPath);
        return 0;
    }
    
    ScanFontFolderRecursive(fontsPath, L"", &ctx, 0);
    
    WriteLog(LOG_LEVEL_INFO, "Font scan complete: %d fonts found", ctx.count);
    return ctx.count;
}

/* ============================================================================
 * Menu Building
 * ============================================================================ */

/**
 * @brief Comparator for font entries (by relative path)
 */
static int CompareFontEntries(const void* a, const void* b) {
    const FontEntry* ea = *(const FontEntry**)a;
    const FontEntry* eb = *(const FontEntry**)b;
    return NaturalPathCompareW(ea->relativePath, eb->relativePath);
}

/**
 * @brief Find or create a submenu with the given name
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
                if (shouldCheck && !(mii.fState & MFS_CHECKED)) {
                    mii.fState |= MFS_CHECKED;
                    SetMenuItemInfoW(hParent, i, TRUE, &mii);
                }
                return mii.hSubMenu;
            }
        }
    }
    
    HMENU hSub = CreatePopupMenu();
    if (!hSub) return NULL;
    
    UINT flags = MF_POPUP;
    if (shouldCheck) flags |= MF_CHECKED;
    AppendMenuW(hParent, flags, (UINT_PTR)hSub, name);
    return hSub;
}

/**
 * @brief Build font menu from scanned entries
 */
static void BuildFontMenuFromEntries(HMENU hRootMenu, FontEntry* entries, int count, int* fontId) {
    /* Sort entries for consistent display */
    FontEntry** sortedEntries = (FontEntry**)malloc(count * sizeof(FontEntry*));
    if (!sortedEntries) return;
    
    for (int i = 0; i < count; i++) {
        sortedEntries[i] = &entries[i];
    }
    
    qsort(sortedEntries, count, sizeof(FontEntry*), CompareFontEntries);
    
    /* Collect parent directories of selected fonts */
    #define MAX_PARENT_DIRS 100
    wchar_t parentDirs[MAX_PARENT_DIRS][MAX_PATH];
    int parentDirCount = 0;
    
    for (int i = 0; i < count; i++) {
        if (sortedEntries[i]->isCurrentFont) {
            wchar_t pathCopy[MAX_PATH];
            wcsncpy(pathCopy, sortedEntries[i]->relativePath, MAX_PATH - 1);
            pathCopy[MAX_PATH - 1] = L'\0';
            
            wchar_t currentPath[MAX_PATH] = L"";
            wchar_t* context = NULL;
            wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
            
            while (token) {
                wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
                if (nextToken) {
                    size_t currentLen = wcslen(currentPath);
                    size_t tokenLen = wcslen(token);
                    
                    if (currentLen + tokenLen + 2 < MAX_PATH) {
                        if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                        wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                    }
                    
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
    
    /* Build menu tree */
    for (int i = 0; i < count; i++) {
        FontEntry* entry = sortedEntries[i];
        
        wchar_t pathCopy[MAX_PATH];
        wcsncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = L'\0';
        
        wchar_t* context = NULL;
        wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
        
        HMENU hCurrent = hRootMenu;
        wchar_t currentPath[MAX_PATH] = L"";
        
        while (token) {
            wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
            
            if (nextToken) {
                size_t currentLen = wcslen(currentPath);
                size_t tokenLen = wcslen(token);
                
                if (currentLen + tokenLen + 2 < MAX_PATH) {
                    if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                    wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                }
                
                BOOL shouldCheck = FALSE;
                for (int j = 0; j < parentDirCount; j++) {
                    if (wcscmp(parentDirs[j], currentPath) == 0) {
                        shouldCheck = TRUE;
                        break;
                    }
                }
                
                hCurrent = EnsureSubMenu(hCurrent, token, shouldCheck);
                if (!hCurrent) break;
            } else {
                UINT flags = MF_STRING;
                if (entry->isCurrentFont) flags |= MF_CHECKED;
                AppendMenuW(hCurrent, flags, (*fontId)++, entry->displayName);
            }
            
            token = nextToken;
        }
    }
    
    free(sortedEntries);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Build font submenu with direct folder scanning
 */
void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    if (!hFontSubMenu) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create font submenu");
        return;
    }
    
    int g_advancedFontId = CMD_FONT_SELECTION_BASE;
    
    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        /* Get current font relative path */
        wchar_t currentFontRelPath[MAX_PATH] = L"";
        GetCurrentFontRelativePath(currentFontRelPath, MAX_PATH);
        
        /* Scan fonts folder directly */
        FontEntry* entries = (FontEntry*)malloc(MAX_FONT_ENTRIES * sizeof(FontEntry));
        if (!entries) {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, L"(Memory error)");
        } else {
            int fontCount = ScanFontsFolder(entries, MAX_FONT_ENTRIES, currentFontRelPath);
            
            if (fontCount == 0) {
                AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, 
                           GetLocalizedString(NULL, L"No font files found"));
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            } else {
                BuildFontMenuFromEntries(hFontSubMenu, entries, fontCount, &g_advancedFontId);
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
            
            /* Determine if current font is a system font */
            BOOL isSystemFont = TRUE;
            const char* prefix = FONTS_PATH_PREFIX;
            size_t prefixLen = strlen(prefix);
            
            if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
                isSystemFont = FALSE;
            } else if (strchr(FONT_FILE_NAME, ':') != NULL) {
                isSystemFont = (strstr(FONT_FILE_NAME, "Windows\\Fonts") != NULL || 
                               strstr(FONT_FILE_NAME, "WINDOWS\\Fonts") != NULL);
            } else if (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL) {
                isSystemFont = FALSE;
            } else {
                /* Just filename - check if in scanned fonts */
                wchar_t wFontName[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFontName, MAX_PATH);
                
                for (int i = 0; i < fontCount; i++) {
                    if (wcsstr(entries[i].relativePath, wFontName) != NULL) {
                        isSystemFont = FALSE;
                        break;
                    }
                }
            }
            
            free(entries);
            
            UINT systemFontFlags = MF_STRING;
            if (isSystemFont) systemFontFlags |= MF_CHECKED;
            
            AppendMenuW(hFontSubMenu, systemFontFlags, CLOCK_IDM_SYSTEM_FONT_PICKER,
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
 * @brief Get font path from menu ID by re-scanning folder
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    if (id < CMD_FONT_SELECTION_BASE) return FALSE;
    
    /* Scan fonts folder */
    FontEntry* entries = (FontEntry*)malloc(MAX_FONT_ENTRIES * sizeof(FontEntry));
    if (!entries) return FALSE;
    
    int fontCount = ScanFontsFolder(entries, MAX_FONT_ENTRIES, L"");
    if (fontCount == 0) {
        free(entries);
        return FALSE;
    }
    
    /* Sort to match menu order */
    FontEntry** sortedEntries = (FontEntry**)malloc(fontCount * sizeof(FontEntry*));
    if (!sortedEntries) {
        free(entries);
        return FALSE;
    }
    
    for (int i = 0; i < fontCount; i++) {
        sortedEntries[i] = &entries[i];
    }
    
    qsort(sortedEntries, fontCount, sizeof(FontEntry*), CompareFontEntries);
    
    /* Find entry at given ID */
    BOOL found = FALSE;
    int currentId = CMD_FONT_SELECTION_BASE;
    
    for (int i = 0; i < fontCount; i++) {
        if (currentId == (int)id) {
            WideCharToMultiByte(CP_UTF8, 0, sortedEntries[i]->relativePath, -1, 
                               outPath, (int)outPathSize, NULL, NULL);
            found = TRUE;
            break;
        }
        currentId++;
    }
    
    free(sortedEntries);
    free(entries);
    
    return found;
}
