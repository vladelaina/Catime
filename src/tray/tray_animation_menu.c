/**
 * @file tray_animation_menu.c
 * @brief Animation menu implementation (direct scan, no cache)
 */

#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "log.h"
#include "language.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_ANIM_ENTRIES 200
#define MAX_RECURSION_DEPTH 10
#define MAX_ANIM_NAME_LENGTH 260

/* ============================================================================
 * Forward declarations
 * ============================================================================ */

BOOL SetCurrentAnimationName(const char* name);

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Single animation entry for menu building
 */
typedef struct {
    char fileName[MAX_ANIM_NAME_LENGTH];
    char relativePath[MAX_PATH];
    BOOL isSpecial;
} AnimEntry;

/**
 * @brief Animation scan context
 */
typedef struct {
    AnimEntry* entries;
    int count;
    int capacity;
} AnimScanContext;

/* ============================================================================
 * Path Helpers
 * ============================================================================ */

/**
 * @brief Get animations folder path (wide char)
 */
static BOOL GetAnimationsFolderPathW(wchar_t* outPath, size_t size) {
    char utf8Path[MAX_PATH];
    GetAnimationsFolderPath(utf8Path, MAX_PATH);
    
    if (utf8Path[0] == '\0') return FALSE;
    
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, outPath, (int)size);
    return TRUE;
}

/* ============================================================================
 * Animation Entry Management
 * ============================================================================ */

/**
 * @brief Check if file is an animation file
 */
static BOOL IsAnimationFile(const wchar_t* fileName) {
    wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext) return FALSE;
    
    return _wcsicmp(ext, L".gif") == 0 ||
           _wcsicmp(ext, L".webp") == 0 ||
           _wcsicmp(ext, L".png") == 0 ||
           _wcsicmp(ext, L".jpg") == 0 ||
           _wcsicmp(ext, L".jpeg") == 0 ||
           _wcsicmp(ext, L".bmp") == 0;
}

/**
 * @brief Add animation entry to context
 */
