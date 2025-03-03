#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>

void GetConfigPath(char* path, size_t size) {
    if (!path || size == 0) return;

    char* appdata_path = getenv("LOCALAPPDATA");
    if (appdata_path) {
        if (snprintf(path, size, "%s\\Catime\\config.txt", appdata_path) >= size) {
            strncpy(path, ".\\asset\\config.txt", size - 1);
            path[size - 1] = '\0';
            return;
        }
        
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime", appdata_path) < sizeof(dir_path)) {
            if (!CreateDirectoryA(dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\asset\\config.txt", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\asset\\config.txt", size - 1);
        path[size - 1] = '\0';
    }
}

void CreateDefaultConfig(const char* config_path) {
    FILE *file = fopen(config_path, "w");
    if (file) {
        fprintf(file, "FONT_FILE_NAME=Wallpoet Essence.ttf\n");  
        fprintf(file, "CLOCK_TEXT_COLOR=#FFB6C1\n");
        fprintf(file, "CLOCK_BASE_FONT_SIZE=20\n");
        fprintf(file, "CLOCK_WINDOW_POS_X=960\n");  
        fprintf(file, "CLOCK_WINDOW_POS_Y=-1\n");   
        fprintf(file, "WINDOW_SCALE=1.62\n");
        fprintf(file, "CLOCK_DEFAULT_START_TIME=1500\n");
        fprintf(file, "CLOCK_TIME_OPTIONS=25,10,5\n");
        fprintf(file, "CLOCK_TIMEOUT_TEXT=0\n");
        fprintf(file, "CLOCK_EDIT_MODE=FALSE\n");
        fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");
        fprintf(file, "CLOCK_USE_24HOUR=FALSE\n");
        fprintf(file, "CLOCK_SHOW_SECONDS=FALSE\n");

        
        fprintf(file, "COLOR_OPTIONS=#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174\n");


        fclose(file);
    }
}

void ReadConfig() {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        CreateDefaultConfig(config_path);
        file = fopen(config_path, "r");
        if (!file) {
            fprintf(stderr, "Failed to open config file after creation: %s\n", config_path);
            return;
        }
    }

    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        if (strncmp(line, "COLOR_OPTIONS=", 13) == 0) {
            continue;
        }

        if (strncmp(line, "CLOCK_TIME_OPTIONS=", 19) == 0) {
            char *token = strtok(line + 19, ",");
            while (token && time_options_count < MAX_TIME_OPTIONS) {
                while (*token == ' ') token++;
                time_options[time_options_count++] = atoi(token);
                token = strtok(NULL, ",");
            }
        }
        else if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            strncpy(FONT_FILE_NAME, line + 15, sizeof(FONT_FILE_NAME) - 1);
            FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
            
            size_t name_len = strlen(FONT_FILE_NAME);
            if (name_len > 4 && strcmp(FONT_FILE_NAME + name_len - 4, ".ttf") == 0) {
                strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, name_len - 4);
                FONT_INTERNAL_NAME[name_len - 4] = '\0';
            } else {
                strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
                FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
            }
        }
        else if (strncmp(line, "CLOCK_TEXT_COLOR=", 17) == 0) {
            strncpy(CLOCK_TEXT_COLOR, line + 17, sizeof(CLOCK_TEXT_COLOR) - 1);
            CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
        }
        else if (strncmp(line, "CLOCK_DEFAULT_START_TIME=", 25) == 0) {
            sscanf(line + 25, "%d", &CLOCK_DEFAULT_START_TIME);
        }
        else if (strncmp(line, "CLOCK_WINDOW_POS_X=", 19) == 0) {
            sscanf(line + 19, "%d", &CLOCK_WINDOW_POS_X);
        }
        else if (strncmp(line, "CLOCK_WINDOW_POS_Y=", 19) == 0) {
            sscanf(line + 19, "%d", &CLOCK_WINDOW_POS_Y);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_TEXT=", 19) == 0) {
            sscanf(line + 19, "%49[^\n]", CLOCK_TIMEOUT_TEXT);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 20) == 0) {
            char action[8] = {0};
            sscanf(line + 20, "%7s", action);
            if (strcmp(action, "MESSAGE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            } else if (strcmp(action, "LOCK") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
            } else if (strcmp(action, "SHUTDOWN") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
            } else if (strcmp(action, "RESTART") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
            } else if (strcmp(action, "OPEN_FILE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
            }
        }
        else if (strncmp(line, "CLOCK_EDIT_MODE=", 15) == 0) {
            char edit_mode[8] = {0};
            sscanf(line + 15, "%7s", edit_mode);
            if (strcmp(edit_mode, "TRUE") == 0) {
                CLOCK_EDIT_MODE = TRUE;
            } else if (strcmp(edit_mode, "FALSE") == 0) {
                CLOCK_EDIT_MODE = FALSE;
            }
        }
        else if (strncmp(line, "WINDOW_SCALE=", 13) == 0) {
            CLOCK_WINDOW_SCALE = atof(line + 13);
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) == 0) {
            char *path = line + 19;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';
            
            while (*path == '=' || *path == ' ' || *path == '"') path++;
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '"') path[len-1] = '\0';
            
            if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_TIMEOUT_FILE_PATH, path, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                
                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                }
            } else {
                memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            }
        }
        else if (strncmp(line, "COLOR_OPTIONS=", 14) == 0) {
            char* token = strtok(line + 14, ",");
            while (token) {
                COLOR_OPTIONS = realloc(COLOR_OPTIONS, sizeof(PredefinedColor) * (COLOR_OPTIONS_COUNT + 1));
                if (COLOR_OPTIONS) {
                    COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = strdup(token);
                    COLOR_OPTIONS_COUNT++;
                }
                token = strtok(NULL, ",");
            }
        }
        else if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
            sscanf(line, "STARTUP_MODE=%19s", CLOCK_STARTUP_MODE);
        }
        else if (strncmp(line, "CLOCK_USE_24HOUR=", 17) == 0) {
            CLOCK_USE_24HOUR = (strncmp(line + 17, "TRUE", 4) == 0);
        }
        else if (strncmp(line, "CLOCK_SHOW_SECONDS=", 19) == 0) {
            CLOCK_SHOW_SECONDS = (strncmp(line + 19, "TRUE", 4) == 0);
        }
    }

    fclose(file);
    last_config_time = time(NULL);

    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    LoadRecentFiles();
}

