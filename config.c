#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) return;
    
    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;
    
    while (fgets(line, sizeof(line), file) && CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
        // ... rest of the existing implementation ...
    }
    
    fclose(file);
    
    // ... remaining code ...
}

void SaveRecentFile(const char* filePath) {
    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);
    
    // ... rest of the existing implementation ...
    
    free(config_content);
    free(new_config);
} 