/**
 * @file dialog_font_picker.c
 * @brief System font picker implementation
 */

#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "../../resource/resource.h"
#include "config.h"
#include "font.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "window/window_core.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <shlobj.h>

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];

typedef struct {
    char originalFontName[MAX_PATH];
    char originalFileName[MAX_PATH];
    char originalRuntimeFileName[MAX_PATH];
    BOOL closeHandled;
} FontDialogState;

static FontDialogState g_fontState = {0};

static BOOL WideFontPathToUtf8(const wchar_t* fontPath, char* outPath, size_t outPathSize) {
    if (!fontPath || !outPath || outPathSize == 0 || outPathSize > INT_MAX) {
        return FALSE;
    }

    outPath[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > outPathSize) {
        return FALSE;
    }

    return WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, outPath,
                               (int)outPathSize, NULL, NULL) > 0;
}

static BOOL ShouldStopFontEnumeration(HANDLE stopEvent);

static void SaveOriginalFont(void) {
    strncpy(g_fontState.originalFontName, FONT_INTERNAL_NAME, sizeof(g_fontState.originalFontName) - 1);
    g_fontState.originalFontName[sizeof(g_fontState.originalFontName) - 1] = '\0';

    strncpy(g_fontState.originalFileName, FONT_FILE_NAME, sizeof(g_fontState.originalFileName) - 1);
    g_fontState.originalFileName[sizeof(g_fontState.originalFileName) - 1] = '\0';
    strncpy(g_fontState.originalRuntimeFileName, FONT_RUNTIME_FILE_NAME,
            sizeof(g_fontState.originalRuntimeFileName) - 1);
    g_fontState.originalRuntimeFileName[sizeof(g_fontState.originalRuntimeFileName) - 1] = '\0';
    g_fontState.closeHandled = FALSE;
}

static void RestoreOriginalFont(void) {
    HWND hwnd = FindCurrentProcessMainWindow();
    CancelPreview(hwnd);

    strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';

    strncpy(FONT_FILE_NAME, g_fontState.originalFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    strncpy(FONT_RUNTIME_FILE_NAME, g_fontState.originalRuntimeFileName,
            sizeof(FONT_RUNTIME_FILE_NAME) - 1);
    FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';

    HINSTANCE hInstance = GetModuleHandleW(NULL);
    const char* loadFontName = FONT_RUNTIME_FILE_NAME;
    if (IsFontsFolderPath(FONT_RUNTIME_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_RUNTIME_FILE_NAME);
        if (relativePath) {
            loadFontName = relativePath;
        }
    }

    if (loadFontName && loadFontName[0] &&
        !LoadFontByNameAndGetRealName(hInstance, loadFontName,
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }

    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
        RestoreWindowVisibility(hwnd);
    }
}

static BOOL GetSystemFontPath(const wchar_t* fontName, char* outPath,
                              size_t outPathSize, HANDLE stopEvent) {
    if (ShouldStopFontEnumeration(stopEvent)) {
        return FALSE;
    }

    /* Get Windows Fonts directory */
    static wchar_t fontsDir[MAX_PATH] = {0};
    static BOOL fontsDirInitialized = FALSE;

    if (!fontsDirInitialized) {
        if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
            LOG_WARNING("FontPath: Failed to get system fonts directory, using fallback");
            wcscpy_s(fontsDir, MAX_PATH, L"C:\\Windows\\Fonts");
        }
        fontsDirInitialized = TRUE;
    }

    /* Build font file path - try common patterns */
    wchar_t fontPath[MAX_PATH];
    const wchar_t* extensions[] = {L".ttf", L".otf", L".ttc"};

    wchar_t lowerName[MAX_PATH];
    if (wcscpy_s(lowerName, MAX_PATH, fontName) != 0 ||
        _wcslwr_s(lowerName, MAX_PATH) != 0) {
        return FALSE;
    }

    wchar_t noSpace[MAX_PATH];
    const wchar_t* src = lowerName;
    wchar_t* dst = noSpace;
    while (*src) {
        if (*src != L' ') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = L'\0';

    for (int i = 0; i < 3; i++) {
        if (ShouldStopFontEnumeration(stopEvent)) {
            return FALSE;
        }

        /* Try direct name match (e.g., "Arial" -> "Arial.ttf") */
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, fontName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            if (!WideFontPathToUtf8(fontPath, outPath, outPathSize)) {
                return FALSE;
            }
            return TRUE;
        }

        /* Try lowercase (e.g., "Arial" -> "arial.ttf") */
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, lowerName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            if (!WideFontPathToUtf8(fontPath, outPath, outPathSize)) {
                return FALSE;
            }
            return TRUE;
        }

        /* Try without spaces (e.g., "Times New Roman" -> "times.ttf") */
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, noSpace, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            if (!WideFontPathToUtf8(fontPath, outPath, outPathSize)) {
                return FALSE;
            }
            return TRUE;
        }
    }

    /* Only log variable font variants at debug level to reduce noise */
    if (wcsstr(fontName, L"Light") || wcsstr(fontName, L"SemiLight") ||
        wcsstr(fontName, L"SemiBold") || wcsstr(fontName, L"ExtraLight")) {
        /* Likely a variable font variant that doesn't have a separate file */
        return FALSE;
    }

    return FALSE;
}

