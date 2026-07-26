/**
 * @file tray_menu_font.c
 * @brief Public font-menu construction and command mapping.
 */

#include "tray_menu_font_internal.h"

#include "config.h"
#include "language.h"
#include "log.h"
#include "tray/tray_menu.h"
#include "../resource/resource.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if CMD_FONT_SELECTION_BASE + MAX_FONT_ENTRIES > CLOCK_IDM_ANIMATIONS_MENU
#error "Font menu command range overlaps animation menu identifiers"
#endif

#if CLOCK_IDM_SYSTEM_FONT_PICKER >= CLOCK_IDM_ANIMATIONS_BASE && CLOCK_IDM_SYSTEM_FONT_PICKER < CLOCK_IDM_ANIMATIONS_END
#error "System font picker menu ID overlaps dynamic animation menu command range"
#endif

extern char FONT_FILE_NAME[MAX_PATH];
extern BOOL NeedsFontLicenseVersionAcceptance(void);

typedef struct {
    UINT id;
    wchar_t relativePath[MAX_PATH];
} FontMenuIdMapEntry;

static FontMenuIdMapEntry g_fontMenuIdMap[MAX_FONT_ENTRIES];
static int g_fontMenuIdMapCount = 0;

static BOOL CopyStringExactW(const wchar_t* src, wchar_t* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = L'\0';
    if (!src) return FALSE;

    size_t srcLen = wcslen(src);
    if (srcLen >= outSize) return FALSE;

    memcpy(out, src, (srcLen + 1) * sizeof(wchar_t));
    return TRUE;
}

void FontMenuInternal_ResetIdMap(void) {
    ZeroMemory(g_fontMenuIdMap, sizeof(g_fontMenuIdMap));
    g_fontMenuIdMapCount = 0;
}

BOOL FontMenuInternal_RememberId(UINT id, const wchar_t* relativePath) {
    if (!relativePath || g_fontMenuIdMapCount >= MAX_FONT_ENTRIES) return FALSE;

    FontMenuIdMapEntry entry = {0};
    entry.id = id;
    if (!CopyStringExactW(relativePath, entry.relativePath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Font menu path is too long: %ls", relativePath);
        return FALSE;
    }

    g_fontMenuIdMap[g_fontMenuIdMapCount] = entry;
    g_fontMenuIdMapCount++;
    return TRUE;
}

void FontMenuInternal_ForgetLastId(UINT id) {
    if (g_fontMenuIdMapCount <= 0) return;
    if (g_fontMenuIdMap[g_fontMenuIdMapCount - 1].id != id) return;

    g_fontMenuIdMapCount--;
    ZeroMemory(&g_fontMenuIdMap[g_fontMenuIdMapCount], sizeof(g_fontMenuIdMap[0]));
}

static void GetCurrentFontRelativePath(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0 || size > INT_MAX) return;
    outPath[0] = L'\0';

    const char* prefix = FONTS_PATH_PREFIX;
    size_t prefixLen = strlen(prefix);
    const char* source = NULL;

    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        /* Custom font - extract relative path */
        source = FONT_FILE_NAME + prefixLen;
    } else if (strchr(FONT_FILE_NAME, ':') == NULL &&
               (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL)) {
        /* Relative path without prefix */
        source = FONT_FILE_NAME;
    }

    if (source && MultiByteToWideChar(CP_UTF8, 0, source, -1, outPath, (int)size) <= 0) {
        outPath[0] = L'\0';
    }
    /* System fonts or just filename - leave empty */
}

static const wchar_t* GetPathBaseNameW(const wchar_t* path) {
    if (!path) return L"";

    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* forwardSlash = wcsrchr(path, L'/');
    if (!slash || (forwardSlash && forwardSlash > slash)) {
        slash = forwardSlash;
    }

    return slash ? slash + 1 : path;
}

void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    if (!hFontSubMenu) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create font submenu");
        return;
    }
    FontMenuInternal_ResetIdMap();

    int g_advancedFontId = CMD_FONT_SELECTION_BASE;

    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE,
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        FontMenu_RequestScanAsync();

        /* Get current font relative path */
        wchar_t currentFontRelPath[MAX_PATH] = L"";
        GetCurrentFontRelativePath(currentFontRelPath, MAX_PATH);

        BOOL cacheReady = FALSE;
        int fontCount = 0;
        BOOL isSystemFont = TRUE;
        FontEntry* fontSnapshot =
            (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*fontSnapshot));
        if (!fontSnapshot) {
            LOG_WARNING("Failed to allocate font menu cache snapshot");
        }

        AcquireSRWLockShared(&g_fontMenuCacheLock);
        cacheReady = g_fontMenuCacheReady || g_fontMenuCacheFailed;
        fontCount = g_fontMenuCacheCount;
        if (fontCount > MAX_FONT_ENTRIES) {
            fontCount = MAX_FONT_ENTRIES;
        }
        if (fontCount > 0 && fontSnapshot) {
            memcpy(fontSnapshot, g_fontMenuCache, (size_t)fontCount * sizeof(*fontSnapshot));
        } else if (fontCount > 0) {
            fontCount = 0;
            cacheReady = FALSE;
        }
        ReleaseSRWLockShared(&g_fontMenuCacheLock);

        if (fontCount == 0) {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0,
                        cacheReady
                            ? GetLocalizedString(NULL, L"No font files found")
                            : GetLocalizedString(NULL, L"Loading..."));
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        } else {
            FontMenuInternal_BuildMenuFromEntries(hFontSubMenu, fontSnapshot, fontCount,
                                     currentFontRelPath, &g_advancedFontId);
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        }

        /* Determine if current font is a system font */
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
            wchar_t wFontName[MAX_PATH] = L"";
            if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFontName, MAX_PATH) > 0 &&
                wFontName[0] != L'\0') {
                for (int i = 0; i < fontCount; i++) {
                    if (_wcsicmp(GetPathBaseNameW(fontSnapshot[i].relativePath), wFontName) == 0) {
                        isSystemFont = FALSE;
                        break;
                    }
                }
            }
        }
        free(fontSnapshot);

        UINT systemFontFlags = MF_STRING;
        if (isSystemFont) systemFontFlags |= MF_CHECKED;

        AppendMenuW(hFontSubMenu, systemFontFlags, CLOCK_IDM_SYSTEM_FONT_PICKER,
                   GetLocalizedString(NULL, L"System Fonts..."));

        AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED,
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu,
                     GetLocalizedString(NULL, L"Font"))) {
        DestroyMenu(hFontSubMenu);
        WriteLog(LOG_LEVEL_ERROR, "Failed to attach font submenu");
    }
}

BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';
    if (outPathSize > INT_MAX) return FALSE;
    if (id < CMD_FONT_SELECTION_BASE) return FALSE;

    for (int i = 0; i < g_fontMenuIdMapCount; i++) {
        if (g_fontMenuIdMap[i].id == id) {
            if (WideCharToMultiByte(CP_UTF8, 0, g_fontMenuIdMap[i].relativePath, -1,
                                    outPath, (int)outPathSize, NULL, NULL) <= 0) {
                outPath[0] = '\0';
                return FALSE;
            }
            return TRUE;
        }
    }

    return FALSE;
}
