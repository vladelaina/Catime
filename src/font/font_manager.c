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
#include "config.h"
#include "../../resource/resource.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

char FONT_FILE_NAME[MAX_PATH] = FONT_FOLDER_PREFIX "Wallpoet Essence.ttf";
char FONT_INTERNAL_NAME[MAX_PATH];
char PREVIEW_FONT_NAME[MAX_PATH] = "";
char PREVIEW_INTERNAL_NAME[MAX_PATH] = "";
BOOL IS_PREVIEWING = FALSE;

static wchar_t CURRENT_LOADED_FONT_PATH[MAX_PATH] = {0};
static BOOL FONT_RESOURCE_LOADED = FALSE;

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
    CURRENT_LOADED_FONT_PATH[0] = 0;
    FONT_RESOURCE_LOADED = FALSE;
    return result;
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
    
    /* Load new font */
    int addResult = AddFontResourceExW(wFontPath, FR_PRIVATE, NULL);
    if (addResult <= 0) {
        return FALSE;
    }
    
    /* Unload previous font if different */
    if (FONT_RESOURCE_LOADED && CURRENT_LOADED_FONT_PATH[0] != 0 && 
        wcscmp(CURRENT_LOADED_FONT_PATH, wFontPath) != 0) {
        RemoveFontResourceExW(CURRENT_LOADED_FONT_PATH, FR_PRIVATE, NULL);
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
    
    /* Direct load failed: try auto-fix */
    FontPathInfo pathInfo;
    if (!AutoFixFontPath(fontFileName, &pathInfo)) {
        return FALSE;
    }
    
    /* Update config if requested and this is the active font */
    if (shouldUpdateConfig && IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
        if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
            /* Update global FONT_FILE_NAME */
            strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
            FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
            
            /* Write to config */
            WriteConfigFont(pathInfo.relativePath, FALSE);
            FlushConfigToDisk();
        }
    }
    
    return LoadFontFromFile(pathInfo.absolutePath);
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
    if (!fontExists) {
        FontPathInfo pathInfo;
        if (AutoFixFontPath(fontFileName, &pathInfo)) {
            strncpy(fontPath, pathInfo.absolutePath, MAX_PATH - 1);
            fontPath[MAX_PATH - 1] = '\0';
            
            /* Update config if this is the active font */
            if (IsFontsFolderPath(FONT_FILE_NAME)) {
                const char* currentRelative = ExtractRelativePath(FONT_FILE_NAME);
                if (currentRelative && strcmp(currentRelative, fontFileName) == 0) {
                    strncpy(FONT_FILE_NAME, pathInfo.configPath, sizeof(FONT_FILE_NAME) - 1);
                    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
                    
                    WriteConfigFont(pathInfo.relativePath, FALSE);
                    FlushConfigToDisk();
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
    return LoadFontFromFile(fontPath);
}

BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /* Update active font filename */
    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    /* Load and extract internal name */
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, FONT_INTERNAL_NAME, 
                                      sizeof(FONT_INTERNAL_NAME))) {
        return FALSE;
    }
    
    /* Write to config (without reload) */
    WriteConfigFont(FONT_FILE_NAME, FALSE);
    
    return TRUE;
}

/* ============================================================================
 * Preview System
 * ============================================================================ */

BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /* Save preview font name */
    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
    
    /* Load and extract internal name */
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, PREVIEW_INTERNAL_NAME, 
                                      sizeof(PREVIEW_INTERNAL_NAME))) {
        return FALSE;
    }
    
    /* Set preview mode */
    IS_PREVIEWING = TRUE;
    return TRUE;
}

void CancelFontPreview(void) {
    /* Clear preview mode */
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
    
    /* Reload original font */
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
        if (relativePath) {
            LoadFontByNameAndGetRealName(hInstance, relativePath, 
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
        }
    } else if (FONT_FILE_NAME[0] != '\0') {
        LoadFontByNameAndGetRealName(hInstance, FONT_FILE_NAME, 
                                    FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    }
}

void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;
    
    /* Commit preview to active font */
    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    /* Write to config */
    WriteConfigFont(FONT_FILE_NAME, FALSE);
    
    /* Clear preview state */
    CancelFontPreview();
}

/* ============================================================================
 * Embedded Font Resources
 * ============================================================================ */

BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath) {
    if (!outputPath) return FALSE;
    
    /* Find resource */
    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) return FALSE;
    
    /* Load resource */
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) return FALSE;
    
    /* Lock resource */
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) return FALSE;
    
    /* Get size */
    DWORD fontLength = SizeofResource(hInstance, hResource);
    if (fontLength == 0) return FALSE;
    
    /* Convert path to wide */
    wchar_t wOutputPath[MAX_PATH];
    if (!Utf8ToWide(outputPath, wOutputPath, MAX_PATH)) return FALSE;
    
    /* Write to file */
    HANDLE hFile = CreateFileW(wOutputPath, GENERIC_WRITE, 0, NULL, 
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile, fontData, fontLength, &bytesWritten, NULL);
    CloseHandle(hFile);
    
    return (result && bytesWritten == fontLength);
}

BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance) {
    /* Get fonts folder path */
    wchar_t wFontsFolderPath[MAX_PATH] = {0};
    if (!GetFontsFolderW(wFontsFolderPath, MAX_PATH, TRUE)) return FALSE;
    
    char fontsFolderPath[MAX_PATH];
    if (!WideToUtf8(wFontsFolderPath, fontsFolderPath, MAX_PATH)) return FALSE;
    
    /* Extract each font */
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        char outputPath[MAX_PATH];
        snprintf(outputPath, MAX_PATH, "%s\\%s", fontsFolderPath, fontResources[i].fontName);
        ExtractFontResourceToFile(hInstance, fontResources[i].resourceId, outputPath);
    }
    
    return TRUE;
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
    SelectObject(hdc, hFont);
    
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);
    
    DeleteObject(hFont);
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