static BOOL PreviewFontInMainWindow(const wchar_t* fontName, const char* cachedFontPath,
                                    HWND hdlg, HWND hwndList) {
    char fontPath[MAX_PATH] = {0};

    if (cachedFontPath && cachedFontPath[0]) {
        strncpy(fontPath, cachedFontPath, sizeof(fontPath) - 1);
        fontPath[sizeof(fontPath) - 1] = '\0';
    } else if (!GetSystemFontPath(fontName, fontPath, sizeof(fontPath), NULL)) {
        LOG_ERROR("FontApply: ✗ Failed to locate font file for: %S", fontName);
        return FALSE;
    }

    HWND hwnd = FindCurrentProcessMainWindow();
    if (hwnd) {
        ShowWindowForPreview(hwnd);
        StartPreview(PREVIEW_TYPE_FONT, fontPath, hwnd);
        if (GetActivePreviewType() != PREVIEW_TYPE_FONT) {
            RestoreWindowVisibility(hwnd);
            return FALSE;
        }
    } else {
        LOG_WARNING("FontApply: Main window not found");
        return FALSE;
    }

    /* Restore focus to dialog listbox (main window InvalidateRect may steal focus) */
    if (hwndList && hdlg) {
        SetFocus(hwndList);
        InvalidateRect(hwndList, NULL, FALSE);
        UpdateWindow(hwndList);
    }

    return TRUE;
}

static BOOL CommitCurrentFontSelection(HWND hwnd) {
    if (GetActivePreviewType() != PREVIEW_TYPE_FONT) {
        if (hwnd) {
            RestoreWindowVisibility(hwnd);
        }
        return TRUE;
    }

    if (!ApplyPreview(hwnd)) {
        LOG_WARNING("FontPicker: failed to persist selected font preview");
        return FALSE;
    }

    if (hwnd) {
        RestoreWindowVisibility(hwnd);
    }
    return TRUE;
}

typedef struct {
    wchar_t fontName[LF_FACESIZE];
    char fontPath[MAX_PATH];
} FontMapEntry;

static FontMapEntry* g_fontMap = NULL;
static int g_fontMapCount = 0;
static int g_fontMapCapacity = 0;
static HANDLE g_fontEnumThread = NULL;
static HANDLE g_fontEnumStopEvent = NULL;
static BOOL g_fontListReady = FALSE;
static BOOL g_fontEnumRestartAfterCleanup = FALSE;
static volatile LONG g_fontEnumGeneration = 0;
static int g_currentFontIndex = -1;
static int g_previewFontIndex = -1;

typedef struct {
    HWND hdlg;
    HANDLE stopEvent;
    LONG generation;
} FontEnumerationThreadParams;

#define WM_APP_FONT_ENUM_COMPLETE (WM_APP + 410)
#define MAX_FONT_PICKER_ENTRIES 1024
#define FONT_ENUM_POLL_TIMER_ID 9997
#define FONT_PICKER_TOPMOST_TIMER_ID 9998
#define FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID 9996
#define FONT_ENUM_START_RETRY_TIMER_ID 9995
#define FONT_ENUM_STOP_WAIT_MS 250
#define FONT_ENUM_SHUTDOWN_WAIT_MS 2000
#define FONT_ENUM_POLL_INTERVAL_MS 50
#define FONT_ENUM_DEFERRED_CLEANUP_INTERVAL_MS 1000
#define FONT_ENUM_START_RETRY_INTERVAL_MS 1000

static BOOL ShouldStopFontEnumeration(HANDLE stopEvent) {
    return stopEvent &&
           WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0;
}

static void ResetFontMap(void) {
    if (g_fontMap) {
        free(g_fontMap);
        g_fontMap = NULL;
