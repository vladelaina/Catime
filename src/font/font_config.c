/**
 * @file font_config.c
 * @brief Font configuration implementation
 */

#include "font/font_config.h"
#include "font/font_path_manager.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

/* External references */
extern void GetConfigPath(char* path, size_t size);
extern void ReadConfig(void);

static BOOL IsRawFontConfigValue(const char* fontFileName) {
    if (!fontFileName || !fontFileName[0]) return FALSE;

    if (IsFontsFolderPath(fontFileName)) return TRUE;
    if (strchr(fontFileName, ':')) return TRUE;  /* Drive-qualified absolute path */
    if (fontFileName[0] == '%' || fontFileName[0] == '\\' || fontFileName[0] == '/') return TRUE;

    return FALSE;
}

BOOL WriteConfigFont(const char* fontFileName, BOOL shouldReload) {
    if (!fontFileName) return FALSE;
    
    char configFontName[MAX_PATH];

    if (IsRawFontConfigValue(fontFileName)) {
        if (strlen(fontFileName) >= sizeof(configFontName)) {
            LOG_WARNING("Font config path too long, skipping write");
            return FALSE;
        }
        strcpy_s(configFontName, sizeof(configFontName), fontFileName);
    } else if (!BuildFontConfigPath(fontFileName, configFontName, MAX_PATH)) {
        LOG_WARNING("Font config path too long, skipping write");
        return FALSE;
    }
    
    /* Write to config */
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentConfigValue[MAX_PATH] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "", currentConfigValue,
                  sizeof(currentConfigValue), config_path);

    BOOL runtimeMatches = (strcmp(FONT_FILE_NAME, configFontName) == 0);
    BOOL configMatches = (strcmp(currentConfigValue, configFontName) == 0);

    BOOL writeSucceeded = TRUE;
    if (!configMatches) {
        writeSucceeded = WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME",
                                        configFontName, config_path);
        if (!writeSucceeded) {
            LOG_WARNING("Failed to write font config: %s", configFontName);
            return FALSE;
        }
    }
    
    /* Reload if requested */
    if (shouldReload && (!runtimeMatches || !configMatches)) {
        ReadConfig();
    }

    return TRUE;
}

