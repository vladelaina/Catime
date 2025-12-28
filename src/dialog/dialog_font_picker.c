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
} FontDialogState;

static FontDialogState g_fontState = {0};

static void SaveOriginalFont(void) {
    strncpy(g_fontState.originalFontName, FONT_INTERNAL_NAME, sizeof(g_fontState.originalFontName) - 1);
    g_fontState.originalFontName[sizeof(g_fontState.originalFontName) - 1] = '\0';
    
    strncpy(g_fontState.originalFileName, FONT_FILE_NAME, sizeof(g_fontState.originalFileName) - 1);
    g_fontState.originalFileName[sizeof(g_fontState.originalFileName) - 1] = '\0';
}

static void RestoreOriginalFont(void) {
    LOG_INFO("FontRestore: Font selection cancelled, restoring original font");
    LOG_INFO("FontRestore: Original internal name: %s", g_fontState.originalFontName);
    LOG_INFO("FontRestore: Original file path: %s", g_fontState.originalFileName);
    
    strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    strncpy(FONT_FILE_NAME, g_fontState.originalFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    LOG_INFO("FontRestore: Restoring config...");
    extern void WriteConfigFont(const char* fontName, BOOL reload);
    WriteConfigFont(FONT_FILE_NAME, FALSE);
    FlushConfigToDisk();
    LOG_INFO("FontRestore: ✓ Config restored");
    
    HWND hwnd = FindWindowW(L"CatimeWindowClass", L"Catime");
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
        LOG_INFO("FontRestore: ✓ Main window invalidated for redraw");
    }
    
    LOG_INFO("FontRestore: ========== FONT RESTORED ==========");
}

static BOOL GetSystemFontPath(const wchar_t* fontName, char* outPath, size_t outPathSize) {
    /* Get Windows Fonts directory */
    static wchar_t fontsDir[MAX_PATH] = {0};
    static BOOL fontsDirInitialized = FALSE;
    
    if (!fontsDirInitialized) {
        if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
            LOG_WARNING("FontPath: Failed to get system fonts directory, using fallback");
            wcscpy_s(fontsDir, MAX_PATH, L"C:\\Windows\\Fonts");
        }
        fontsDirInitialized = TRUE;
        LOG_INFO("FontPath: System fonts directory initialized: %S", fontsDir);
    }
    
    /* Build font file path - try common patterns */
    wchar_t fontPath[MAX_PATH];
    const wchar_t* extensions[] = {L".ttf", L".otf", L".ttc"};
    
    for (int i = 0; i < 3; i++) {
        /* Try direct name match (e.g., "Arial" -> "Arial.ttf") */
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, fontName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, outPath, (int)outPathSize, NULL, NULL);
            LOG_INFO("FontPath: ✓ '%S' -> %s", fontName, outPath);
            return TRUE;
        }
        
        /* Try lowercase (e.g., "Arial" -> "arial.ttf") */
        wchar_t lowerName[MAX_PATH];
        wcscpy_s(lowerName, MAX_PATH, fontName);
        _wcslwr_s(lowerName, MAX_PATH);
        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s", fontsDir, lowerName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, outPath, (int)outPathSize, NULL, NULL);
            LOG_INFO("FontPath: ✓ '%S' -> %s (lowercase)", fontName, outPath);
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
            WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, outPath, (int)outPathSize, NULL, NULL);
            LOG_INFO("FontPath: ✓ '%S' -> %s (no-space)", fontName, outPath);
            return TRUE;
        }
    }
    
    /* Only log variable font variants at debug level to reduce noise */
    if (wcsstr(fontName, L"Light") || wcsstr(fontName, L"SemiLight") || 
        wcsstr(fontName, L"SemiBold") || wcsstr(fontName, L"ExtraLight")) {
        /* Likely a variable font variant that doesn't have a separate file */
        return FALSE;
    }
    
    LOG_WARNING("FontPath: ✗ Cannot resolve '%S' (may be variable font variant)", fontName);
    return FALSE;
}

