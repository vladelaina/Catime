/**
 * @file dialog_font_picker_map.c
 * @brief Font map construction and listbox population.
 */

#include "dialog_font_picker_internal.h"
#include "font.h"
#include "language.h"
#include "log.h"
#include "../../resource/resource.h"
#include <limits.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

FontMapEntry* g_fontMap = NULL;
int g_fontMapCount = 0;
int g_fontMapCapacity = 0;
BOOL g_fontListReady = FALSE;
int g_currentFontIndex = -1;
int g_previewFontIndex = -1;

void DialogFontPickerInternal_ResetFontMap(void) {
    if (g_fontMap) {
        free(g_fontMap);
        g_fontMap = NULL;
    }
    g_fontMapCount = 0;
    g_fontMapCapacity = 0;
}

static BOOL AddOrUpdateFontMap(const wchar_t* fontName, const char* fontPath) {
    for (int i = 0; i < g_fontMapCount; i++) {
        if (strcmp(g_fontMap[i].fontPath, fontPath) == 0) {
            size_t newLen = wcslen(fontName);
            size_t existingLen = wcslen(g_fontMap[i].fontName);
            BOOL shouldReplace = newLen < existingLen ||
                (newLen == existingLen &&
                 _wcsicmp(fontName, g_fontMap[i].fontName) < 0);
            if (shouldReplace) {
                wcscpy_s(g_fontMap[i].fontName, LF_FACESIZE, fontName);
            }
            return TRUE;
        }
    }

    if (g_fontMapCount >= MAX_FONT_PICKER_ENTRIES) {
        LOG_WARNING("FontPicker: font entry limit reached (%d), stopping enumeration",
                    MAX_FONT_PICKER_ENTRIES);
        return FALSE;
    }

    if (g_fontMapCount >= g_fontMapCapacity) {
        if (g_fontMapCapacity > INT_MAX / 2) {
            return FALSE;
        }
        int newCapacity = g_fontMapCapacity <= 0 ? 256 : g_fontMapCapacity * 2;
        if (newCapacity > MAX_FONT_PICKER_ENTRIES) {
            newCapacity = MAX_FONT_PICKER_ENTRIES;
        }
        if ((size_t)newCapacity > ((size_t)-1) / sizeof(FontMapEntry)) {
            return FALSE;
        }
        FontMapEntry* newMap = (FontMapEntry*)realloc(
            g_fontMap, (size_t)newCapacity * sizeof(FontMapEntry));
        if (!newMap) {
            return FALSE;
        }
        g_fontMap = newMap;
        g_fontMapCapacity = newCapacity;
    }

    wcscpy_s(g_fontMap[g_fontMapCount].fontName, LF_FACESIZE, fontName);
    strncpy(g_fontMap[g_fontMapCount].fontPath, fontPath, MAX_PATH - 1);
    g_fontMap[g_fontMapCount].fontPath[MAX_PATH - 1] = '\0';
    g_fontMapCount++;
    return TRUE;
}

