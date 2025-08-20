/**
 * @file font.c
 * @brief Font management and embedded font resource handling
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/font.h"
#include "../resource/resource.h"

/** @brief Current font file name */
char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";
/** @brief Internal font name for Windows GDI */
char FONT_INTERNAL_NAME[100];
/** @brief Preview font file name during font selection */
char PREVIEW_FONT_NAME[100] = "";
/** @brief Preview internal font name for Windows GDI */
char PREVIEW_INTERNAL_NAME[100] = "";
/** @brief Flag indicating if font preview mode is active */
BOOL IS_PREVIEWING = FALSE;

/** @brief Array of embedded font resources mapping control IDs to resource IDs and filenames */
FontResource fontResources[] = {
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_ARBUTUS, IDR_FONT_ARBUTUS, "Arbutus Essence.ttf"},
    {CLOCK_IDC_FONT_BERKSHIRE, IDR_FONT_BERKSHIRE, "Berkshire Swash Essence.ttf"},
    {CLOCK_IDC_FONT_CAVEAT, IDR_FONT_CAVEAT, "Caveat Brush Essence.ttf"},
    {CLOCK_IDC_FONT_CREEPSTER, IDR_FONT_CREEPSTER, "Creepster Essence.ttf"},
    {CLOCK_IDC_FONT_DOTGOTHIC, IDR_FONT_DOTGOTHIC, "DotGothic16 Essence.ttf"},
    {CLOCK_IDC_FONT_DOTO, IDR_FONT_DOTO, "Doto ExtraBold Essence.ttf"},
    {CLOCK_IDC_FONT_FOLDIT, IDR_FONT_FOLDIT, "Foldit SemiBold Essence.ttf"},
    {CLOCK_IDC_FONT_FREDERICKA, IDR_FONT_FREDERICKA, "Fredericka the Great Essence.ttf"},
    {CLOCK_IDC_FONT_FRIJOLE, IDR_FONT_FRIJOLE, "Frijole Essence.ttf"},
    {CLOCK_IDC_FONT_GWENDOLYN, IDR_FONT_GWENDOLYN, "Gwendolyn Essence.ttf"},
    {CLOCK_IDC_FONT_HANDJET, IDR_FONT_HANDJET, "Handjet Essence.ttf"},
    {CLOCK_IDC_FONT_INKNUT, IDR_FONT_INKNUT, "Inknut Antiqua Medium Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARD, IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARDA, IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {CLOCK_IDC_FONT_KAVOON, IDR_FONT_KAVOON, "Kavoon Essence.ttf"},
    {CLOCK_IDC_FONT_KUMAR_ONE_OUTLINE, IDR_FONT_KUMAR_ONE_OUTLINE, "Kumar One Outline Essence.ttf"},
    {CLOCK_IDC_FONT_KUMAR_ONE, IDR_FONT_KUMAR_ONE, "Kumar One Essence.ttf"},
    {CLOCK_IDC_FONT_LAKKI_REDDY, IDR_FONT_LAKKI_REDDY, "Lakki Reddy Essence.ttf"},
    {CLOCK_IDC_FONT_LICORICE, IDR_FONT_LICORICE, "Licorice Essence.ttf"},
    {CLOCK_IDC_FONT_MA_SHAN_ZHENG, IDR_FONT_MA_SHAN_ZHENG, "Ma Shan Zheng Essence.ttf"},
    {CLOCK_IDC_FONT_MOIRAI_ONE, IDR_FONT_MOIRAI_ONE, "Moirai One Essence.ttf"},
    {CLOCK_IDC_FONT_MYSTERY_QUEST, IDR_FONT_MYSTERY_QUEST, "Mystery Quest Essence.ttf"},
    {CLOCK_IDC_FONT_NOTO_NASTALIQ, IDR_FONT_NOTO_NASTALIQ, "Noto Nastaliq Urdu Medium Essence.ttf"},
    {CLOCK_IDC_FONT_PIEDRA, IDR_FONT_PIEDRA, "Piedra Essence.ttf"},
    {CLOCK_IDC_FONT_PINYON_SCRIPT, IDR_FONT_PINYON_SCRIPT, "Pinyon Script Essence.ttf"},
    {CLOCK_IDC_FONT_PIXELIFY, IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {CLOCK_IDC_FONT_PRESS_START, IDR_FONT_PRESS_START, "Press Start 2P Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BUBBLES, IDR_FONT_RUBIK_BUBBLES, "Rubik Bubbles Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BURNED, IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_GLITCH, IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_MARKER_HATCH, IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_PUDDLES, IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_VINYL, IDR_FONT_RUBIK_VINYL, "Rubik Vinyl Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_WET_PAINT, IDR_FONT_RUBIK_WET_PAINT, "Rubik Wet Paint Essence.ttf"},
    {CLOCK_IDC_FONT_RUGE_BOOGIE, IDR_FONT_RUGE_BOOGIE, "Ruge Boogie Essence.ttf"},
    {CLOCK_IDC_FONT_SEVILLANA, IDR_FONT_SEVILLANA, "Sevillana Essence.ttf"},
    {CLOCK_IDC_FONT_SILKSCREEN, IDR_FONT_SILKSCREEN, "Silkscreen Essence.ttf"},
    {CLOCK_IDC_FONT_STICK, IDR_FONT_STICK, "Stick Essence.ttf"},
    {CLOCK_IDC_FONT_UNDERDOG, IDR_FONT_UNDERDOG, "Underdog Essence.ttf"},
    {CLOCK_IDC_FONT_WALLPOET, IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {CLOCK_IDC_FONT_YESTERYEAR, IDR_FONT_YESTERYEAR, "Yesteryear Essence.ttf"},
    {CLOCK_IDC_FONT_ZCOOL_KUAILE, IDR_FONT_ZCOOL_KUAILE, "ZCOOL KuaiLe Essence.ttf"},
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

/** @brief Total number of embedded font resources */
const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

/** @brief External reference to clock text color configuration */
extern char CLOCK_TEXT_COLOR[];

/** @brief External function to get configuration file path */
extern void GetConfigPath(char* path, size_t maxLen);
/** @brief External function to reload configuration */
extern void ReadConfig(void);
/** @brief Font enumeration callback function */
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

/**
 * @brief Load font from embedded resource into memory
 * @param hInstance Application instance handle
 * @param resourceId Resource ID of the font to load
 * @return TRUE if font loaded successfully, FALSE otherwise
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    /** Find font resource in executable */
    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    /** Load resource into memory */
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    /** Lock resource data for access */
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    /** Add font to system font table from memory */
    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    
    if (handle == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief Load font from file on disk
 * @param fontFilePath Full path to font file
 * @return TRUE if font loaded successfully, FALSE otherwise
 */
BOOL LoadFontFromFile(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;
    
    /** Convert to wide character for Unicode support */
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontFilePath, -1, wFontPath, MAX_PATH);
    
    /** Check if file exists */
    if (GetFileAttributesW(wFontPath) == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    /** Add font from file to system font table */
    int result = AddFontResourceExW(wFontPath, FR_PRIVATE, NULL);
    return (result > 0);
}

/**
 * @brief Find font file in fonts folder and subfolders
 * @param fontFileName Font filename to search for
 * @param foundPath Buffer to store found font path
 * @param foundPathSize Size of foundPath buffer
 * @return TRUE if font file found, FALSE otherwise
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize) {
    if (!fontFileName || !foundPath || foundPathSize == 0) return FALSE;
    
    /** Helper function to recursively search for font file */
    BOOL SearchFontRecursive(const char* folderPath, const char* targetFile, char* resultPath, size_t resultSize) {
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", folderPath);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath, &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                /** Skip . and .. entries */
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                char fullItemPath[MAX_PATH];
                snprintf(fullItemPath, MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
                
                /** Check if this is the target font file */
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (stricmp(findData.cFileName, targetFile) == 0) {
                        strncpy(resultPath, fullItemPath, resultSize - 1);
                        resultPath[resultSize - 1] = '\0';
                        FindClose(hFind);
                        return TRUE;
                    }
                }
                /** Recursively search subdirectories */
                else if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (SearchFontRecursive(fullItemPath, targetFile, resultPath, resultSize)) {
                        FindClose(hFind);
                        return TRUE;
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return FALSE;
    }
    
    /** Get fonts folder path */
    char fontsFolderPath[MAX_PATH];
    char* appdata_path = getenv("LOCALAPPDATA");
    if (!appdata_path) return FALSE;
    
    snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
    
    /** Search for font file recursively */
    return SearchFontRecursive(fontsFolderPath, fontFileName, foundPath, foundPathSize);
}

/**
 * @brief Load font by name from embedded resources or fonts folder
 * @param hInstance Application instance handle
 * @param fontName Font filename to search for
 * @return TRUE if font found and loaded, FALSE otherwise
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    /** First try embedded resources */
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    
    /** If not found in embedded resources, try fonts folder */
    char fontPath[MAX_PATH];
    if (FindFontInFontsFolder(fontName, fontPath, MAX_PATH)) {
        return LoadFontFromFile(fontPath);
    }
    
    return FALSE;
}

/**
 * @brief Write font configuration to config file
 * @param font_file_name Font filename to save in configuration
 */
void WriteConfigFont(const char* font_file_name) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert path to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    /** Read existing config file */
    FILE *file = _wfopen(wconfig_path, L"r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

    /** Get file size and allocate buffer */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *config_content = (char *)malloc(file_size + 1);
    if (!config_content) {
        fprintf(stderr, "Memory allocation failed!\n");
        fclose(file);
        return;
    }
    fread(config_content, sizeof(char), file_size, file);
    config_content[file_size] = '\0';
    fclose(file);

    /** Allocate buffer for modified config */
    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

    /** Process each line and update FONT_FILE_NAME */
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            /** Replace font file name line */
            strcat(new_config, "FONT_FILE_NAME=");
            strcat(new_config, font_file_name);
            strcat(new_config, "\n");
        } else {
            /** Keep existing line */
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }

    free(config_content);

    /** Write updated config back to file */
    file = _wfopen(wconfig_path, L"w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    /** Reload configuration to apply changes */
    ReadConfig();
}

/**
 * @brief Enumerate all available system fonts
 */
void ListAvailableFonts(void) {
    /** Get device context for font enumeration */
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    /** Create temporary font for enumeration context */
    HFONT hFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              lf.lfCharSet, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    /** Enumerate all font families */
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, 0, 0);

    /** Clean up resources */
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

/**
 * @brief Font enumeration callback procedure
 * @param lpelfe Extended logical font information
 * @param lpntme Font metrics information
 * @param FontType Font type flags
 * @param lParam User-defined parameter
 * @return 1 to continue enumeration, 0 to stop
 */
int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEXW *lpelfe,
    NEWTEXTMETRICEX *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    /** Continue enumeration (placeholder implementation) */
    return 1;
}

/**
 * @brief Start font preview mode with specified font
 * @param hInstance Application instance handle
 * @param fontName Font filename to preview
 * @return TRUE if preview started successfully, FALSE otherwise
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /** Copy font name for preview */
    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
    
    /** Extract internal name by removing .ttf extension */
    size_t name_len = strlen(PREVIEW_FONT_NAME);
    if (name_len > 4 && strcmp(PREVIEW_FONT_NAME + name_len - 4, ".ttf") == 0) {
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(PREVIEW_INTERNAL_NAME))
            copy_len = sizeof(PREVIEW_INTERNAL_NAME) - 1;
        
        memcpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, copy_len);
        PREVIEW_INTERNAL_NAME[copy_len] = '\0';
    } else {
        /** Use full name if no .ttf extension */
        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, sizeof(PREVIEW_INTERNAL_NAME) - 1);
        PREVIEW_INTERNAL_NAME[sizeof(PREVIEW_INTERNAL_NAME) - 1] = '\0';
    }
    
    /** Load font for preview */
    if (!LoadFontByName(hInstance, PREVIEW_FONT_NAME)) {
        return FALSE;
    }
    
    /** Enable preview mode */
    IS_PREVIEWING = TRUE;
    return TRUE;
}

