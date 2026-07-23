/**
 * @file font_manager.c
 * @brief Low-level GDI font resource ownership
 */

#include "font_manager_internal.h"

char FONT_FILE_NAME[MAX_PATH] = FONT_FOLDER_PREFIX "Wallpoet Essence.ttf";
char FONT_RUNTIME_FILE_NAME[MAX_PATH] = FONT_FOLDER_PREFIX "Wallpoet Essence.ttf";
char FONT_INTERNAL_NAME[MAX_PATH];
char PREVIEW_FONT_NAME[MAX_PATH] = "";
char PREVIEW_INTERNAL_NAME[MAX_PATH] = "";
BOOL IS_PREVIEWING = FALSE;

static wchar_t g_currentLoadedFontPath[MAX_PATH] = {0};
static BOOL g_fontResourceLoaded = FALSE;

BOOL FontManager_CopyStringExact(
    const char* source, char* output, size_t outputSize) {
    if (!output || outputSize == 0) {
        return FALSE;
    }
    output[0] = '\0';
    if (!source) {
        return FALSE;
    }

    size_t length = strlen(source);
    if (length >= outputSize) {
        return FALSE;
    }
    memcpy(output, source, length + 1);
    return TRUE;
}

BOOL UnloadCurrentFontResource(void) {
    if (!g_fontResourceLoaded || g_currentLoadedFontPath[0] == 0) {
        return TRUE;
    }

    BOOL result = RemoveFontResourceExW(
        g_currentLoadedFontPath, FR_PRIVATE, NULL);
    if (!result) {
        LOG_WARNING("Failed to unload current font resource: %S (error=%lu)",
                    g_currentLoadedFontPath, GetLastError());
        return FALSE;
    }

    g_currentLoadedFontPath[0] = 0;
    g_fontResourceLoaded = FALSE;
    return TRUE;
}

BOOL LoadFontFromFile(const char* fontFilePath) {
    if (!fontFilePath) {
        return FALSE;
    }

    wchar_t wideFontPath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, wideFontPath, MAX_PATH) ||
        GetFileAttributesW(wideFontPath) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    if (g_fontResourceLoaded &&
        wcscmp(g_currentLoadedFontPath, wideFontPath) == 0) {
        return TRUE;
    }

    wchar_t previousFontPath[MAX_PATH] = {0};
    BOOL hadPreviousFont =
        g_fontResourceLoaded && g_currentLoadedFontPath[0] != 0;
    if (hadPreviousFont) {
        wcscpy_s(previousFontPath, MAX_PATH, g_currentLoadedFontPath);
        if (!UnloadCurrentFontResource()) {
            return FALSE;
        }
    }

    int addResult = AddFontResourceExW(wideFontPath, FR_PRIVATE, NULL);
    if (addResult <= 0) {
        LOG_WARNING("Failed to load font resource: %S (error=%lu)",
                    wideFontPath, GetLastError());
        if (hadPreviousFont) {
            int restoreResult = AddFontResourceExW(
                previousFontPath, FR_PRIVATE, NULL);
            if (restoreResult > 0) {
                wcscpy_s(g_currentLoadedFontPath, MAX_PATH, previousFontPath);
                g_fontResourceLoaded = TRUE;
            } else {
                LOG_WARNING(
                    "Failed to restore previous font resource: %S (error=%lu)",
                    previousFontPath, GetLastError());
                g_currentLoadedFontPath[0] = 0;
                g_fontResourceLoaded = FALSE;
            }
        }
        return FALSE;
    }

    wcscpy_s(g_currentLoadedFontPath, MAX_PATH, wideFontPath);
    g_fontResourceLoaded = TRUE;
    return TRUE;
}
