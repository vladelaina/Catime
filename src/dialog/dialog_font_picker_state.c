/**
 * @file dialog_font_picker_state.c
 * @brief Font selection state, preview, and commit helpers.
 */

#include "dialog_font_picker_internal.h"
#include "font.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "window/window_core.h"
#include "log.h"
#include <string.h>

FontDialogState g_fontState = {0};

void DialogFontPickerInternal_SaveOriginalFont(void) {
    strncpy(g_fontState.originalFontName, FONT_INTERNAL_NAME,
            sizeof(g_fontState.originalFontName) - 1);
    g_fontState.originalFontName[sizeof(g_fontState.originalFontName) - 1] = '\0';

    strncpy(g_fontState.originalFileName, FONT_FILE_NAME,
            sizeof(g_fontState.originalFileName) - 1);
    g_fontState.originalFileName[sizeof(g_fontState.originalFileName) - 1] = '\0';
    strncpy(g_fontState.originalRuntimeFileName, FONT_RUNTIME_FILE_NAME,
            sizeof(g_fontState.originalRuntimeFileName) - 1);
    g_fontState.originalRuntimeFileName[
        sizeof(g_fontState.originalRuntimeFileName) - 1] = '\0';
    g_fontState.closeHandled = FALSE;
}

void DialogFontPickerInternal_RestoreOriginalFont(void) {
    HWND hwnd = FindCurrentProcessMainWindow();
    CancelPreview(hwnd);

    strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName,
            sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';

    strncpy(FONT_FILE_NAME, g_fontState.originalFileName,
            sizeof(FONT_FILE_NAME) - 1);
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
                                      FONT_INTERNAL_NAME,
                                      sizeof(FONT_INTERNAL_NAME))) {
        strncpy(FONT_INTERNAL_NAME, g_fontState.originalFontName,
                sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }

    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
        RestoreWindowVisibility(hwnd);
    }
}

BOOL DialogFontPickerInternal_PreviewFont(const wchar_t* fontName,
                                          const char* cachedFontPath,
                                          HWND hdlg, HWND hwndList) {
    char fontPath[MAX_PATH] = {0};

    if (cachedFontPath && cachedFontPath[0]) {
        strncpy(fontPath, cachedFontPath, sizeof(fontPath) - 1);
        fontPath[sizeof(fontPath) - 1] = '\0';
    } else if (!DialogFontPickerInternal_GetSystemFontPath(
                   fontName, fontPath, sizeof(fontPath), NULL)) {
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

    /* Restore focus to the list after preview invalidates the main window. */
    if (hwndList && hdlg) {
        SetFocus(hwndList);
        InvalidateRect(hwndList, NULL, FALSE);
        UpdateWindow(hwndList);
    }

    return TRUE;
}

BOOL DialogFontPickerInternal_CommitSelection(HWND hwnd) {
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
