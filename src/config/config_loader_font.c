#include "config_loader_internal.h"
#include "config.h"
#include "font/font_ttf_parser.h"
#include <string.h>

void ProcessConfigFontPath(ConfigSnapshot* snapshot, const char* config_path) {
    if (!snapshot || !config_path) return;
    char actualFontFileName[MAX_PATH];
    BOOL isFontsFolderFont = FALSE;
    const char* localappdata_prefix = FONTS_PATH_PREFIX;
    if (_strnicmp(snapshot->fontFileName, localappdata_prefix,
                  strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName,
                snapshot->fontFileName + strlen(localappdata_prefix),
                sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
        isFontsFolderFont = TRUE;
    } else {
        strncpy(actualFontFileName, snapshot->fontFileName,
                sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    if (isFontsFolderFont) {
        BOOL resolvedFontName = FALSE;
        wchar_t wConfigPath[MAX_PATH] = {0};
        if (MultiByteToWideChar(CP_UTF8, 0, config_path, -1,
                                wConfigPath, MAX_PATH) > 0) {
            wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
            if (lastSep) {
                *lastSep = L'\0';
                wchar_t wActualFontFileName[MAX_PATH] = {0};
                if (MultiByteToWideChar(CP_UTF8, 0, actualFontFileName, -1,
                                        wActualFontFileName, MAX_PATH) > 0) {
                    wchar_t wFontPath[MAX_PATH] = {0};
                    int written = _snwprintf_s(wFontPath, MAX_PATH, _TRUNCATE,
                                               L"%s\\resources\\fonts\\%s",
                                               wConfigPath,
                                               wActualFontFileName);
                    if (written >= 0 &&
                        GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
                        char fontPath[MAX_PATH] = {0};
                        if (WideCharToMultiByte(CP_UTF8, 0, wFontPath, -1,
                                                fontPath, MAX_PATH, NULL, NULL) > 0 &&
                            GetFontNameFromFile(fontPath,
                                                snapshot->fontInternalName,
                                                sizeof(snapshot->fontInternalName))) {
                            resolvedFontName = TRUE;
                        }
                    }
                }
            }
        }
        if (!resolvedFontName) {
            char* lastSlash = strrchr(actualFontFileName, '\\');
            const char* filenameOnly = lastSlash ? lastSlash + 1 : actualFontFileName;
            strncpy(snapshot->fontInternalName, filenameOnly,
                    sizeof(snapshot->fontInternalName) - 1);
            snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
            char* dot = strrchr(snapshot->fontInternalName, '.');
            if (dot) *dot = '\0';
        }
    } else {
        strncpy(snapshot->fontInternalName, actualFontFileName,
                sizeof(snapshot->fontInternalName) - 1);
        snapshot->fontInternalName[sizeof(snapshot->fontInternalName) - 1] = '\0';
        char* dot = strrchr(snapshot->fontInternalName, '.');
        if (dot) *dot = '\0';
    }
}
