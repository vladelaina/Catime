#include "font_manager_internal.h"

BOOL FontManager_ShouldAttemptAutoFix(const char* fontFileName) {
    if (!fontFileName || fontFileName[0] == '\0') {
        return FALSE;
    }
    if (IsFontsFolderPath(fontFileName)) {
        return TRUE;
    }
    if (strchr(fontFileName, ':') != NULL || fontFileName[0] == '\\' ||
        fontFileName[0] == '/' || fontFileName[0] == '%') {
        return FALSE;
    }
    return TRUE;
}

static BOOL LoadFontInternal(const char* fontFileName, BOOL updateConfig) {
    if (!fontFileName) {
        return FALSE;
    }

    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) {
        return FALSE;
    }
    if (LoadFontFromFile(fontPath)) {
        return TRUE;
    }
    if (!FontManager_ShouldAttemptAutoFix(fontFileName)) {
        return FALSE;
    }

    FontPathInfo pathInfo;
    if (!AutoFixFontPath(fontFileName, &pathInfo) ||
        !LoadFontFromFile(pathInfo.absolutePath)) {
        return FALSE;
    }

    BOOL persistAutoFix = FALSE;
    char previousFontName[MAX_PATH] = {0};
    if (updateConfig && IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
        if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
            FontManager_CopyStringExact(
                FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
            persistAutoFix = TRUE;
        }
    }

    if (persistAutoFix) {
        strncpy(FONT_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_FILE_NAME) - 1);
        FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
        strncpy(FONT_RUNTIME_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_RUNTIME_FILE_NAME) - 1);
        FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';
        if (!WriteConfigFont(pathInfo.relativePath, FALSE) ||
            !FlushConfigToDisk()) {
            LOG_WARNING("Failed to persist auto-fixed font path: %s",
                        pathInfo.relativePath);
            FontManager_CopyStringExact(
                previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        }
    }
    return TRUE;
}

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    (void)hInstance;
    return LoadFontInternal(fontName, TRUE);
}

BOOL LoadFontByNameAndGetRealName(
    HINSTANCE hInstance, const char* fontFileName,
    char* realFontName, size_t realFontNameSize) {
    if (!fontFileName || !realFontName || realFontNameSize == 0) {
        return FALSE;
    }
    (void)hInstance;

    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) {
        return FALSE;
    }

    wchar_t wideFontPath[MAX_PATH];
    BOOL fontExists = Utf8ToWide(fontPath, wideFontPath, MAX_PATH) &&
                      GetFileAttributesW(wideFontPath) != INVALID_FILE_ATTRIBUTES;
    BOOL persistAutoFix = FALSE;
    FontPathInfo pathInfo = {0};
    char previousFontName[MAX_PATH] = {0};
    if (!fontExists) {
        if (!FontManager_ShouldAttemptAutoFix(fontFileName) ||
            !AutoFixFontPath(fontFileName, &pathInfo)) {
            return FALSE;
        }
        strncpy(fontPath, pathInfo.absolutePath, MAX_PATH - 1);
        fontPath[MAX_PATH - 1] = '\0';
        if (IsFontsFolderPath(FONT_FILE_NAME)) {
            const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
            if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
                FontManager_CopyStringExact(
                    FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
                persistAutoFix = TRUE;
            }
        }
    }

    if (!GetFontNameFromFile(fontPath, realFontName, realFontNameSize)) {
        const char* filename = GetFileNameU8(fontFileName);
        strncpy(realFontName, filename, realFontNameSize - 1);
        realFontName[realFontNameSize - 1] = '\0';
        char* dot = strrchr(realFontName, '.');
        if (dot) {
            *dot = '\0';
        }
    }
    if (!LoadFontFromFile(fontPath)) {
        return FALSE;
    }

    if (persistAutoFix) {
        strncpy(FONT_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_FILE_NAME) - 1);
        FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
        strncpy(FONT_RUNTIME_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_RUNTIME_FILE_NAME) - 1);
        FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';
        if (!WriteConfigFont(pathInfo.relativePath, FALSE) ||
            !FlushConfigToDisk()) {
            LOG_WARNING("Failed to persist auto-fixed font path: %s",
                        pathInfo.relativePath);
            FontManager_CopyStringExact(
                previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        }
    }
    return TRUE;
}