void WriteConfigTimeoutAction(const char* action) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE* temp = fopen(temp_path, "w");
    FILE* file = fopen(config_path, "r");
    
    if (!temp || !file) {
        if (temp) fclose(temp);
        if (file) fclose(file);
        return;
    }

    char line[MAX_PATH];
    int success = 1;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 20) != 0 && 
            strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) != 0) {
            if (fputs(line, temp) == EOF) {
                success = 0;
                break;
            }
        }
    }

    if (success) {
        if (fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action) < 0) {
            success = 0;
        }
    }
    
    if (success && strcmp(action, "OPEN_FILE") == 0 && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        if (GetFileAttributes(CLOCK_TIMEOUT_FILE_PATH) != INVALID_FILE_ATTRIBUTES) {
            if (fprintf(temp, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH) < 0) {
                success = 0;
            }
        }
    }

    fclose(file);
    fclose(temp);

    if (success) {
        remove(config_path);
        rename(temp_path, config_path);
    } else {
        remove(temp_path);
    }
}

void WriteConfigEditMode(const char* mode) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    char temp_path[MAX_PATH];
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_EDIT_MODE=", 15) == 0) {
            fprintf(temp_file, "CLOCK_EDIT_MODE=%s\n", mode);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    if (!found) {
        fprintf(temp_file, "CLOCK_EDIT_MODE=%s\n", mode);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

void WriteConfigTimeOptions(const char* options) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIME_OPTIONS=", 19) == 0) {
            fprintf(temp_file, "CLOCK_TIME_OPTIONS=%s\n", options);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    if (!found) {
        fprintf(temp_file, "CLOCK_TIME_OPTIONS=%s\n", options);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    FILE *file = fopen(config_path, "r");
    if (!file) return;

    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_RECENT_FILE=", 19) == 0) {
            char *path = line + 19;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';

            if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, path, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';

                char *filename = strrchr(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, '\\');
                if (filename) filename++;
                else filename = CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path;
                
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, filename, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name[MAX_PATH - 1] = '\0';

                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }

    fclose(file);
}

