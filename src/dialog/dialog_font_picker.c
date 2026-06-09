/**
 * @file dialog_font_picker.c
 * @brief System font picker implementation
 */

#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_common.h"
#include "../../resource/resource.h"
#include "config.h"
#include "font.h"
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
    g_fontState.closeHandled = FALSE;
}

static void RestoreOriginalFont(void) {
    strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';

    strncpy(FONT_FILE_NAME, g_fontState.originalFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';

    HINSTANCE hInstance = GetModuleHandleW(NULL);
    const char* loadFontName = FONT_FILE_NAME;
    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
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

    HWND hwnd = FindCurrentProcessMainWindow();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
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
            LOG_DEBUG("FontPath: ✓ '%S' -> %s", fontName, outPath);
            return TRUE;
        }
        
        /* Try lowercase (e.g., "Arial" -> "arial.ttf") */
        wchar_t lowerName[MAX_PATH];
        wcscpy_s(lowerName, MAX_PATH, fontName);
        _wcslwr_s(lowerName, MAX_PATH);
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, lowerName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            if (!WideFontPathToUtf8(fontPath, outPath, outPathSize)) {
                return FALSE;
            }
            LOG_DEBUG("FontPath: ✓ '%S' -> %s (lowercase)", fontName, outPath);
            return TRUE;
        }
        
        /* Try without spaces (e.g., "Times New Roman" -> "times.ttf") */
        wchar_t noSpace[MAX_PATH];
        const wchar_t* src = lowerName;
        wchar_t* dst = noSpace;
        while (*src) {
            if (*src != L' ') *dst++ = *src;
            src++;
        }
        *dst = L'\0';
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, noSpace, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            if (!WideFontPathToUtf8(fontPath, outPath, outPathSize)) {
                return FALSE;
            }
            LOG_DEBUG("FontPath: ✓ '%S' -> %s (no-space)", fontName, outPath);
            return TRUE;
        }
    }
    
    /* Only log variable font variants at debug level to reduce noise */
    if (wcsstr(fontName, L"Light") || wcsstr(fontName, L"SemiLight") || 
        wcsstr(fontName, L"SemiBold") || wcsstr(fontName, L"ExtraLight")) {
        /* Likely a variable font variant that doesn't have a separate file */
        return FALSE;
    }
    
    LOG_DEBUG("FontPath: ✗ Cannot resolve '%S' (may be variable font variant)", fontName);
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

    /* Load font and get internal name */
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    char internalName[MAX_PATH] = {0};
    if (!LoadFontByNameAndGetRealName(hInstance, fontPath, internalName, sizeof(internalName))) {
        LOG_WARNING("FontApply: LoadFontByNameAndGetRealName failed, using display name as fallback");
        if (!WideFontPathToUtf8(fontName, internalName, sizeof(internalName))) {
            LOG_ERROR("FontApply: Failed to convert fallback font name: %S", fontName);
            return FALSE;
        }
    }

    strncpy(FONT_FILE_NAME, fontPath, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    strncpy(FONT_INTERNAL_NAME, internalName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';

    HWND hwnd = FindCurrentProcessMainWindow();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        LOG_WARNING("FontApply: Main window not found");
    }
    
        /* Restore focus to dialog listbox (main window InvalidateRect may steal focus) */
    if (hwndList && hdlg) {
        SetFocus(hwndList);
        InvalidateRect(hwndList, NULL, TRUE);
        UpdateWindow(hwndList);
    }

    return TRUE;
}

static BOOL CommitCurrentFontSelection(void) {
    if (FONT_FILE_NAME[0] == '\0') {
        return FALSE;
    }

    if (strcmp(FONT_FILE_NAME, g_fontState.originalFileName) == 0 &&
        strcmp(FONT_INTERNAL_NAME, g_fontState.originalFontName) == 0) {
        return TRUE;
    }

    if (!WriteConfigFont(FONT_FILE_NAME, FALSE)) {
        LOG_WARNING("FontPicker: failed to persist selected font: %s", FONT_FILE_NAME);
        return FALSE;
    }
    if (!FlushConfigToDisk()) {
        LOG_WARNING("FontPicker: failed to flush selected font to disk: %s", FONT_FILE_NAME);
        return FALSE;
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
    }
    g_fontMapCount = 0;
    g_fontMapCapacity = 0;
}

