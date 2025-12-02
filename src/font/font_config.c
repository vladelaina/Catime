/**
 * @file font_config.c
 * @brief Font configuration implementation
 */

#include "font/font_config.h"
#include "font/font_path_manager.h"
#include "log.h"
#include <stdio.h>
#include <windows.h>

/* External references */
extern void GetConfigPath(char* path, size_t size);
extern void ReadConfig(void);
extern BOOL WriteIniString(const char* section, const char* key, const char* value, const char* filePath);

void WriteConfigFont(const char* fontFileName, BOOL shouldReload) {
    if (!fontFileName) return;
    
    char configFontName[MAX_PATH];
    
    /* CRITICAL FIX: Use the exact relative path provided by user selection!
     * DO NOT re-search for the file, as recursive search may find a different
     * file with the same name in a subdirectory, causing incorrect path storage.
     * 
     * The fontFileName parameter is already a correct relative path from the cache,
     * selected by the user from the menu. We just need to add the config prefix.
     */
    
    /* Build config-style path directly from the provided relative path */
    BuildFontConfigPath(fontFileName, configFontName, MAX_PATH);
    
    /* Write to config */
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString("Display", "FONT_FILE_NAME", configFontName, config_path);
    
    /* Reload if requested */
    if (shouldReload) {
        ReadConfig();
    }
}

