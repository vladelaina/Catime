/**
 * @file font_manager.c
 * @brief Font loading and management implementation
 */

#include "font/font_manager.h"
#include "font/font_ttf_parser.h"
#include "font/font_path_manager.h"
#include "font/font_config.h"
#include "utils/string_convert.h"
#include "utils/path_utils.h"
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
#include "utils/compressed_resource.h"
#endif
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

char FONT_FILE_NAME[MAX_PATH] = FONT_FOLDER_PREFIX "Wallpoet Essence.ttf";
char FONT_RUNTIME_FILE_NAME[MAX_PATH] = FONT_FOLDER_PREFIX "Wallpoet Essence.ttf";
char FONT_INTERNAL_NAME[MAX_PATH];
char PREVIEW_FONT_NAME[MAX_PATH] = "";
char PREVIEW_INTERNAL_NAME[MAX_PATH] = "";
BOOL IS_PREVIEWING = FALSE;

static wchar_t CURRENT_LOADED_FONT_PATH[MAX_PATH] = {0};
static BOOL FONT_RESOURCE_LOADED = FALSE;

static BOOL CopyStringExactA(const char* src, char* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = '\0';
    if (!src) return FALSE;

    size_t len = strlen(src);
    if (len >= outSize) return FALSE;

    memcpy(out, src, len + 1);
    return TRUE;
}

static BOOL ShouldAttemptFontAutoFix(const char* fontFileName) {
    if (!fontFileName || fontFileName[0] == '\0') return FALSE;
    if (IsFontsFolderPath(fontFileName)) return TRUE;
    if (strchr(fontFileName, ':') != NULL ||
        fontFileName[0] == '\\' ||
        fontFileName[0] == '/' ||
        fontFileName[0] == '%') {
        return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * Embedded Font Resources
 * ============================================================================ */

FontResource fontResources[] = {
    {IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

/* ============================================================================
 * Font Resource Management
 * ============================================================================ */

BOOL UnloadCurrentFontResource(void) {
    if (!FONT_RESOURCE_LOADED || CURRENT_LOADED_FONT_PATH[0] == 0) {
        return TRUE;
    }

    BOOL result = RemoveFontResourceExW(CURRENT_LOADED_FONT_PATH, FR_PRIVATE, NULL);
    if (!result) {
        LOG_WARNING("Failed to unload current font resource: %S (error=%lu)",
                    CURRENT_LOADED_FONT_PATH, GetLastError());
        return FALSE;
    }

    CURRENT_LOADED_FONT_PATH[0] = 0;
    FONT_RESOURCE_LOADED = FALSE;
    return TRUE;
}

BOOL LoadFontFromFile(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;
    
    /* Convert to wide */
    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontFilePath, wFontPath, MAX_PATH)) return FALSE;
    
    /* Check if file exists */
    if (GetFileAttributesW(wFontPath) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    /* Skip if already loaded */
    if (FONT_RESOURCE_LOADED && wcscmp(CURRENT_LOADED_FONT_PATH, wFontPath) == 0) {
        return TRUE;
    }

    wchar_t previousFontPath[MAX_PATH] = {0};
    BOOL hadPreviousFont = FONT_RESOURCE_LOADED && CURRENT_LOADED_FONT_PATH[0] != 0;
    if (hadPreviousFont) {
        wcscpy_s(previousFontPath, MAX_PATH, CURRENT_LOADED_FONT_PATH);
        if (!UnloadCurrentFontResource()) {
            return FALSE;
        }
    }

    /* Load new font */
    int addResult = AddFontResourceExW(wFontPath, FR_PRIVATE, NULL);
    if (addResult <= 0) {
        LOG_WARNING("Failed to load font resource: %S (error=%lu)",
                    wFontPath, GetLastError());
        if (hadPreviousFont) {
            int restoreResult = AddFontResourceExW(previousFontPath, FR_PRIVATE, NULL);
            if (restoreResult > 0) {
                wcscpy_s(CURRENT_LOADED_FONT_PATH, MAX_PATH, previousFontPath);
                FONT_RESOURCE_LOADED = TRUE;
            } else {
                LOG_WARNING("Failed to restore previous font resource: %S (error=%lu)",
                            previousFontPath, GetLastError());
                CURRENT_LOADED_FONT_PATH[0] = 0;
                FONT_RESOURCE_LOADED = FALSE;
            }
        }
        return FALSE;
    }

    /* Save current loaded font */
    wcscpy_s(CURRENT_LOADED_FONT_PATH, MAX_PATH, wFontPath);
    FONT_RESOURCE_LOADED = TRUE;
    return TRUE;
}

/* ============================================================================
 * High-Level Font Loading (with auto-recovery)
 * ============================================================================ */

/**
 * @brief Internal font loading with optional config update
 * @param fontFileName Font filename
 * @param shouldUpdateConfig TRUE to update config if auto-fixed
 * @return TRUE on success
 */
static BOOL LoadFontInternal(const char* fontFileName, BOOL shouldUpdateConfig) {
    if (!fontFileName) return FALSE;
    
    /* Try direct load */
    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) return FALSE;
    
    if (LoadFontFromFile(fontPath)) {
        return TRUE;
    }

    if (!ShouldAttemptFontAutoFix(fontFileName)) {
        return FALSE;
    }

    /* Direct load failed: try auto-fix */
    FontPathInfo pathInfo;
    if (!AutoFixFontPath(fontFileName, &pathInfo)) {
        return FALSE;
    }
    
    BOOL shouldPersistAutoFix = FALSE;
    char previousFontName[MAX_PATH] = {0};
    if (shouldUpdateConfig && IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
        if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
            CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
            shouldPersistAutoFix = TRUE;
        }
    }

    if (!LoadFontFromFile(pathInfo.absolutePath)) {
        return FALSE;
    }

    if (shouldPersistAutoFix) {
        /* Update global FONT_FILE_NAME only after the repaired font is loaded. */
        strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
        FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
        strncpy(FONT_RUNTIME_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_RUNTIME_FILE_NAME) - 1);
        FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';

        if (!WriteConfigFont(pathInfo.relativePath, FALSE) ||
            !FlushConfigToDisk()) {
            LOG_WARNING("Failed to persist auto-fixed font path: %s",
                        pathInfo.relativePath);
            CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        }
    }

    return TRUE;
}

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    (void)hInstance;
    return LoadFontInternal(fontName, TRUE);
}

BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName,
                                  char* realFontName, size_t realFontNameSize) {
    if (!fontFileName || !realFontName || realFontNameSize == 0) return FALSE;
    
    (void)hInstance;
    
    /* Build full path */
    char fontPath[MAX_PATH];
    if (!BuildFullFontPath(fontFileName, fontPath, MAX_PATH)) return FALSE;
    
    /* Check if file exists */
    wchar_t wFontPath[MAX_PATH];
    BOOL fontExists = FALSE;
    if (Utf8ToWide(fontPath, wFontPath, MAX_PATH)) {
        fontExists = (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES);
    }
    
    /* If not exists, try auto-fix */
    BOOL shouldPersistAutoFix = FALSE;
    FontPathInfo pathInfo = {0};
    char previousFontName[MAX_PATH] = {0};
    if (!fontExists) {
        if (!ShouldAttemptFontAutoFix(fontFileName)) {
            return FALSE;
        }

        if (AutoFixFontPath(fontFileName, &pathInfo)) {
            strncpy(fontPath, pathInfo.absolutePath, MAX_PATH - 1);
            fontPath[MAX_PATH - 1] = '\0';

            /* Defer config/global update until the repaired font has loaded. */
            if (IsFontsFolderPath(FONT_FILE_NAME)) {
                const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
                if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
                    CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
                    shouldPersistAutoFix = TRUE;
                }
            }
        } else {
            return FALSE;
        }
    }
    
    /* Extract font name from TTF */
    if (!GetFontNameFromFile(fontPath, realFontName, realFontNameSize)) {
        /* Fallback: use filename without extension */
        const char* filename = GetFileNameU8(fontFileName);
        strncpy(realFontName, filename, realFontNameSize - 1);
        realFontName[realFontNameSize - 1] = '\0';
        
        /* Remove extension */
        char* dot = strrchr(realFontName, '.');
        if (dot) *dot = '\0';
    }
    
    /* Load font */
    if (!LoadFontFromFile(fontPath)) {
        return FALSE;
    }

    if (shouldPersistAutoFix) {
        strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
        FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
        strncpy(FONT_RUNTIME_FILE_NAME, pathInfo.configPath,
                sizeof(FONT_RUNTIME_FILE_NAME) - 1);
        FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';

        if (!WriteConfigFont(pathInfo.relativePath, FALSE) ||
            !FlushConfigToDisk()) {
            LOG_WARNING("Failed to persist auto-fixed font path: %s",
                        pathInfo.relativePath);
            CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        }
    }

    return TRUE;
}

