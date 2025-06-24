/**
 * @file font.c
 * @brief Font management module implementation file
 * 
 * This file implements the font management functionality of the application, including font loading, preview,
 * application and configuration file management. Supports loading multiple predefined fonts from resources.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/font.h"
#include "../resource/resource.h"

/// @name Global font variables
/// @{
char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";  ///< Currently used font file name
char FONT_INTERNAL_NAME[100];                     ///< Font internal name (without extension)
char PREVIEW_FONT_NAME[100] = "";                 ///< Preview font file name
char PREVIEW_INTERNAL_NAME[100] = "";             ///< Preview font internal name
BOOL IS_PREVIEWING = FALSE;                       ///< Whether font preview is active
/// @}

/**
 * @brief Font resource array
 * 
 * Stores information for all built-in font resources in the application
 */
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

/// Number of font resources
const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

/// @name External variable declarations
/// @{
extern char CLOCK_TEXT_COLOR[];  ///< Current clock text color
/// @}

/// @name External function declarations
/// @{
extern void GetConfigPath(char* path, size_t maxLen);             ///< Get configuration file path
extern void ReadConfig(void);                                  ///< Read configuration file
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam); ///< Font enumeration callback function
/// @}

/**
 * @brief Load font from resource
 * @param hInstance Application instance handle
 * @param resourceId Font resource ID
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Load font from application resources and add it to the system font collection.
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    // Find font resource
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    // Load resource into memory
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    // Lock resource
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    // Get resource size and add font
    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    
    if (handle == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief Load font by name
 * @param hInstance Application instance handle
 * @param fontName Font file name
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Search for a font with the specified name in the predefined font resource list and load it.
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    // Iterate through the font resource array to find a matching font
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    return FALSE;
}

/**
 * @brief Write font name to configuration file
 * @param font_file_name Font file name to write
 * 
 * Update font settings in the configuration file, preserving other configuration items.
 */
void WriteConfigFont(const char* font_file_name) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Open configuration file for reading
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

    // Read the entire configuration file content
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

    // Create new configuration file content
    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

    // Process line by line and replace font settings
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            strcat(new_config, "FONT_FILE_NAME=");
            strcat(new_config, font_file_name);
            strcat(new_config, "\n");
        } else {
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }

    free(config_content);

    // Write new configuration content
    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    // Re-read configuration
    ReadConfig();
}

/**
 * @brief List available fonts in the system
 * 
 * Enumerate all available fonts in the system, processing font information through callback function.
 */
void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    // Create temporary font and enumerate fonts
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             lf.lfCharSet, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, 0, 0);

    // Clean up resources
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

/**
 * @brief Font enumeration callback function
 * @param lpelfe Font enumeration information
 * @param lpntme Font metrics information
 * @param FontType Font type
 * @param lParam Callback parameter
 * @return int Return 1 to continue enumeration
 * 
 * Called by EnumFontFamiliesEx, processes each enumerated font information.
 */
int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEX *lpelfe,
    NEWTEXTMETRICEX *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    return 1;
}

/**
 * @brief Preview font
 * @param hInstance Application instance handle
 * @param fontName Font name to preview
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Load and set preview font, but don't apply to configuration file.
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    // Save current font name
    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
    
    // Get internal font name (remove .ttf extension)
    size_t name_len = strlen(PREVIEW_FONT_NAME);
    if (name_len > 4 && strcmp(PREVIEW_FONT_NAME + name_len - 4, ".ttf") == 0) {
        // Ensure target size is sufficient, avoid depending on source string length
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(PREVIEW_INTERNAL_NAME))
            copy_len = sizeof(PREVIEW_INTERNAL_NAME) - 1;
        
        memcpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, copy_len);
        PREVIEW_INTERNAL_NAME[copy_len] = '\0';
    } else {
        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, sizeof(PREVIEW_INTERNAL_NAME) - 1);
        PREVIEW_INTERNAL_NAME[sizeof(PREVIEW_INTERNAL_NAME) - 1] = '\0';
    }
    
    // Load preview font
    if (!LoadFontByName(hInstance, PREVIEW_FONT_NAME)) {
        return FALSE;
    }
    
    IS_PREVIEWING = TRUE;
    return TRUE;
}

/**
 * @brief Cancel font preview
 * 
 * Clear preview state and restore to the currently set font.
 */
void CancelFontPreview(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

/**
 * @brief Apply font preview
 * 
 * Set the currently previewed font as the actual font in use, and write to configuration file.
 */
void ApplyFontPreview(void) {
    // Check if there is a valid preview font
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;
    
    // Update current font
    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    // Save to configuration file and cancel preview state
    WriteConfigFont(FONT_FILE_NAME);
    CancelFontPreview();
}

/**
 * @brief Switch font
 * @param hInstance Application instance handle
 * @param fontName Font name to switch to
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Switch directly to the specified font, without going through the preview process.
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    // Load new font
    if (!LoadFontByName(hInstance, fontName)) {
        return FALSE;
    }
    
    // Update font name
    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    // Update internal font name (remove .ttf extension)
    size_t name_len = strlen(FONT_FILE_NAME);
    if (name_len > 4 && strcmp(FONT_FILE_NAME + name_len - 4, ".ttf") == 0) {
        // Ensure target size is sufficient, avoid depending on source string length
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(FONT_INTERNAL_NAME))
            copy_len = sizeof(FONT_INTERNAL_NAME) - 1;
            
        memcpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, copy_len);
        FONT_INTERNAL_NAME[copy_len] = '\0';
    } else {
        strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }
    
    // Write to configuration file
    WriteConfigFont(FONT_FILE_NAME);
    return TRUE;
}