void SaveRecentFile(const char* filePath) {
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
            for (int j = i; j < CLOCK_RECENT_FILES_COUNT - 1; j++) {
                strcpy(CLOCK_RECENT_FILES[j].path, CLOCK_RECENT_FILES[j + 1].path);
                strcpy(CLOCK_RECENT_FILES[j].name, CLOCK_RECENT_FILES[j + 1].name);
            }
            CLOCK_RECENT_FILES_COUNT--;
            break;
        }
    }

    if (CLOCK_RECENT_FILES_COUNT == MAX_RECENT_FILES) {
        for (int i = 0; i < MAX_RECENT_FILES - 1; i++) {
            strcpy(CLOCK_RECENT_FILES[i].path, CLOCK_RECENT_FILES[i + 1].path);
            strcpy(CLOCK_RECENT_FILES[i].name, CLOCK_RECENT_FILES[i + 1].name);
        }
        CLOCK_RECENT_FILES_COUNT--;
    }

    strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, filePath, MAX_PATH - 1);
    CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';

    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);

    wchar_t* wFilename = wcsrchr(wFilePath, L'\\');
    if (wFilename) {
        wFilename++;  
        WideCharToMultiByte(CP_UTF8, 0, wFilename, -1,
                           CLOCK_RECENT_FILES[0].name,
                           MAX_PATH, NULL, NULL);
    } else {
        WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1,
                           CLOCK_RECENT_FILES[0].name,
                           MAX_PATH, NULL, NULL);
    }
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) return;
    
    char *config_content = NULL;
    long file_size;
    
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    config_content = (char *)malloc(file_size + MAX_RECENT_FILES * (MAX_PATH + 20));
    if (!config_content) {
        fclose(file);
        return;
    }
    
    size_t bytes_read = fread(config_content, 1, file_size, file);
    config_content[bytes_read] = '\0';
    fclose(file);
    
    char *new_config = (char *)malloc(strlen(config_content) + MAX_RECENT_FILES * (MAX_PATH + 20));
    if (!new_config) {
        free(config_content);
        return;
    }
    new_config[0] = '\0';
    
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "CLOCK_RECENT_FILE", 16) != 0 && 
            strncmp(line, "CLOCK_TIMEOUT_FILE", 17) != 0 &&
            strncmp(line, "CLOCK_TIMEOUT_ACTION", 19) != 0) {
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }
    
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char recent_file_line[MAX_PATH + 20];
        snprintf(recent_file_line, sizeof(recent_file_line), 
                "CLOCK_RECENT_FILE=%s\n", CLOCK_RECENT_FILES[i].path);
        strcat(new_config, recent_file_line);
    }

    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        strcat(new_config, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        
        char timeout_file_line[MAX_PATH + 20];
        char clean_path[MAX_PATH];
        strncpy(clean_path, CLOCK_TIMEOUT_FILE_PATH, MAX_PATH - 1);
        clean_path[MAX_PATH - 1] = '\0';
        
        char* p = clean_path;
        while (*p == '=' || *p == ' ') p++;
        
        snprintf(timeout_file_line, sizeof(timeout_file_line),
                "CLOCK_TIMEOUT_FILE=%s\n", p);
        strcat(new_config, timeout_file_line);
    }
    
    file = fopen(config_path, "w");
    if (file) {
        fputs(new_config, file);
        fclose(file);
    }
    
    free(config_content);
    free(new_config);
}

char* UTF8ToANSI(const char* utf8Str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wlen == 0) {
        return _strdup(utf8Str);
    }

    wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);
    if (!wstr) {
        return _strdup(utf8Str);
    }

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr, wlen) == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    int len = WideCharToMultiByte(936, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    char* str = (char*)malloc(len);
    if (!str) {
        free(wstr);
        return _strdup(utf8Str);
    }

    if (WideCharToMultiByte(936, 0, wstr, -1, str, len, NULL, NULL) == 0) {
        free(wstr);
        free(str);
        return _strdup(utf8Str);
    }

    free(wstr);
    return str;
}