static void ApplyFontToMainWindow(const wchar_t* fontName, HWND hdlg, HWND hwndList) {
    char fontPath[MAX_PATH];
    
    LOG_INFO("FontApply: User selected font: %S", fontName);
    
    /* Try to find the font file in Windows Fonts directory */
    if (!GetSystemFontPath(fontName, fontPath, sizeof(fontPath))) {
        LOG_ERROR("FontApply: ✗ Failed to locate font file for: %S", fontName);
        return;
    }
    
    LOG_INFO("FontApply: System font resolved to: %s", fontPath);
    
    /* Save the full file path */
    strncpy(FONT_FILE_NAME, fontPath, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    LOG_INFO("FontApply: Updated FONT_FILE_NAME to: %s", FONT_FILE_NAME);
    
    /* Load font and get internal name */
    extern int LoadFontByNameAndGetRealName(HINSTANCE, const char*, char*, size_t);
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    
    LOG_INFO("FontApply: Loading font via LoadFontByNameAndGetRealName...");
    if (LoadFontByNameAndGetRealName(hInstance, fontPath, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME)) == 0) {
        LOG_WARNING("FontApply: LoadFontByNameAndGetRealName failed, using display name as fallback");
        WideCharToMultiByte(CP_UTF8, 0, fontName, -1,
                           FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME), NULL, NULL);
    } else {
        LOG_INFO("FontApply: ✓ Font loaded successfully, internal name: %s", FONT_INTERNAL_NAME);
    }
    
    extern void WriteConfigFont(const char* fontName, BOOL reload);
    LOG_INFO("FontApply: Writing config to disk...");
    WriteConfigFont(FONT_FILE_NAME, FALSE);
    FlushConfigToDisk();
    LOG_INFO("FontApply: ✓ Config saved");
    
    HWND hwnd = FindWindowW(L"CatimeWindowClass", L"Catime");
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
        LOG_INFO("FontApply: ✓ Main window invalidated for redraw");
    } else {
        LOG_WARNING("FontApply: Main window not found");
    }
    
    /* Restore focus to dialog listbox (main window InvalidateRect may steal focus) */
    if (hwndList && hdlg) {
        SetFocus(hwndList);
        InvalidateRect(hwndList, NULL, TRUE);
        UpdateWindow(hwndList);
        LOG_INFO("FontApply: ✓ Restored focus to font picker listbox");
    }
    
    LOG_INFO("FontApply: ========== FONT APPLIED ==========");
}

typedef struct {
    wchar_t fontName[LF_FACESIZE];
    char fontPath[MAX_PATH];
} FontMapEntry;

static FontMapEntry* g_fontMap = NULL;
static int g_fontMapCount = 0;
static int g_fontMapCapacity = 0;

static BOOL CheckFontHasRequiredGlyphs(const wchar_t* fontName) {
    /* Required characters for countdown timer: 0-9 and colon */
    const wchar_t requiredChars[] = L"0123456789:";
    const int requiredCount = 11;
    
    LOG_INFO("GlyphCheck: Testing font '%S' for required glyphs", fontName);
    
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        LOG_ERROR("GlyphCheck: ✗ Failed to get DC for font '%S'", fontName);
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
        ReleaseDC(NULL, hdc);
        return FALSE;
    }
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    /* Check if all required glyphs exist */
    WORD glyphIndices[11];
    DWORD result = GetGlyphIndicesW(hdc, requiredChars, requiredCount, glyphIndices, GGI_MARK_NONEXISTING_GLYPHS);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
    
    if (result == GDI_ERROR) {
        LOG_ERROR("GlyphCheck: ✗ GetGlyphIndicesW failed for '%S'", fontName);
        return FALSE;
    }
    
    /* Check if any glyph is missing (marked as 0xFFFF) */
    int missingCount = 0;
    for (int i = 0; i < requiredCount; i++) {
        if (glyphIndices[i] == 0xFFFF) {
            LOG_WARNING("GlyphCheck: Missing char '%lc' (U+%04X) in font '%S'", requiredChars[i], requiredChars[i], fontName);
            missingCount++;
        }
    }
    
    if (missingCount > 0) {
        LOG_WARNING("GlyphCheck: ✗ Font '%S' missing %d required glyphs", fontName, missingCount);
        return FALSE;
    }
    
    LOG_INFO("GlyphCheck: ✓ Font '%S' has all required glyphs", fontName);
    return TRUE;
}