static BOOL CleanupCompletedFontEnumeration(void) {
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("FontPicker: Failed to query font enumeration thread (error=%lu)",
                        GetLastError());
        }
        return FALSE;
    }

    CloseHandle(g_fontEnumThread);
    g_fontEnumThread = NULL;
    if (g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
    return TRUE;
}

static BOOL StopFontEnumerationWithTimeout(DWORD timeoutMs) {
    if (g_fontEnumThread || g_fontEnumStopEvent) {
        InterlockedIncrement(&g_fontEnumGeneration);
    }
    if (g_fontEnumStopEvent) {
        SetEvent(g_fontEnumStopEvent);
    }
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, timeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        CloseHandle(g_fontEnumThread);
        g_fontEnumThread = NULL;
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }
    if (waitResult == WAIT_TIMEOUT) {
        LOG_WARNING("FontPicker: Font enumeration stop timed out after %lu ms",
                    timeoutMs);
    } else {
        LOG_WARNING("FontPicker: Font enumeration stop wait failed (wait=%lu, error=%lu)",
                    waitResult, GetLastError());
    }
    return FALSE;
}

static BOOL StartFontEnumPollTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_POLL_TIMER_ID, FONT_ENUM_POLL_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration poll timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL StartFontEnumRetryTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID,
                  FONT_ENUM_START_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration retry timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static VOID CALLBACK FontEnumDeferredCleanupTimerProc(HWND hwnd, UINT msg,
                                                      UINT_PTR idEvent,
                                                      DWORD time) {
    (void)msg;
    (void)time;

    if (idEvent != FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID) {
        return;
    }

    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
        return;
    }

    if (!CleanupCompletedFontEnumeration()) {
        return;
    }

    KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    ResetFontMap();
    g_currentFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

static void ScheduleDeferredFontEnumerationCleanup(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (!hwndMain) {
        return;
    }

    if (!SetTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID,
                  FONT_ENUM_DEFERRED_CLEANUP_INTERVAL_MS,
                  FontEnumDeferredCleanupTimerProc)) {
        LOG_WARNING("FontPicker: Failed to schedule deferred enumeration cleanup (error=%lu)",
                    GetLastError());
    }
}