static BOOL AddAnimEntry(AnimScanContext* ctx, const char* fileName,
                         const char* relativePath, BOOL isSpecial) {
    if (ctx->count >= ctx->capacity) {
        WriteLog(LOG_LEVEL_WARNING, "Animation list capacity reached (%d), skipping: %s",
                 ctx->capacity, fileName);
        return FALSE;
    }
    
    AnimEntry* entry = &ctx->entries[ctx->count];
    strncpy(entry->fileName, fileName, MAX_ANIM_NAME_LENGTH - 1);
    entry->fileName[MAX_ANIM_NAME_LENGTH - 1] = '\0';
    
    strncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = '\0';
    
    entry->isSpecial = isSpecial;
    
    ctx->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan animations folder
 */
static void ScanAnimationFolderRecursive(const wchar_t* folderPath,
                                          const char* relativePathUtf8,
                                          AnimScanContext* ctx,
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
        
        char fileNameUtf8[MAX_ANIM_NAME_LENGTH];
        WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1,
                           fileNameUtf8, MAX_ANIM_NAME_LENGTH, NULL, NULL);
        
        char newRelativePath[MAX_PATH];
        if (relativePathUtf8[0] == '\0') {
            strncpy(newRelativePath, fileNameUtf8, MAX_PATH - 1);
        } else {
            snprintf(newRelativePath, MAX_PATH, "%s\\%s", relativePathUtf8, fileNameUtf8);
        }
        newRelativePath[MAX_PATH - 1] = '\0';
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanAnimationFolderRecursive(fullPath, newRelativePath, ctx, depth + 1);
        } else {
            if (IsAnimationFile(findData.cFileName)) {
                AddAnimEntry(ctx, fileNameUtf8, newRelativePath, FALSE);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

/**
 * @brief Scan animations folder and return entries
 */
static int ScanAnimationsFolder(AnimEntry* entries, int capacity) {
    AnimScanContext ctx = {0};
    ctx.entries = entries;
    ctx.count = 0;
    ctx.capacity = capacity;
    
    wchar_t animPath[MAX_PATH];
    if (!GetAnimationsFolderPathW(animPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get animations folder path");
        return 0;
    }
    
    DWORD attribs = GetFileAttributesW(animPath);
    if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Animations folder does not exist: %ls", animPath);
        return 0;
    }
    
    ScanAnimationFolderRecursive(animPath, "", &ctx, 0);
    
    WriteLog(LOG_LEVEL_INFO, "Animation scan complete: %d animations found", ctx.count);
    return ctx.count;
}

/* ============================================================================
 * Menu Building
 * ============================================================================ */

/**
 * @brief Comparator for animation entries (by relative path)
 */
static int CompareAnimEntries(const void* a, const void* b) {
    const AnimEntry* ea = *(const AnimEntry**)a;
    const AnimEntry* eb = *(const AnimEntry**)b;
    return NaturalPathCompareA(ea->relativePath, eb->relativePath);
}

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
    
    HMENU hSub = CreatePopupMenu();
    AppendMenuW(hParent, MF_POPUP, (UINT_PTR)hSub, name);
    return hSub;
}

/**
 * @brief Build menu from scanned entries
 */
static void BuildAnimationMenuFromEntries(HMENU hRootMenu, AnimEntry* entries, int count, 
                                           const char* currentAnim, UINT* nextId) {
    /* Sort entries for consistent display */
    AnimEntry** sortedEntries = (AnimEntry**)malloc(count * sizeof(AnimEntry*));
    if (!sortedEntries) return;
    
    for (int i = 0; i < count; i++) {
        sortedEntries[i] = &entries[i];
    }
    
    qsort(sortedEntries, count, sizeof(AnimEntry*), CompareAnimEntries);
    
    for (int i = 0; i < count; i++) {
        AnimEntry* entry = sortedEntries[i];
        
        if (entry->isSpecial) continue;
        
        /* Parse relative path to find parent menu */
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
                hCurrent = EnsureSubMenu(hCurrent, wName);
            } else {
                UINT flags = MF_STRING;
                if (currentAnim && strcmp(currentAnim, entry->relativePath) == 0) {
                    flags |= MF_CHECKED;
                }
                
                if (*nextId < CLOCK_IDM_ANIMATIONS_END) {
                    AppendMenuW(hCurrent, flags, (*nextId)++, wName);
                }
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
 * @brief Build animation menu with direct folder scanning
 */
void BuildAnimationMenu(HMENU hMenu, const char* currentAnimationName) {
    if (!hMenu) return;
    
    /* Builtin options */
    int builtinCount = 0;
    const BuiltinAnimDef* builtins = GetBuiltinAnims(&builtinCount);
    
    for (int i = 0; i < builtinCount; i++) {
        UINT flags = MF_STRING;
        if (currentAnimationName && _stricmp(currentAnimationName, builtins[i].name) == 0) {
            flags |= MF_CHECKED;
        }
        
        const wchar_t* translatedLabel = GetLocalizedString(NULL, builtins[i].menuLabel);
        AppendMenuW(hMenu, flags, builtins[i].menuId, translatedLabel);
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    /* Scan animations folder directly */
    AnimEntry* entries = (AnimEntry*)malloc(MAX_ANIM_ENTRIES * sizeof(AnimEntry));
    if (!entries) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(Memory error)");
        return;
    }
    
    int animCount = ScanAnimationsFolder(entries, MAX_ANIM_ENTRIES);
    
    if (animCount > 0) {
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        BuildAnimationMenuFromEntries(hMenu, entries, animCount, currentAnimationName, &nextId);
    }
    
    free(entries);
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
        /* Scan folder to find animation at this ID */
        AnimEntry* entries = (AnimEntry*)malloc(MAX_ANIM_ENTRIES * sizeof(AnimEntry));
        if (!entries) return FALSE;
        
        int animCount = ScanAnimationsFolder(entries, MAX_ANIM_ENTRIES);
        if (animCount == 0) {
            free(entries);
            return FALSE;
        }
        
        /* Sort to match menu order */
        AnimEntry** sortedEntries = (AnimEntry**)malloc(animCount * sizeof(AnimEntry*));
        if (!sortedEntries) {
            free(entries);
            return FALSE;
        }
        
        for (int i = 0; i < animCount; i++) {
            sortedEntries[i] = &entries[i];
        }
        
        qsort(sortedEntries, animCount, sizeof(AnimEntry*), CompareAnimEntries);
        
        /* Map ID to entry */
        UINT currentId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL found = FALSE;
        
        for (int i = 0; i < animCount; i++) {
            if (sortedEntries[i]->isSpecial) continue;
            
            if (currentId == id) {
                SetCurrentAnimationName(sortedEntries[i]->relativePath);
                found = TRUE;
                break;
            }
            currentId++;
        }
        
        free(sortedEntries);
        free(entries);
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
 * @brief Get animation name from menu ID by re-scanning folder
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
        AnimEntry* entries = (AnimEntry*)malloc(MAX_ANIM_ENTRIES * sizeof(AnimEntry));
        if (!entries) return FALSE;
        
        int animCount = ScanAnimationsFolder(entries, MAX_ANIM_ENTRIES);
        if (animCount == 0) {
            free(entries);
            return FALSE;
        }
        
        AnimEntry** sortedEntries = (AnimEntry**)malloc(animCount * sizeof(AnimEntry*));
        if (!sortedEntries) {
            free(entries);
            return FALSE;
        }
        
        for (int i = 0; i < animCount; i++) {
            sortedEntries[i] = &entries[i];
        }
        
        qsort(sortedEntries, animCount, sizeof(AnimEntry*), CompareAnimEntries);
        
        UINT currentId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL found = FALSE;
        
        for (int i = 0; i < animCount; i++) {
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
        free(entries);
        return found;
    }
    
    return FALSE;
}