static int CALLBACK EnumFontFamiliesProc(const LOGFONTW* lpelf,
                                         const TEXTMETRICW* lpntm,
                                         DWORD fontType, LPARAM lParam) {
    (void)lpntm;

    if (lParam && WaitForSingleObject((HANDLE)lParam, 0) == WAIT_OBJECT_0) {
        return 0;
    }

    if (fontType & TRUETYPE_FONTTYPE) {
        const wchar_t* faceName = lpelf->lfFaceName;
        if (faceName[0] == L'@' || lpelf->lfCharSet == SYMBOL_CHARSET) {
            return 1;
        }

        const wchar_t* blacklist[] = {
            L"Symbol", L"Webdings", L"Wingdings", L"Wingdings 2",
            L"Wingdings 3", L"Marlett", L"MT Extra", L"MS Outlook",
            L"MS Reference Specialty", L"Bookshelf Symbol 7", NULL
        };
        for (int i = 0; blacklist[i] != NULL; i++) {
            if (_wcsicmp(faceName, blacklist[i]) == 0) {
                return 1;
            }
        }

        char fontPath[MAX_PATH];
        if (DialogFontPickerInternal_GetSystemFontPath(
                faceName, fontPath, sizeof(fontPath), (HANDLE)lParam) &&
            !AddOrUpdateFontMap(faceName, fontPath)) {
            return 0;
        }
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
    if (!hdlg || !hwndList) {
        return;
    }

    wchar_t fontsDir[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
        return;
    }

    wchar_t currentFontW[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1,
                            currentFontW, MAX_PATH) <= 0) {
        return;
    }

    if (wcsstr(currentFontW, fontsDir) == currentFontW) {
        BOOL found = FALSE;
        for (int i = 0; i < g_fontMapCount; i++) {
            if (_stricmp(g_fontMap[i].fontPath, FONT_FILE_NAME) == 0) {
                LRESULT idx = SendMessageW(hwndList, LB_FINDSTRINGEXACT,
                                           (WPARAM)-1,
                                           (LPARAM)g_fontMap[i].fontName);
                if (idx != LB_ERR) {
                    SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)idx, 0);
                    if (idx >= 0 && idx <= INT_MAX) {
                        g_currentFontIndex = (int)idx;
                        g_previewFontIndex = (int)idx;
                    } else {
                        g_currentFontIndex = -1;
                        g_previewFontIndex = -1;
                    }
                    SendMessageW(hwndList, LB_SETTOPINDEX, (WPARAM)idx, 0);
                    SetFocus(hwndList);
                    InvalidateRect(hwndList, NULL, FALSE);
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
            g_previewFontIndex = -1;
        }
    } else {
        SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
        g_currentFontIndex = -1;
        g_previewFontIndex = -1;
    }
    InvalidateRect(hwndList, NULL, FALSE);
}

void DialogFontPickerInternal_PopulateFontList(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (!hwndList) {
        return;
    }

    SendMessageW(hwndList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
    if (g_fontMapCount > 0) {
        LRESULT reserveResult = SendMessageW(
            hwndList, LB_INITSTORAGE, (WPARAM)g_fontMapCount,
            (LPARAM)CalculateFontListStorageBytes());
        if (reserveResult == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox storage reservation failed for %d fonts",
                        g_fontMapCount);
        }
    }

    int addedCount = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        LRESULT index = SendMessageW(hwndList, LB_ADDSTRING, 0,
                                     (LPARAM)g_fontMap[i].fontName);
        if (index == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox ran out of storage after %d/%d fonts",
                        addedCount, g_fontMapCount);
            break;
        }
        if (index == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to add font '%S' to listbox",
                        g_fontMap[i].fontName);
            continue;
        }
        if (SendMessageW(hwndList, LB_SETITEMDATA, (WPARAM)index,
                         (LPARAM)i) == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to bind font map index for '%S'",
                        g_fontMap[i].fontName);
        }
        addedCount++;
    }
    SendMessageW(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, NULL, FALSE);
    EnableWindow(hwndList, TRUE);
    EnableWindow(GetDlgItem(hdlg, IDOK), TRUE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Font families (variants filtered):"));
    SelectCurrentFontInList(hdlg, hwndList);
}

void DialogFontPickerInternal_BuildFontMap(HANDLE stopEvent) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return;
    }

    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamiliesProc,
                        (LPARAM)stopEvent, 0);
    if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
        ReleaseDC(NULL, hdc);
        return;
    }

    int writeIndex = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
            ReleaseDC(NULL, hdc);
            return;
        }
        if (DialogFontPickerInternal_CheckRequiredGlyphs(
                hdc, g_fontMap[i].fontName, stopEvent)) {
            if (writeIndex != i) {
                g_fontMap[writeIndex] = g_fontMap[i];
            }
            writeIndex++;
        }
    }
    g_fontMapCount = writeIndex;
    ReleaseDC(NULL, hdc);
}