void CleanupSystemFontDialogResources(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (hwndMain) {
        KillTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    }

    BOOL enumStopped = StopFontEnumerationWithTimeout(FONT_ENUM_SHUTDOWN_WAIT_MS);
    if (!enumStopped) {
        LOG_WARNING("FontPicker: Font enumeration still running during shutdown; leaving resources to process teardown");
        return;
    }

    ResetFontMap();
    g_currentFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

static BOOL CheckFontHasRequiredGlyphs(HDC hdc, const wchar_t* fontName, HANDLE stopEvent) {
    if (ShouldStopFontEnumeration(stopEvent)) {
        return FALSE;
    }

    /* Required characters for countdown timer: 0-9 and colon */
    const wchar_t requiredChars[] = L"0123456789:";
    const int requiredCount = 11;

    LOG_DEBUG("GlyphCheck: Testing font '%S' for required glyphs", fontName);

    if (!hdc) {
        LOG_ERROR("GlyphCheck: ✗ Invalid DC for font '%S'", fontName);
        return FALSE;
    }

    /* Create font for testing */
    HFONT hFont = CreateFontW(
        -24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        VARIABLE_PITCH | FF_SWISS,
        fontName
    );

    if (!hFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to create test font for '%S'", fontName);
        return FALSE;
    }

    if (ShouldStopFontEnumeration(stopEvent)) {
        DeleteObject(hFont);
        return FALSE;
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    if (!oldFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to select test font for '%S'", fontName);
        DeleteObject(hFont);
        return FALSE;
    }

    /* Check if all required glyphs exist */
    WORD glyphIndices[11];
    DWORD result = GetGlyphIndicesW(hdc, requiredChars, requiredCount, glyphIndices, GGI_MARK_NONEXISTING_GLYPHS);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    if (result == GDI_ERROR) {
        LOG_ERROR("GlyphCheck: ✗ GetGlyphIndicesW failed for '%S'", fontName);
        return FALSE;
    }
    
    /* Check if any glyph is missing (marked as 0xFFFF) */
    int missingCount = 0;
    for (int i = 0; i < requiredCount; i++) {
        if (glyphIndices[i] == 0xFFFF) {
            LOG_DEBUG("GlyphCheck: Missing char '%lc' (U+%04X) in font '%S'", requiredChars[i], requiredChars[i], fontName);
            missingCount++;
        }
    }
    
    if (missingCount > 0) {
        LOG_DEBUG("GlyphCheck: ✗ Font '%S' missing %d required glyphs", fontName, missingCount);
        return FALSE;
    }
    
    LOG_DEBUG("GlyphCheck: ✓ Font '%S' has all required glyphs", fontName);
    return TRUE;
}

static BOOL AddOrUpdateFontMap(const wchar_t* fontName, const char* fontPath) {
    /* Check if this path already exists */
    for (int i = 0; i < g_fontMapCount; i++) {
        if (strcmp(g_fontMap[i].fontPath, fontPath) == 0) {
            /* Same file - keep the shorter name, or alphabetically first if same length */
            size_t newLen = wcslen(fontName);
            size_t existingLen = wcslen(g_fontMap[i].fontName);
            
            BOOL shouldReplace = newLen < existingLen || 
                                 (newLen == existingLen && _wcsicmp(fontName, g_fontMap[i].fontName) < 0);
            
            if (shouldReplace) {
                LOG_DEBUG("Font dedup: '%S' -> '%S' (same file: %s)",
                         g_fontMap[i].fontName, fontName, fontPath);
                wcscpy_s(g_fontMap[i].fontName, LF_FACESIZE, fontName);
            } else {
                LOG_DEBUG("Font dedup: keeping '%S' over '%S' (same file: %s)",
                         g_fontMap[i].fontName, fontName, fontPath);
            }
            return TRUE;
        }
    }

    /* New file - add to map */
    if (g_fontMapCount >= MAX_FONT_PICKER_ENTRIES) {
        LOG_WARNING("FontPicker: font entry limit reached (%d), stopping enumeration",
                    MAX_FONT_PICKER_ENTRIES);
        return FALSE;
    }

    if (g_fontMapCount >= g_fontMapCapacity) {
        if (g_fontMapCapacity > INT_MAX / 2) return FALSE;
        int newCapacity = g_fontMapCapacity <= 0 ? 256 : g_fontMapCapacity * 2;
        if (newCapacity > MAX_FONT_PICKER_ENTRIES) {
            newCapacity = MAX_FONT_PICKER_ENTRIES;
        }
        if ((size_t)newCapacity > ((size_t)-1) / sizeof(FontMapEntry)) return FALSE;
        FontMapEntry* newMap = (FontMapEntry*)realloc(g_fontMap, (size_t)newCapacity * sizeof(FontMapEntry));
        if (!newMap) return FALSE;
        g_fontMap = newMap;
        g_fontMapCapacity = newCapacity;
    }
    
    wcscpy_s(g_fontMap[g_fontMapCount].fontName, LF_FACESIZE, fontName);
    strncpy(g_fontMap[g_fontMapCount].fontPath, fontPath, MAX_PATH - 1);
    g_fontMap[g_fontMapCount].fontPath[MAX_PATH - 1] = '\0';
    g_fontMapCount++;
    return TRUE;
}

static int CALLBACK EnumFontFamiliesProc(const LOGFONTW* lpelf, const TEXTMETRICW* lpntm,
                                        DWORD fontType, LPARAM lParam) {
    (void)lpntm;

    if (lParam && WaitForSingleObject((HANDLE)lParam, 0) == WAIT_OBJECT_0) {
        return 0;
    }
    
    if (fontType & TRUETYPE_FONTTYPE) {
        const wchar_t* faceName = lpelf->lfFaceName;
        
        /* Skip vertical fonts */
        if (faceName[0] == L'@') {
            LOG_DEBUG("FontEnum: Skipping vertical font: %S", faceName);
            return 1;
        }
        
        /* Skip symbol fonts by charset */
        if (lpelf->lfCharSet == SYMBOL_CHARSET) {
            LOG_DEBUG("FontEnum: Skipping SYMBOL_CHARSET font: %S", faceName);
            return 1;
        }
        
        /* Blacklist of known incompatible fonts */
        const wchar_t* blacklist[] = {
            L"Symbol",
            L"Webdings",
            L"Wingdings",
            L"Wingdings 2",
            L"Wingdings 3",
            L"Marlett",
            L"MT Extra",
            L"MS Outlook",
            L"MS Reference Specialty",
            L"Bookshelf Symbol 7",
            NULL
        };
        
        for (int i = 0; blacklist[i] != NULL; i++) {
            if (_wcsicmp(faceName, blacklist[i]) == 0) {
                LOG_DEBUG("FontEnum: Skipping blacklisted font: %S", faceName);
                return 1;  /* Skip blacklisted font */
            }
        }
        
        LOG_DEBUG("FontEnum: Processing font: %S", faceName);
        
        /* Try to resolve font path */
        char fontPath[MAX_PATH];
        if (GetSystemFontPath(faceName, fontPath, sizeof(fontPath), (HANDLE)lParam)) {
            if (!AddOrUpdateFontMap(faceName, fontPath)) {
                return 0;
            }
        } else {
            LOG_DEBUG("FontEnum: Failed to resolve path for font: %S", faceName);
        }
    } else {
        LOG_DEBUG("FontEnum: Skipping non-TrueType font: %S", lpelf->lfFaceName);
    }
    return 1;
}

static int CalculateFontListStorageBytes(void) {
    size_t storageBytes = 0;

    for (int i = 0; i < g_fontMapCount; i++) {
        size_t nameBytes = (wcslen(g_fontMap[i].fontName) + 1) * sizeof(wchar_t);
        if (nameBytes > (size_t)INT_MAX ||
            storageBytes > (size_t)INT_MAX - nameBytes) {
            return INT_MAX;
        }
        storageBytes += nameBytes;
    }

    return (int)storageBytes;
}

static void SelectCurrentFontInList(HWND hdlg, HWND hwndList) {
    if (!hdlg || !hwndList) return;

    /* Check if current font is from Windows Fonts directory and select it */
    wchar_t fontsDir[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
        return;
    }

    wchar_t currentFontW[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, currentFontW, MAX_PATH) <= 0) {
        return;
    }

    if (wcsstr(currentFontW, fontsDir) == currentFontW) {
        BOOL found = FALSE;
        for (int i = 0; i < g_fontMapCount; i++) {
            if (_stricmp(g_fontMap[i].fontPath, FONT_FILE_NAME) == 0) {
                LRESULT idx = SendMessageW(hwndList, LB_FINDSTRINGEXACT,
                                           (WPARAM)-1, (LPARAM)g_fontMap[i].fontName);

                if (idx != LB_ERR) {
                    SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)idx, 0);
                    if (idx >= 0 && idx <= INT_MAX) {
                        g_currentFontIndex = (int)idx;
                    } else {
                        g_currentFontIndex = -1;
                    }

                    SendMessageW(hwndList, LB_SETTOPINDEX, (WPARAM)idx, 0);
                    SetFocus(hwndList);
                    InvalidateRect(hwndList, NULL, TRUE);
                    UpdateWindow(hwndList);
                    found = TRUE;
                }
                break;
            }
        }

        if (!found) {
            LOG_WARNING("FontPicker: Current system font not in font map (may be filtered)");
            SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
            g_currentFontIndex = -1;
        }
    } else {
        SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
        g_currentFontIndex = -1;
    }

    InvalidateRect(hwndList, NULL, TRUE);
}