static void AddOrUpdateFontMap(const wchar_t* fontName, const char* fontPath) {
    /* Check if this path already exists */
    for (int i = 0; i < g_fontMapCount; i++) {
        if (strcmp(g_fontMap[i].fontPath, fontPath) == 0) {
            /* Same file - keep the shorter name, or alphabetically first if same length */
            size_t newLen = wcslen(fontName);
            size_t existingLen = wcslen(g_fontMap[i].fontName);
            
            BOOL shouldReplace = newLen < existingLen || 
                                 (newLen == existingLen && _wcsicmp(fontName, g_fontMap[i].fontName) < 0);
            
            if (shouldReplace) {
                LOG_INFO("Font dedup: '%S' -> '%S' (same file: %s)", 
                         g_fontMap[i].fontName, fontName, fontPath);
                wcscpy_s(g_fontMap[i].fontName, LF_FACESIZE, fontName);
            } else {
                LOG_INFO("Font dedup: keeping '%S' over '%S' (same file: %s)", 
                         g_fontMap[i].fontName, fontName, fontPath);
            }
            return;
        }
    }
    
    /* New file - add to map */
    if (g_fontMapCount >= g_fontMapCapacity) {
        g_fontMapCapacity = g_fontMapCapacity == 0 ? 256 : g_fontMapCapacity * 2;
        FontMapEntry* newMap = (FontMapEntry*)realloc(g_fontMap, g_fontMapCapacity * sizeof(FontMapEntry));
        if (!newMap) return;
        g_fontMap = newMap;
    }
    
    wcscpy_s(g_fontMap[g_fontMapCount].fontName, LF_FACESIZE, fontName);
    strncpy(g_fontMap[g_fontMapCount].fontPath, fontPath, MAX_PATH - 1);
    g_fontMap[g_fontMapCount].fontPath[MAX_PATH - 1] = '\0';
    g_fontMapCount++;
}

static int CALLBACK EnumFontFamiliesProc(const LOGFONTW* lpelf, const TEXTMETRICW* lpntm, 
                                        DWORD fontType, LPARAM lParam) {
    (void)lpntm;
    (void)lParam;
    
    if (fontType & TRUETYPE_FONTTYPE) {
        const wchar_t* faceName = lpelf->lfFaceName;
        
        /* Skip vertical fonts */
        if (faceName[0] == L'@') {
            LOG_INFO("FontEnum: Skipping vertical font: %S", faceName);
            return 1;
        }
        
        /* Skip symbol fonts by charset */
        if (lpelf->lfCharSet == SYMBOL_CHARSET) {
            LOG_INFO("FontEnum: Skipping SYMBOL_CHARSET font: %S", faceName);
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
                LOG_INFO("FontEnum: Skipping blacklisted font: %S", faceName);
                return 1;  /* Skip blacklisted font */
            }
        }
        
        LOG_INFO("FontEnum: Processing font: %S", faceName);
        
        /* Try to resolve font path */
        char fontPath[MAX_PATH];
        if (GetSystemFontPath(faceName, fontPath, sizeof(fontPath))) {
            AddOrUpdateFontMap(faceName, fontPath);
        } else {
            LOG_WARNING("FontEnum: Failed to resolve path for font: %S", faceName);
        }
    } else {
        LOG_INFO("FontEnum: Skipping non-TrueType font: %S", lpelf->lfFaceName);
    }
    return 1;
}

