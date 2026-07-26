/**
 * @file tray_menu_font_build.c
 * @brief Sorting and hierarchical construction of font menu entries.
 */

#include "tray_menu_font_internal.h"

#include "log.h"
#include "utils/natural_sort.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const FontEntry* entry;
} FontEntrySortItem;

static int CompareFontEntries(const void* a, const void* b) {
    const FontEntrySortItem* ea = a;
    const FontEntrySortItem* eb = b;
    return NaturalPathCompareW(ea->entry->relativePath, eb->entry->relativePath);
}

static BOOL IsFontEntryCurrent(const FontEntry* entry, const wchar_t* currentFontRelPath) {
    return entry &&
           currentFontRelPath &&
           currentFontRelPath[0] != L'\0' &&
           _wcsicmp(entry->relativePath, currentFontRelPath) == 0;
}

static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name, BOOL shouldCheck) {
    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_STATE;
    wchar_t buffer[MAX_PATH] = {0};

    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && _wcsicmp(buffer, name) == 0) {
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
    if (!AppendMenuW(hParent, flags, (UINT_PTR)hSub, name)) {
        DestroyMenu(hSub);
        return NULL;
    }
    return hSub;
}

void FontMenuInternal_BuildMenuFromEntries(HMENU hRootMenu, const FontEntry* entries, int count,
                                     const wchar_t* currentFontRelPath, int* fontId) {
    if (!entries || count <= 0 || count > MAX_FONT_ENTRIES) return;

    /* Sort entries for consistent display */
    FontEntrySortItem sortedEntries[MAX_FONT_ENTRIES];

    for (int i = 0; i < count; i++) {
        sortedEntries[i].entry = &entries[i];
    }

    qsort(sortedEntries, count, sizeof(sortedEntries[0]), CompareFontEntries);

    /* Collect parent directories of selected fonts */
    enum { MAX_PARENT_DIRS = 100 };
    wchar_t (*parentDirs)[MAX_PATH] = NULL;
    int parentDirCount = 0;

    for (int i = 0; i < count; i++) {
        if (IsFontEntryCurrent(sortedEntries[i].entry, currentFontRelPath)) {
            if (!parentDirs) {
                parentDirs = (wchar_t (*)[MAX_PATH])calloc(MAX_PARENT_DIRS,
                                                           sizeof(*parentDirs));
                if (!parentDirs) {
                    LOG_WARNING("Failed to allocate font menu parent directory buffer");
                    break;
                }
            }

            wchar_t pathCopy[MAX_PATH];
            wcsncpy(pathCopy, sortedEntries[i].entry->relativePath, MAX_PATH - 1);
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
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
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
        const FontEntry* entry = sortedEntries[i].entry;
        BOOL isCurrentFont = IsFontEntryCurrent(entry, currentFontRelPath);

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
                if (parentDirs) {
                    for (int j = 0; j < parentDirCount; j++) {
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
                            shouldCheck = TRUE;
                            break;
                        }
                    }
                }

                hCurrent = EnsureSubMenu(hCurrent, token, shouldCheck);
                if (!hCurrent) break;
            } else {
                UINT flags = MF_STRING;
                if (isCurrentFont) flags |= MF_CHECKED;
                UINT id = (UINT)(*fontId);
                if (hCurrent && FontMenuInternal_RememberId(id, entry->relativePath)) {
                    if (AppendMenuW(hCurrent, flags, id, entry->displayName)) {
                        (*fontId)++;
                    } else {
                        FontMenuInternal_ForgetLastId(id);
                    }
                }
            }

            token = nextToken;
        }
    }

    free(parentDirs);
}