static void PopulateFontList(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (!hwndList) {
        return;
    }

    SendMessageW(hwndList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
    if (g_fontMapCount > 0) {
        LRESULT reserveResult = SendMessageW(hwndList, LB_INITSTORAGE,
                                             (WPARAM)g_fontMapCount,
                                             (LPARAM)CalculateFontListStorageBytes());
        if (reserveResult == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox storage reservation failed for %d fonts", g_fontMapCount);
        }
    }

    int addedCount = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        LRESULT index = SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)g_fontMap[i].fontName);
        if (index == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox ran out of storage after %d/%d fonts", addedCount, g_fontMapCount);
            break;
        }
        if (index == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to add font '%S' to listbox", g_fontMap[i].fontName);
            continue;
        }
        if (SendMessageW(hwndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)i) == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to bind font map index for '%S'", g_fontMap[i].fontName);
        }
        addedCount++;
    }
    SendMessageW(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, NULL, TRUE);

    EnableWindow(hwndList, TRUE);
    EnableWindow(GetDlgItem(hdlg, IDOK), TRUE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Font families (variants filtered):"));

    SelectCurrentFontInList(hdlg, hwndList);

}

static void BuildFontMap(HANDLE stopEvent) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return;
    }

    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamiliesProc, (LPARAM)stopEvent, 0);

    if (ShouldStopFontEnumeration(stopEvent)) {
        ReleaseDC(NULL, hdc);
        return;
    }

    int writeIndex = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        if (ShouldStopFontEnumeration(stopEvent)) {
            ReleaseDC(NULL, hdc);
            return;
        }

        if (CheckFontHasRequiredGlyphs(hdc, g_fontMap[i].fontName, stopEvent)) {
            if (writeIndex != i) {
                g_fontMap[writeIndex] = g_fontMap[i];
            }
            writeIndex++;
        }
    }

    g_fontMapCount = writeIndex;
    ReleaseDC(NULL, hdc);
}