/* Store the index of currently selected font for visual indication */
static int g_currentFontIndex = -1;

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
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
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
            
            return TRUE;
        }
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_FONT_PICKER, hdlg);
            
            /* Start timer to maintain TOPMOST state across virtual desktop switches */
            SetTimer(hdlg, 9998, 500, NULL);
            
            /* Initialize checkmark index at dialog start */
            g_currentFontIndex = -1;
            
            LOG_INFO("FontPicker: Dialog opened");
            SetWindowTextW(hdlg, GetLocalizedString(NULL, L"Select Font"));
            SetDlgItemTextW(hdlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hdlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
            SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL, 
                           GetLocalizedString(NULL, L"Font families (variants filtered):"));
            
            MoveDialogToPrimaryScreen(hdlg);
            SaveOriginalFont();
            LOG_INFO("FontPicker: Saved original font: %s", FONT_INTERNAL_NAME);
            
            HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
            if (!hwndList) {
                LOG_ERROR("FontPicker: Failed to get list control");
                return TRUE;
            }
            
            /* Reset font map */
            g_fontMapCount = 0;
            LOG_INFO("FontPicker: Starting font enumeration");
            
            /* Enumerate all fonts and build deduplication map */
            HDC hdc = GetDC(NULL);
            LOGFONTW lf = {0};
            lf.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamiliesProc, 0, 0);
            ReleaseDC(NULL, hdc);
            
            LOG_INFO("FontPicker: Enumeration complete, found %d unique fonts", g_fontMapCount);
            
            /* Add deduplicated fonts to list, filtering out fonts without required glyphs */
            LOG_INFO("FontPicker: Starting glyph validation for %d fonts", g_fontMapCount);
            int validCount = 0;
            int filteredCount = 0;
            for (int i = 0; i < g_fontMapCount; i++) {
                if (CheckFontHasRequiredGlyphs(g_fontMap[i].fontName)) {
                    SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)g_fontMap[i].fontName);
                    validCount++;
                } else {
                    LOG_WARNING("FontPicker: Filtered out font '%S' (missing glyphs)", g_fontMap[i].fontName);
                    filteredCount++;
                }
            }
            
            LOG_INFO("FontPicker: ========== SUMMARY ==========");
            LOG_INFO("FontPicker: Total enumerated: %d fonts", g_fontMapCount);
            LOG_INFO("FontPicker: Valid (passed glyph check): %d fonts", validCount);
            LOG_INFO("FontPicker: Filtered (failed glyph check): %d fonts", filteredCount);
            LOG_INFO("FontPicker: ==============================");
            
            /* Check initial list selection state */
            LRESULT initialSel = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
            LOG_INFO("FontPicker: Initial listbox selection state: %d (LB_ERR=%d)", (int)initialSel, LB_ERR);
            
            /* Check if current font is from Windows Fonts directory and select it */
            wchar_t fontsDir[MAX_PATH];
            if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) == S_OK) {
                wchar_t currentFontW[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, currentFontW, MAX_PATH);
                
                LOG_INFO("FontPicker: Current font file: %s", FONT_FILE_NAME);
                LOG_INFO("FontPicker: System fonts directory: %S", fontsDir);
                
                if (wcsstr(currentFontW, fontsDir) == currentFontW) {
                    /* Current font is from system fonts, find it in our font map */
                    LOG_INFO("FontPicker: ✓ Current font IS a system font");
                    
                    /* Search for matching font in the map by file path */
                    BOOL found = FALSE;
                    for (int i = 0; i < g_fontMapCount; i++) {
                        if (_stricmp(g_fontMap[i].fontPath, FONT_FILE_NAME) == 0) {
                            /* Found it! Select this font in the list */
                            LOG_INFO("FontPicker: Found matching font in map: '%S'", g_fontMap[i].fontName);
                            
                            LRESULT idx = SendMessageW(hwndList, LB_FINDSTRINGEXACT, 
                                                       (WPARAM)-1, (LPARAM)g_fontMap[i].fontName);
                            LOG_INFO("FontPicker: LB_FINDSTRINGEXACT returned index: %d", (int)idx);
                            
                            if (idx != LB_ERR) {
                                LRESULT result = SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)idx, 0);
                                LOG_INFO("FontPicker: LB_SETCURSEL(%d) returned: %d", (int)idx, (int)result);
                                
                                /* Store index for checkmark display (with bounds check) */
                                if (idx >= 0 && idx <= INT_MAX) {
                                    g_currentFontIndex = (int)idx;
                                } else {
                                    g_currentFontIndex = -1;  /* Invalid index */
                                    LOG_WARNING("FontPicker: Font index out of range: %lld", (long long)idx);
                                }
                                
                                /* Ensure item is visible and focused */
                                SendMessageW(hwndList, LB_SETTOPINDEX, (WPARAM)idx, 0);
                                SetFocus(hwndList);
                                InvalidateRect(hwndList, NULL, TRUE);
                                UpdateWindow(hwndList);
                                LOG_INFO("FontPicker: Set focus, scrolled, and refreshed display for index %d", (int)idx);
                                
                                /* Verify selection */
                                LRESULT curSel = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                                if (curSel == idx) {
                                    LOG_INFO("FontPicker: ✓✓✓ Selection SUCCESS - index %d is selected with checkmark", (int)curSel);
                                } else {
                                    LOG_ERROR("FontPicker: ✗✗✗ Selection FAILED - wanted %d, got %d", (int)idx, (int)curSel);
                                }
                                found = TRUE;
                            } else {
                                LOG_ERROR("FontPicker: ✗ Font '%S' not found in listbox", g_fontMap[i].fontName);
                            }
                            break;
                        }
                    }
                    
                    if (!found) {
                        LOG_WARNING("FontPicker: Current system font not in font map (may be filtered)");
                        /* Explicitly clear selection */
                        SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
                        g_currentFontIndex = -1;  /* No checkmark */
                        LOG_INFO("FontPicker: Cleared selection (no match found)");
                    }
                } else {
                    LOG_INFO("FontPicker: ✗ Current font is NOT a system font (custom/local font)");
                    /* Explicitly clear any selection */
                    LRESULT clearResult = SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
                    g_currentFontIndex = -1;  /* No checkmark for custom fonts */
                    LOG_INFO("FontPicker: LB_SETCURSEL(-1) to clear selection, returned: %d", (int)clearResult);
                    
                    /* Verify selection is cleared */
                    LRESULT afterClear = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                    if (afterClear == LB_ERR) {
                        LOG_INFO("FontPicker: ✓ Selection cleared successfully (no item selected)");
                    } else {
                        LOG_WARNING("FontPicker: ✗ Selection clear failed, still at index: %d", (int)afterClear);
                    }
                }
            }
            
            /* Final selection state check */
            LRESULT finalSel = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
            LOG_INFO("FontPicker: ========== INIT COMPLETE ==========");
            LOG_INFO("FontPicker: Final selection state: %d (LB_ERR=%d)", (int)finalSel, LB_ERR);
            if (finalSel == LB_ERR) {
                LOG_INFO("FontPicker: No item selected (correct for custom fonts)");
            } else {
                wchar_t selectedFont[LF_FACESIZE];
                SendMessageW(hwndList, LB_GETTEXT, (WPARAM)finalSel, (LPARAM)selectedFont);
                LOG_INFO("FontPicker: Selected item at index %d: '%S'", (int)finalSel, selectedFont);
            }
            LOG_INFO("FontPicker: ======================================");
            return TRUE;
        }

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wp == 9998) {
                /* Re-apply TOPMOST to maintain visibility across virtual desktops */
                Dialog_ApplyTopmost(hdlg);
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            LOG_INFO("FontPicker: WM_COMMAND received (wParam: 0x%X)", wp);
            
            if (LOWORD(wp) == IDOK) {
                LOG_INFO("FontPicker: User clicked OK button - confirming selection");
                /* Just close dialog, font already applied during preview */
                LOG_INFO("FontPicker: Closing dialog with OK");
                KillTimer(hdlg, 9998);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDCANCEL) {
                LOG_INFO("FontPicker: User clicked Cancel button");
                RestoreOriginalFont();
                LOG_INFO("FontPicker: Closing dialog with Cancel");
                KillTimer(hdlg, 9998);
                DestroyWindow(hdlg);
                return TRUE;
            } else if (LOWORD(wp) == IDC_FONT_LIST_SIMPLE) {
                if (HIWORD(wp) == LBN_SELCHANGE) {
                    /* Real-time preview when selection changes */
                    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
                    int sel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                    LOG_INFO("FontPicker: ========== LBN_SELCHANGE ==========");
                    LOG_INFO("FontPicker: User changed selection to index: %d", sel);
                    if (sel != LB_ERR) {
                        wchar_t fontName[LF_FACESIZE];
                        SendMessageW(hwndList, LB_GETTEXT, sel, (LPARAM)fontName);
                        LOG_INFO("FontPicker: Font at index %d: '%S'", sel, fontName);
                        LOG_INFO("FontPicker: Applying real-time preview...");
                        ApplyFontToMainWindow(fontName, hdlg, hwndList);
                        
                        /* Verify selection is still set after applying */
                        int newSel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                        LOG_INFO("FontPicker: After apply, selection is: %d", newSel);
                        if (newSel != sel) {
                            LOG_ERROR("FontPicker: ✗✗✗ SELECTION LOST! Was %d, now %d. Re-selecting.", sel, newSel);
                            SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)sel, 0);
                            SetFocus(hwndList);
                            int recheck = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
                            LOG_INFO("FontPicker: After re-select, selection is: %d", recheck);
                        } else {
                            LOG_INFO("FontPicker: ✓ Selection maintained at index: %d", sel);
                            /* Focus already restored by ApplyFontToMainWindow */
                        }
                    } else {
                        LOG_WARNING("FontPicker: LBN_SELCHANGE but GETCURSEL returned LB_ERR!");
                    }
                    LOG_INFO("FontPicker: ======================================");
                    return TRUE;
                } else if (HIWORD(wp) == LBN_DBLCLK) {
                    LOG_INFO("FontPicker: User double-clicked font in list");
                    /* Double-click acts as OK */
                    SendMessageW(hdlg, WM_COMMAND, IDOK, 0);
                    return TRUE;
                }
            }
            break;
        }
        
        case WM_CLOSE:
            LOG_INFO("FontPicker: WM_CLOSE received");
            KillTimer(hdlg, 9998);
            SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
            return TRUE;
            
        case WM_DESTROY:
            LOG_INFO("FontPicker: WM_DESTROY - Cleaning up resources");
            KillTimer(hdlg, 9998);
            Dialog_UnregisterInstance(DIALOG_INSTANCE_FONT_PICKER);
            
            /* Clear checkmark index */
            g_currentFontIndex = -1;
            
            /* Cleanup font map memory */
            if (g_fontMap) {
                LOG_INFO("FontPicker: Freeing font map (%d entries, %d bytes)", 
                         g_fontMapCount, g_fontMapCapacity * (int)sizeof(FontMapEntry));
                free(g_fontMap);
                g_fontMap = NULL;
                g_fontMapCount = 0;
                g_fontMapCapacity = 0;
            }
            LOG_INFO("FontPicker: Dialog destroyed");
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
