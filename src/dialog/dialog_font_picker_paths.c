/**
 * @file dialog_font_picker_paths.c
 * @brief System font path resolution and glyph validation.
 */

#include "dialog_font_picker_internal.h"
#include "log.h"
#include <limits.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

static BOOL WideFontPathToUtf8(const wchar_t* fontPath, char* outPath,
                               size_t outPathSize) {
    if (!fontPath || !outPath || outPathSize == 0 || outPathSize > INT_MAX) {
        return FALSE;
    }

    outPath[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, fontPath, -1,
                                       NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > outPathSize) {
        return FALSE;
    }

    return WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, outPath,
                               (int)outPathSize, NULL, NULL) > 0;
}

BOOL DialogFontPickerInternal_GetSystemFontPath(const wchar_t* fontName,
                                                char* outPath,
                                                size_t outPathSize,
                                                HANDLE stopEvent) {
    if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
        return FALSE;
    }

    static wchar_t fontsDir[MAX_PATH] = {0};
    static BOOL fontsDirInitialized = FALSE;
    if (!fontsDirInitialized) {
        if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
            LOG_WARNING("FontPath: Failed to get system fonts directory, using fallback");
            wcscpy_s(fontsDir, MAX_PATH, L"C:\\Windows\\Fonts");
        }
        fontsDirInitialized = TRUE;
    }

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
        if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
            return FALSE;
        }

        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s",
                   fontsDir, fontName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            return WideFontPathToUtf8(fontPath, outPath, outPathSize);
        }

        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s",
                   fontsDir, lowerName, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            return WideFontPathToUtf8(fontPath, outPath, outPathSize);
        }

        swprintf_s(fontPath, MAX_PATH, L"%s\\%s%s",
                   fontsDir, noSpace, extensions[i]);
        if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
            return WideFontPathToUtf8(fontPath, outPath, outPathSize);
        }
    }

    /* Variable font variants usually share a file with their base family. */
    if (wcsstr(fontName, L"Light") || wcsstr(fontName, L"SemiLight") ||
        wcsstr(fontName, L"SemiBold") || wcsstr(fontName, L"ExtraLight")) {
        return FALSE;
    }
    return FALSE;
}

BOOL DialogFontPickerInternal_CheckRequiredGlyphs(HDC hdc,
                                                  const wchar_t* fontName,
                                                  HANDLE stopEvent) {
    if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
        return FALSE;
    }

    const wchar_t requiredChars[] = L"0123456789:";
    const int requiredCount = 11;
    if (!hdc) {
        LOG_ERROR("GlyphCheck: ✗ Invalid DC for font '%S'", fontName);
        return FALSE;
    }

    HFONT hFont = CreateFontW(
        -24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, fontName);
    if (!hFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to create test font for '%S'", fontName);
        return FALSE;
    }

    if (DialogFontPickerInternal_ShouldStopEnumeration(stopEvent)) {
        DeleteObject(hFont);
        return FALSE;
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    if (!oldFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to select test font for '%S'", fontName);
        DeleteObject(hFont);
        return FALSE;
    }

    WORD glyphIndices[11];
    DWORD result = GetGlyphIndicesW(hdc, requiredChars, requiredCount,
                                    glyphIndices, GGI_MARK_NONEXISTING_GLYPHS);
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    if (result == GDI_ERROR) {
        LOG_ERROR("GlyphCheck: ✗ GetGlyphIndicesW failed for '%S'", fontName);
        return FALSE;
    }

    for (int i = 0; i < requiredCount; i++) {
        if (glyphIndices[i] == 0xFFFF) {
            return FALSE;
        }
    }
    return TRUE;
}