BOOL CheckAndReloadCurrentFontPath(void) {
    if (strcmp(FONT_RUNTIME_FILE_NAME, FONT_FILE_NAME) != 0) {
        char previousRuntimeFontName[MAX_PATH] = {0};
        CopyStringExactA(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName,
                         sizeof(previousRuntimeFontName));
        const char* loadName = FONT_FILE_NAME;
        if (IsFontsFolderPath(FONT_FILE_NAME)) {
            const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
            if (relativePath) loadName = relativePath;
        }

        char loadedInternalName[MAX_PATH] = {0};
        if (!loadName[0] ||
            !LoadFontByNameAndGetRealName(GetModuleHandle(NULL), loadName,
                                          loadedInternalName, sizeof(loadedInternalName))) {
            return FALSE;
        }

        if (strcmp(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName) == 0) {
            CopyStringExactA(FONT_FILE_NAME, FONT_RUNTIME_FILE_NAME,
                             sizeof(FONT_RUNTIME_FILE_NAME));
        }
        CopyStringExactA(loadedInternalName, FONT_INTERNAL_NAME,
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
    if (!BuildFullFontPath(relativePath, fontPath, MAX_PATH)) {
        return FALSE;
    }

    wchar_t wFontPath[MAX_PATH];
    if (!Utf8ToWide(fontPath, wFontPath, MAX_PATH)) {
        return FALSE;
    }

    if (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeFontName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};
    CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    CopyStringExactA(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName,
                     sizeof(previousRuntimeFontName));
    CopyStringExactA(FONT_INTERNAL_NAME, previousInternalName, sizeof(previousInternalName));

    if (!LoadFontByNameAndGetRealName(GetModuleHandle(NULL), relativePath,
                                      loadedInternalName, sizeof(loadedInternalName))) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }

    CopyStringExactA(loadedInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    if (strcmp(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName) == 0) {
        CopyStringExactA(FONT_FILE_NAME, FONT_RUNTIME_FILE_NAME,
                         sizeof(FONT_RUNTIME_FILE_NAME));
    }
    return TRUE;
}

static BOOL ReloadFontFromConfigName(HINSTANCE hInstance,
                                     const char* fontName,
                                     char* outInternalName,
                                     size_t outInternalNameSize) {
    if (!fontName || fontName[0] == '\0' || !outInternalName || outInternalNameSize == 0) {
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

    if (LoadFontByNameAndGetRealName(hInstance, reloadName,
                                     outInternalName, outInternalNameSize)) {
        return TRUE;
    }

    UnloadCurrentFontResource();
    outInternalName[0] = '\0';
    return FALSE;
}

BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeFontName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char pendingFontName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};

    if (!CopyStringExactA(fontName, pendingFontName, sizeof(pendingFontName))) {
        LOG_WARNING("Font name too long, ignoring switch: %s", fontName);
        return FALSE;
    }

    CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    CopyStringExactA(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName,
                     sizeof(previousRuntimeFontName));
    CopyStringExactA(FONT_INTERNAL_NAME, previousInternalName, sizeof(previousInternalName));

    /* Load and extract internal name */
    if (!LoadFontByNameAndGetRealName(hInstance, pendingFontName,
                                      loadedInternalName, sizeof(loadedInternalName))) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        return FALSE;
    }

    CopyStringExactA(pendingFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
    CopyStringExactA(pendingFontName, FONT_RUNTIME_FILE_NAME,
                     sizeof(FONT_RUNTIME_FILE_NAME));
    CopyStringExactA(loadedInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));

    /* Write to config (without reload) */
    if (!WriteConfigFont(FONT_FILE_NAME, FALSE)) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousRuntimeFontName, FONT_RUNTIME_FILE_NAME,
                         sizeof(FONT_RUNTIME_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        if (!ReloadFontFromConfigName(hInstance, previousRuntimeFontName,
                                      FONT_INTERNAL_NAME,
                                      sizeof(FONT_INTERNAL_NAME))) {
            LOG_WARNING("Failed to restore previous font after config write failure: %s",
                        previousFontName);
            CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
        return FALSE;
    }

    return TRUE;
}

/* ============================================================================
 * Preview System
 * ============================================================================ */

static void ClearFontPreviewState(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;

    BOOL hadPreview = IS_PREVIEWING;
    char pendingFontName[MAX_PATH] = {0};
    char loadedInternalName[MAX_PATH] = {0};

    if (!CopyStringExactA(fontName, pendingFontName, sizeof(pendingFontName))) {
        LOG_WARNING("Font preview name too long, ignoring preview: %s", fontName);
        if (hadPreview) {
            CancelFontPreview();
        } else {
            ClearFontPreviewState();
        }
        return FALSE;
    }

    /* Load and extract internal name */
    if (!LoadFontByNameAndGetRealName(hInstance, pendingFontName, loadedInternalName,
                                      sizeof(loadedInternalName))) {
        if (hadPreview) {
            CancelFontPreview();
        } else {
            ClearFontPreviewState();
        }
        return FALSE;
    }

    CopyStringExactA(pendingFontName, PREVIEW_FONT_NAME, sizeof(PREVIEW_FONT_NAME));
    CopyStringExactA(loadedInternalName, PREVIEW_INTERNAL_NAME, sizeof(PREVIEW_INTERNAL_NAME));

    /* Set preview mode */
    IS_PREVIEWING = TRUE;
    return TRUE;
}

void CancelFontPreview(void) {
    /* Clear preview mode */
    ClearFontPreviewState();
    
    /* Reload original font */
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    if (IsFontsFolderPath(FONT_RUNTIME_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_RUNTIME_FILE_NAME);
        if (relativePath) {
            LoadFontByNameAndGetRealName(hInstance, relativePath, 
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
    } else if (FONT_RUNTIME_FILE_NAME[0] != '\0') {
        LoadFontByNameAndGetRealName(hInstance, FONT_RUNTIME_FILE_NAME,
                                     FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    }
}

void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;

    char previousFontName[MAX_PATH] = {0};
    char previousRuntimeFontName[MAX_PATH] = {0};
    char previousInternalName[MAX_PATH] = {0};
    char committedFontName[MAX_PATH] = {0};
    char committedInternalName[MAX_PATH] = {0};
    CopyStringExactA(FONT_FILE_NAME, previousFontName, sizeof(previousFontName));
    CopyStringExactA(FONT_RUNTIME_FILE_NAME, previousRuntimeFontName,
                     sizeof(previousRuntimeFontName));
    CopyStringExactA(FONT_INTERNAL_NAME, previousInternalName, sizeof(previousInternalName));

    if (!CopyStringExactA(PREVIEW_FONT_NAME, committedFontName, sizeof(committedFontName)) ||
        !CopyStringExactA(PREVIEW_INTERNAL_NAME, committedInternalName, sizeof(committedInternalName))) {
        CancelFontPreview();
        return;
    }

    /* Commit preview to active font */
    CopyStringExactA(committedFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
    CopyStringExactA(committedFontName, FONT_RUNTIME_FILE_NAME,
                     sizeof(FONT_RUNTIME_FILE_NAME));
    CopyStringExactA(committedInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));

    /* Write to config */
    if (!WriteConfigFont(FONT_FILE_NAME, FALSE)) {
        CopyStringExactA(previousFontName, FONT_FILE_NAME, sizeof(FONT_FILE_NAME));
        CopyStringExactA(previousRuntimeFontName, FONT_RUNTIME_FILE_NAME,
                         sizeof(FONT_RUNTIME_FILE_NAME));
        CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        if (!ReloadFontFromConfigName(GetModuleHandle(NULL), previousRuntimeFontName,
                                      FONT_INTERNAL_NAME,
                                      sizeof(FONT_INTERNAL_NAME))) {
            LOG_WARNING("Failed to restore previous preview font after config write failure: %s",
                        previousFontName);
            CopyStringExactA(previousInternalName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
        ClearFontPreviewState();
        return;
    }

    /* Preview font is already loaded; keep it active and only clear preview state. */
    ClearFontPreviewState();
}

/* ============================================================================
 * Embedded Font Resources
 * ============================================================================ */

static BOOL WriteFontDataToFile(const void* fontData, size_t fontLength,
                                const char* outputPath) {
    if (!fontData || fontLength == 0 || fontLength > MAXDWORD || !outputPath) {
        return FALSE;
    }

    /* Convert path to wide */
    wchar_t wOutputPath[MAX_PATH];
    if (!Utf8ToWide(outputPath, wOutputPath, MAX_PATH)) return FALSE;

    wchar_t wOutputDir[MAX_PATH];
    if (!ExtractDirectoryW(wOutputPath, wOutputDir, MAX_PATH)) return FALSE;

    wchar_t wTempPath[MAX_PATH];
    if (GetTempFileNameW(wOutputDir, L"ctf", 0, wTempPath) == 0) {
        return FALSE;
    }

    /* Write to a same-directory temp file, then atomically replace target. */
    HANDLE hFile = CreateFileW(wTempPath, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DeleteFileW(wTempPath);
        return FALSE;
    }

    DWORD bytesWritten;
    DWORD writeLength = (DWORD)fontLength;
    BOOL result = WriteFile(hFile, fontData, writeLength, &bytesWritten, NULL) &&
                  bytesWritten == writeLength;
    if (result && !FlushFileBuffers(hFile)) {
        result = FALSE;
    }
    if (!CloseHandle(hFile)) {
        result = FALSE;
    }

    if (result) {
        result = MoveFileExW(wTempPath, wOutputPath,
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
    if (!result) {
        DeleteFileW(wTempPath);
    }

    return result;
}

BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath) {
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(hInstance,
                                      COMPRESSED_RESOURCE_GROUP_FONTS,
                                      &group)) {
        return FALSE;
    }

    const BYTE* fontData = NULL;
    size_t fontLength = 0;
    BOOL result =
        CompressedResource_GetMember(group, (UINT)resourceId, &fontData,
                                     &fontLength, NULL) &&
        WriteFontDataToFile(fontData, fontLength, outputPath);
    CompressedResource_FreeGroup(group);
    return result;
#else
    if (!outputPath) return FALSE;

    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) return FALSE;

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) return FALSE;

    const void* fontData = LockResource(hMemory);
    if (fontData == NULL) return FALSE;

    DWORD fontLength = SizeofResource(hInstance, hResource);
    if (fontLength == 0) return FALSE;

    return WriteFontDataToFile(fontData, (size_t)fontLength, outputPath);
#endif
}

BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance) {
    /* Get fonts folder path */
    wchar_t wFontsFolderPath[MAX_PATH] = {0};
    if (!GetFontsFolderW(wFontsFolderPath, MAX_PATH, TRUE)) return FALSE;
    
    char fontsFolderPath[MAX_PATH];
    if (!WideToUtf8(wFontsFolderPath, fontsFolderPath, MAX_PATH)) return FALSE;
    
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResourceGroup* group = NULL;
    if (!CompressedResource_LoadGroup(hInstance,
                                      COMPRESSED_RESOURCE_GROUP_FONTS,
                                      &group)) {
        return FALSE;
    }
#endif

    /* Extract each font */
    BOOL allExtracted = TRUE;
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        char outputPath[MAX_PATH];
        int outputPathLen = snprintf(outputPath, MAX_PATH, "%s\\%s", fontsFolderPath, fontResources[i].fontName);
        if (outputPathLen < 0 || outputPathLen >= MAX_PATH) {
            LOG_WARNING("Font output path too long: %s", fontResources[i].fontName);
            allExtracted = FALSE;
            continue;
        }
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
        const BYTE* fontData = NULL;
        size_t fontLength = 0;
        BOOL extracted =
            CompressedResource_GetMember(group,
                                         (UINT)fontResources[i].resourceId,
                                         &fontData, &fontLength, NULL) &&
            WriteFontDataToFile(fontData, fontLength, outputPath);
#else
        BOOL extracted = ExtractFontResourceToFile(hInstance,
                                                   fontResources[i].resourceId,
                                                   outputPath);
#endif
        if (!extracted) {
            LOG_WARNING("Failed to extract embedded font: %s", fontResources[i].fontName);
            allExtracted = FALSE;
        }
    }

#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
    CompressedResource_FreeGroup(group);
#endif

    return allExtracted;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    if (!hdc) return;
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;
    
    HFONT hFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              lf.lfCharSet, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, NULL);
    HFONT oldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;

    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);

    if (oldFont) SelectObject(hdc, oldFont);
    if (hFont) DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme,
                               DWORD FontType, LPARAM lParam) {
    (void)lpelfe;
    (void)lpntme;
    (void)FontType;
    (void)lParam;
    return 1;
}