static DWORD WINAPI FontEnumerationThread(LPVOID param) {
    FontEnumerationThreadParams* params = (FontEnumerationThreadParams*)param;
    HWND hdlg = params ? params->hdlg : NULL;
    HANDLE stopEvent = params ? params->stopEvent : NULL;
    LONG generation = params ? params->generation : 0;
    free(params);

    BuildFontMap(stopEvent);

    if (!ShouldStopFontEnumeration(stopEvent) &&
        InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) == generation &&
        hdlg &&
        IsWindow(hdlg) &&
        Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) == hdlg) {
        PostMessageW(hdlg, WM_APP_FONT_ENUM_COMPLETE, (WPARAM)generation, 0);
    }

    if (stopEvent) {
        CloseHandle(stopEvent);
    }

    return 0;
}

static HANDLE StartFontEnumerationThread(HWND hdlg) {
    FontEnumerationThreadParams* params =
        (FontEnumerationThreadParams*)malloc(sizeof(FontEnumerationThreadParams));
    if (!params) {
        return NULL;
    }

    params->hdlg = hdlg;
    params->stopEvent = NULL;
    params->generation = InterlockedIncrement(&g_fontEnumGeneration);
    if (g_fontEnumStopEvent &&
        !DuplicateHandle(GetCurrentProcess(), g_fontEnumStopEvent,
                         GetCurrentProcess(), &params->stopEvent,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_WARNING("FontPicker: Failed to duplicate enumeration stop event (error=%lu)",
                    GetLastError());
        free(params);
        return NULL;
    }

    HANDLE hThread = CreateThread(NULL, 0, FontEnumerationThread, params, 0, NULL);
    if (!hThread) {
        if (params->stopEvent) {
            CloseHandle(params->stopEvent);
        }
        free(params);
    }
    return hThread;
}

static void CloseFontEnumStopEventIfIdle(void) {
    if (!g_fontEnumThread && g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
}

static void ShowFontEnumerationUnavailable(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (hwndList) {
        SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
        EnableWindow(hwndList, FALSE);
    }
    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Loading..."));
    g_fontListReady = FALSE;
}

static BOOL StartFontEnumerationAsync(HWND hdlg) {
    CloseFontEnumStopEventIfIdle();

    g_fontEnumStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_fontEnumStopEvent) {
        LOG_WARNING("FontPicker: Failed to create enumeration stop event (error=%lu)",
                    GetLastError());
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    g_fontEnumThread = StartFontEnumerationThread(hdlg);
    if (!g_fontEnumThread) {
        LOG_WARNING("FontPicker: Background enumeration unavailable; retrying asynchronously");
        CloseFontEnumStopEventIfIdle();
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
    StartFontEnumPollTimer(hdlg);
    return TRUE;
}

static INT_PTR CALLBACK SimpleFontPickerProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FONT_LIST_SIMPLE) {
                HDC hdc = GetDC(hdlg);
                if (!hdc) {
                    /* Fallback to default height if GetDC fails */
                    mis->itemHeight = 20;
                    return TRUE;
                }
                TEXTMETRIC tm = {0};
                if (!GetTextMetrics(hdc, &tm)) {
                    ReleaseDC(hdlg, hdc);
                    mis->itemHeight = 20;
                    return TRUE;
                }
                ReleaseDC(hdlg, hdc);
                mis->itemHeight = tm.tmHeight + 4;  /* Add padding */
                return TRUE;
            }
            return FALSE;
        }
        
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID != IDC_FONT_LIST_SIMPLE) return FALSE;
            
            if (dis->itemID == (UINT)-1) return TRUE;
            
            /* Get item text */
            wchar_t text[LF_FACESIZE];
            LRESULT textLen = SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
            if (textLen == LB_ERR) {
                /* Failed to get text, skip drawing */
                return TRUE;
            }
            
            /* Fill background */
            COLORREF bgColor = (dis->itemState & ODS_SELECTED) ?
                               GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW);
            COLORREF txtColor = (dis->itemState & ODS_SELECTED) ?
                                GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT);
            COLORREF oldTextColor = GetTextColor(dis->hDC);
            COLORREF oldBkColor = GetBkColor(dis->hDC);

            FillRect(dis->hDC, &dis->rcItem,
                    (dis->itemState & ODS_SELECTED) ?
                    GetSysColorBrush(COLOR_HIGHLIGHT) : GetSysColorBrush(COLOR_WINDOW));
            
            /* Draw checkmark if this is the current font */
            int textLeft = dis->rcItem.left + 4;
            if ((int)dis->itemID == g_currentFontIndex) {
                SetTextColor(dis->hDC, txtColor);
                SetBkColor(dis->hDC, bgColor);
                TextOutW(dis->hDC, dis->rcItem.left + 2, dis->rcItem.top + 2, L"✓", 1);
                textLeft += 16;  /* Make room for checkmark */
            }
            
            /* Draw text */
            SetTextColor(dis->hDC, txtColor);
            SetBkColor(dis->hDC, bgColor);
            RECT textRect = dis->rcItem;
            textRect.left = textLeft;
            DrawTextW(dis->hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            /* Draw focus rectangle */
            if (dis->itemState & ODS_FOCUS) {
                DrawFocusRect(dis->hDC, &dis->rcItem);
            }

            if (oldTextColor != CLR_INVALID) {
                SetTextColor(dis->hDC, oldTextColor);
            }
            if (oldBkColor != CLR_INVALID) {
                SetBkColor(dis->hDC, oldBkColor);
            }

            return TRUE;
        }
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_FONT_PICKER, hdlg);
            
            /* Start timer to maintain TOPMOST state across virtual desktop switches */
            if (!SetTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID, 500, NULL)) {
                LOG_WARNING("FontPicker: Failed to start topmost timer (error=%lu)",
                            GetLastError());
            }
            
            /* Initialize checkmark index at dialog start */
            g_currentFontIndex = -1;
            
            SetWindowTextW(hdlg, GetLocalizedString(NULL, L"Select Font"));
            SetDlgItemTextW(hdlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hdlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
            SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL, 
                           GetLocalizedString(NULL, L"Font families (variants filtered):"));
            
            MoveDialogToPrimaryScreen(hdlg);
            SaveOriginalFont();

            HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
            if (!hwndList) {
                LOG_ERROR("FontPicker: Failed to get list control");
                return TRUE;
            }

            if (!CleanupCompletedFontEnumeration()) {
                StopFontEnumerationWithTimeout(FONT_ENUM_STOP_WAIT_MS);
            }
            if (!CleanupCompletedFontEnumeration()) {
                g_fontEnumRestartAfterCleanup = TRUE;
                SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                               GetLocalizedString(NULL, L"Font families (variants filtered):"));
                StartFontEnumPollTimer(hdlg);
                return TRUE;
            }

            g_fontEnumRestartAfterCleanup = FALSE;
            ResetFontMap();
            g_fontListReady = FALSE;
            EnableWindow(hwndList, FALSE);
            EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
            SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                           GetLocalizedString(NULL, L"Font families (variants filtered):"));

            StartFontEnumerationAsync(hdlg);

            return TRUE;
        }

        case WM_APP_FONT_ENUM_COMPLETE:
            if ((LONG)wp != InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) ||
                Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) != hdlg) {
                return TRUE;
            }
            KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
            if (g_fontListReady) {
                return TRUE;
            }
            if (!CleanupCompletedFontEnumeration()) {
                g_fontEnumRestartAfterCleanup = FALSE;
                StartFontEnumPollTimer(hdlg);
                return TRUE;
            }
            g_fontListReady = TRUE;
            PopulateFontList(hdlg);
            return TRUE;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wp == FONT_PICKER_TOPMOST_TIMER_ID) {
                /* Re-apply TOPMOST to maintain visibility across virtual desktops */
                Dialog_ApplyTopmost(hdlg);
                return TRUE;
            }
            if (wp == FONT_ENUM_POLL_TIMER_ID) {
                if (CleanupCompletedFontEnumeration()) {
                    KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);

                    if (!g_fontEnumRestartAfterCleanup) {
                        if (!g_fontListReady) {
                            g_fontListReady = TRUE;
                            PopulateFontList(hdlg);
                        }
                        return TRUE;
                    }

                    g_fontEnumRestartAfterCleanup = FALSE;
                    ResetFontMap();
                    g_fontListReady = FALSE;

                    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
                    if (hwndList) {
                        EnableWindow(hwndList, FALSE);
                    }
                    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);

                    StartFontEnumerationAsync(hdlg);
                }
                return TRUE;
            }
            if (wp == FONT_ENUM_START_RETRY_TIMER_ID) {
                if (!g_fontEnumThread) {
                    StartFontEnumerationAsync(hdlg);
                }
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            if (LOWORD(wp) == IDOK) {
                if (!CommitCurrentFontSelection()) {
                    RestoreOriginalFont();
                    return TRUE;
                }
                g_fontState.closeHandled = TRUE;
                KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDCANCEL) {
                RestoreOriginalFont();
                g_fontState.closeHandled = TRUE;
                KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDC_FONT_LIST_SIMPLE) {
                if (HIWORD(wp) == LBN_SELCHANGE) {
                    if (!g_fontListReady) {
                        return TRUE;
                    }
                    /* Real-time preview when selection changes */
                    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
                    int sel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR) {
                        LRESULT itemData = SendMessageW(hwndList, LB_GETITEMDATA, (WPARAM)sel, 0);
                        if (itemData >= 0 && itemData < g_fontMapCount) {
                            int fontIndex = (int)itemData;
                            PreviewFontInMainWindow(g_fontMap[fontIndex].fontName,
                                                    g_fontMap[fontIndex].fontPath,
                                                    hdlg, hwndList);
                        } else {
                            wchar_t fontName[LF_FACESIZE];
                            ZeroMemory(fontName, sizeof(fontName));
                            if (SendMessageW(hwndList, LB_GETTEXT, (WPARAM)sel,
                                             (LPARAM)fontName) != LB_ERR) {
                                PreviewFontInMainWindow(fontName, NULL, hdlg, hwndList);
                            }
                        }

                        /* Verify selection is still set after applying */
                        int newSel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                        if (newSel != sel) {
                            LOG_ERROR("FontPicker: ✗✗✗ SELECTION LOST! Was %d, now %d. Re-selecting.", sel, newSel);
                            SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)sel, 0);
                            SetFocus(hwndList);
                        }
                    } else {
                        LOG_WARNING("FontPicker: LBN_SELCHANGE but GETCURSEL returned LB_ERR!");
                    }
                    return TRUE;
                } else if (HIWORD(wp) == LBN_DBLCLK) {
                    /* Double-click acts as OK */
                    SendMessageW(hdlg, WM_COMMAND, IDOK, 0);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
            KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
            SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
            return TRUE;

        case WM_DESTROY:
            if (!g_fontState.closeHandled) {
                RestoreOriginalFont();
                g_fontState.closeHandled = TRUE;
            }
            KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
            KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
            KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_FONT_PICKER, hdlg);

            BOOL enumStopped = StopFontEnumerationWithTimeout(FONT_ENUM_STOP_WAIT_MS);
            if (!enumStopped) {
                LOG_WARNING("FontPicker: Leaving slow enumeration thread to finish asynchronously");
                ScheduleDeferredFontEnumerationCleanup();
            }

            /* Clear checkmark index */
            g_currentFontIndex = -1;
            g_fontListReady = FALSE;
            g_fontState.closeHandled = FALSE;

            if (enumStopped) {
                ResetFontMap();
            }
            return TRUE;
    }
    return FALSE;
}

BOOL ShowSystemFontDialog(HWND hwndParent) {
    /* Check if dialog is already open */
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER);
        SetForegroundWindow(existing);
        return TRUE;
    }
    
    /* Create modeless dialog */
    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(IDD_FONT_PICKER_SIMPLE),
        hwndParent,
        SimpleFontPickerProc
    );
    
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}
