/**
 * @file font_config.c
 * @brief Font configuration implementation
 */

#include "font/font_config.h"
#include "font/font_path_manager.h"
#include <stdio.h>
#include <windows.h>

/* External references */
extern void GetConfigPath(char* path, size_t size);
extern void ReadConfig(void);
extern BOOL WriteIniString(const char* section, const char* key, const char* value, const char* filePath);

void WriteConfigFont(const char* fontFileName, BOOL shouldReload) {
    if (!fontFileName) return;
    
    char actualFontPath[MAX_PATH];
    char configFontName[MAX_PATH];
    
    /* Try to find font and get relative path */
    if (FindFontInFontsFolder(fontFileName, actualFontPath, MAX_PATH)) {
        char relativePath[MAX_PATH];
        if (CalculateRelativePath(actualFontPath, relativePath, MAX_PATH)) {
            /* Build config-style path */
            BuildFontConfigPath(relativePath, configFontName, MAX_PATH);
        } else {
            /* Fallback: use filename as-is with prefix */
            BuildFontConfigPath(fontFileName, configFontName, MAX_PATH);
        }
    } else {
        /* File not found: use filename as-is with prefix */
        BuildFontConfigPath(fontFileName, configFontName, MAX_PATH);
    }
    
    /* Write to config */
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString("Display", "FONT_FILE_NAME", configFontName, config_path);
    
    /* Reload if requested */
    if (shouldReload) {
        ReadConfig();
    }
}

