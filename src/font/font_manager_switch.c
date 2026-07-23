#include "font_manager_internal.h"

BOOL CheckAndReloadCurrentFontPath(void) {
    if (strcmp(FONT_RUNTIME_FILE_NAME, FONT_FILE_NAME) != 0) {
        char previousRuntimeName[MAX_PATH] = {0};
        FontManager_CopyStringExact(
            FONT_RUNTIME_FILE_NAME, previousRuntimeName,
            sizeof(previousRuntimeName));
        const char* loadName = FONT_FILE_NAME;
        if (IsFontsFolderPath(FONT_FILE_NAME)) {
            const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
            if (relativePath) {
                loadName = relativePath;
            }
        }

        char loadedInternalName[MAX_PATH] = {0};
        if (!loadName[0] || !LoadFontByNameAndGetRealName(
                GetModuleHandle(NULL), loadName,
                loadedInternalName, sizeof(loadedInternalName))) {
            return FALSE;
        }
        if (strcmp(FONT_RUNTIME_FILE_NAME, previousRuntimeName) == 0) {
            FontManager_CopyStringExact(
                FONT_FILE_NAME, FONT_RUNTIME_FILE_NAME,
                sizeof(FONT_RUNTIME_FILE_NAME));
        }
        FontManager_CopyStringExact(
            loadedInternalName, FONT_INTERNAL_NAME,
            sizeof(FONT_INTERNAL_NAME));
        LOG_INFO("Recovered configured font: %s", FONT_FILE_NAME);
        return TRUE;
    }

    if (!IsFontsFolderPath(FONT_FILE_NAME)) {
        return FALSE;
    }
    const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
    if (!relativePath) {
        return FALSE;
    }

    char fontPath[MAX_PATH];
    wchar_t wideFontPath[MAX_PATH];
    if (!BuildFullFontPath(relativePath, fontPath, MAX_PATH) ||
        !Utf8ToWide(fontPath, wideFontPath, MAX_PATH) ||
        GetFileAttributesW(wideFontPath) != INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};
    FontManager_CopyStringExact(
        FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    FontManager_CopyStringExact(
        FONT_RUNTIME_FILE_NAME, previousRuntimeName,
        sizeof(previousRuntimeName));
    FontManager_CopyStringExact(
        FONT_INTERNAL_NAME, previousInternalName,
        sizeof(previousInternalName));
    if (!LoadFontByNameAndGetRealName(
            GetModuleHandle(NULL), relativePath,
            loadedInternalName, sizeof(loadedInternalName))) {
        FontManager_CopyStringExact(
            previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        FontManager_CopyStringExact(
            previousInternalName, FONT_INTERNAL_NAME,
            sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }

    FontManager_CopyStringExact(
        loadedInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    if (strcmp(FONT_RUNTIME_FILE_NAME, previousRuntimeName) == 0) {
        FontManager_CopyStringExact(
            FONT_FILE_NAME, FONT_RUNTIME_FILE_NAME,
            sizeof(FONT_RUNTIME_FILE_NAME));
    }
    return TRUE;
}

BOOL FontManager_ReloadFromConfigName(
    HINSTANCE hInstance, const char* fontName,
    char* outInternalName, size_t outInternalNameSize) {
    if (!fontName || fontName[0] == '\0' || !outInternalName ||
        outInternalNameSize == 0) {
        UnloadCurrentFontResource();
        if (outInternalName && outInternalNameSize > 0) {
            outInternalName[0] = '\0';
        }
        return TRUE;
    }

    const char* reloadName = fontName;
    if (IsFontsFolderPath(fontName)) {
        const char* relativePath = ExtractRelativePath(fontName);
        if (relativePath) {
            reloadName = relativePath;
        }
    }
    if (LoadFontByNameAndGetRealName(
            hInstance, reloadName, outInternalName, outInternalNameSize)) {
        return TRUE;
    }
    UnloadCurrentFontResource();
    outInternalName[0] = '\0';
    return FALSE;
}

BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) {
        return FALSE;
    }

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char pendingFontName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};
    if (!FontManager_CopyStringExact(
            fontName, pendingFontName, sizeof(pendingFontName))) {
        LOG_WARNING("Font name too long, ignoring switch: %s", fontName);
        return FALSE;
    }
    FontManager_CopyStringExact(
        FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    FontManager_CopyStringExact(
        FONT_RUNTIME_FILE_NAME, previousRuntimeName,
        sizeof(previousRuntimeName));
    FontManager_CopyStringExact(
        FONT_INTERNAL_NAME, previousInternalName,
        sizeof(previousInternalName));

    if (!LoadFontByNameAndGetRealName(
            hInstance, pendingFontName,
            loadedInternalName, sizeof(loadedInternalName))) {
        FontManager_CopyStringExact(
            previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        FontManager_CopyStringExact(
            previousInternalName, FONT_INTERNAL_NAME,
            sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }
    FontManager_CopyStringExact(
        pendingFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
    FontManager_CopyStringExact(
        pendingFontName, FONT_RUNTIME_FILE_NAME,
        sizeof(FONT_RUNTIME_FILE_NAME));
    FontManager_CopyStringExact(
        loadedInternalName, FONT_INTERNAL_NAME,
        sizeof(FONT_INTERNAL_NAME));

    if (!WriteConfigFont(FONT_FILE_NAME, FALSE)) {
        FontManager_CopyStringExact(
            previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        FontManager_CopyStringExact(
            previousRuntimeName, FONT_RUNTIME_FILE_NAME,
            sizeof(FONT_RUNTIME_FILE_NAME));
        FontManager_CopyStringExact(
            previousInternalName, FONT_INTERNAL_NAME,
            sizeof(FONT_INTERNAL_NAME));
        if (!FontManager_ReloadFromConfigName(
                hInstance, previousRuntimeName,
                FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
            LOG_WARNING(
                "Failed to restore previous font after config write failure: %s",
                previousFontName);
            FontManager_CopyStringExact(
                previousInternalName, FONT_INTERNAL_NAME,
                sizeof(FONT_INTERNAL_NAME));
        }
        return FALSE;
    }
    return TRUE;
}