/**
 * @brief Cancel font preview and return to current font
 */
void CancelFontPreview(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

/**
 * @brief Apply previewed font as the current font
 */
void ApplyFontPreview(void) {
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;
    
    /** Copy preview font to current font variables */
    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    /** Save to configuration and exit preview mode */
    WriteConfigFont(FONT_FILE_NAME);
    CancelFontPreview();
}

/**
 * @brief Switch to a different font permanently
 * @param hInstance Application instance handle
 * @param fontName Font filename to switch to
 * @return TRUE if font switched successfully, FALSE otherwise
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    /** Load the new font */
    if (!LoadFontByName(hInstance, fontName)) {
        return FALSE;
    }
    
    /** Update current font name */
    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    /** Extract internal name by removing .ttf extension */
    size_t name_len = strlen(FONT_FILE_NAME);
    if (name_len > 4 && strcmp(FONT_FILE_NAME + name_len - 4, ".ttf") == 0) {
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(FONT_INTERNAL_NAME))
            copy_len = sizeof(FONT_INTERNAL_NAME) - 1;
            
        memcpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, copy_len);
        FONT_INTERNAL_NAME[copy_len] = '\0';
    } else {
        /** Use full name if no .ttf extension */
        strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }
    
    /** Save new font to configuration */
    WriteConfigFont(FONT_FILE_NAME);
    return TRUE;
}