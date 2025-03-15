/**
 * @file config.c
 * @brief 配置文件管理实现
 * 
 * 本文件实现配置文件的路径获取、创建、读写等管理功能，
 * 包含默认配置生成、配置持久化、最近文件记录等功能。
 */

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

// 修改全局变量的默认值(改为秒)
extern int POMODORO_WORK_TIME;      // 默认工作时间25分钟(1500秒)
extern int POMODORO_SHORT_BREAK;     // 默认短休息5分钟(300秒)
extern int POMODORO_LONG_BREAK;      // 默认长休息10分钟(600秒)
extern int POMODORO_LOOP_COUNT;      // 默认循环次数1次

/**
 * @brief 获取配置文件路径
 * @param path 存储路径的缓冲区
 * @param size 缓冲区大小
 * 
 * 优先获取LOCALAPPDATA环境变量路径，若不存在则使用程序目录。
 * 自动创建配置目录，失败时回退到本地路径。
 */
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

/**
 * @brief 创建默认配置文件
 * @param config_path 配置文件路径
 * 
 * 生成包含字体、颜色、窗口位置等默认参数的配置文件。
 */
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
        fprintf(file, "WINDOW_TOPMOST=TRUE\n");

        
        fprintf(file, "COLOR_OPTIONS=#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174\n");

        // 添加番茄钟默认时间配置(单位:秒)
        fprintf(file, "POMODORO_WORK_TIME=1500\n");     // 25分钟
        fprintf(file, "POMODORO_SHORT_BREAK=300\n");    // 5分钟
        fprintf(file, "POMODORO_LONG_BREAK=600\n");     // 10分钟
        fprintf(file, "POMODORO_LOOP_COUNT=1\n");       // 1次

        fclose(file);
    }
}

/**
 * @brief 读取并解析配置文件
 * 
 * 从配置路径读取配置，若不存在则创建默认配置。
 * 解析各配置项并更新程序状态，最后刷新窗口位置。
 */
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
    
    // 重置最近文件计数
    CLOCK_RECENT_FILES_COUNT = 0;

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
        else if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
            if (strcmp(line + 21, "MESSAGE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            } else if (strcmp(line + 21, "LOCK") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
            } else if (strcmp(line + 21, "SHUTDOWN") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
            } else if (strcmp(line + 21, "RESTART") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
            } else if (strcmp(line + 21, "OPEN_FILE") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
            } else if (strcmp(line + 21, "SHOW_TIME") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
            } else if (strcmp(line + 21, "COUNT_UP") == 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
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
        else if (strncmp(line, "CLOCK_TIMEOUT_FILE_PATH=", 24) == 0) {
            strncpy(CLOCK_TIMEOUT_FILE_PATH, line + 24, MAX_PATH - 1);
            CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
            
            if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
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
        else if (strncmp(line, "WINDOW_TOPMOST=", 15) == 0) {
            CLOCK_WINDOW_TOPMOST = (strcmp(line + 15, "TRUE") == 0);
        }
        // 支持新的CLOCK_RECENT_FILE_N格式，同时保持与旧格式的兼容
        else if (strncmp(line, "CLOCK_RECENT_FILE_", 18) == 0) {
            char *path = strchr(line + 18, '=');
            if (path) {
                path++; // 跳过等号
                char *newline = strchr(path, '\n');
                if (newline) *newline = '\0';

                if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                    // 检查文件是否存在
                    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
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
        }
        // 保持对旧格式的兼容
        else if (strncmp(line, "CLOCK_RECENT_FILE=", 18) == 0) {
            char *path = line + 18;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';

            if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                // 检查文件是否存在
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
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
        else if (strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) == 0) {
            strncpy(CLOCK_TIMEOUT_FILE_PATH, line + 19, MAX_PATH - 1);
            CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
            
            // 移除尾部的换行符
            size_t len = strlen(CLOCK_TIMEOUT_FILE_PATH);
            if (len > 0 && CLOCK_TIMEOUT_FILE_PATH[len-1] == '\n') {
                CLOCK_TIMEOUT_FILE_PATH[len-1] = '\0';
            }
        }
    }

    fclose(file);
    last_config_time = time(NULL);

    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    // 读取番茄钟时间设置
    char work_time[32] = {0};
    char short_break[32] = {0};
    char long_break[32] = {0};
    char loop_count[32] = {0};
    
    if(GetPrivateProfileStringA("Settings", "POMODORO_WORK_TIME", "1500", work_time, sizeof(work_time), config_path)) {
        POMODORO_WORK_TIME = atoi(work_time);
    }
    
    if(GetPrivateProfileStringA("Settings", "POMODORO_SHORT_BREAK", "300", short_break, sizeof(short_break), config_path)) {
        POMODORO_SHORT_BREAK = atoi(short_break);
    }
    
    if(GetPrivateProfileStringA("Settings", "POMODORO_LONG_BREAK", "600", long_break, sizeof(long_break), config_path)) {
        POMODORO_LONG_BREAK = atoi(long_break);
    }
    
    if(GetPrivateProfileStringA("Settings", "POMODORO_LOOP_COUNT", "1", loop_count, sizeof(loop_count), config_path)) {
        POMODORO_LOOP_COUNT = atoi(loop_count);
        // 确保循环次数至少为1
        if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
    }
}

/**
 * @brief 写入超时动作配置
 * @param action 要写入的超时动作
 * 
 * 使用临时文件方式更新配置文件中的超时动作设置，
 * 处理OPEN_FILE动作时自动关联超时文件路径。
 */
void WriteConfigTimeoutAction(const char* action) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    FILE* temp = fopen(temp_path, "w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL found = FALSE;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action);
            found = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", action);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入编辑模式配置
 * @param mode 编辑模式状态("TRUE"/"FALSE")
 * 
 * 通过临时文件方式更新配置文件中的编辑模式设置，
 * 确保配置项存在时更新，不存在时追加到文件末尾。
 */
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

/**
 * @brief 写入时间选项配置
 * @param options 逗号分隔的时间选项字符串
 * 
 * 更新配置文件中的预设时间选项，支持动态调整
 * 倒计时时长选项列表，最大支持MAX_TIME_OPTIONS个选项。
 */
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

/**
 * @brief 加载最近使用文件记录
 * 
 * 从配置文件中解析CLOCK_RECENT_FILE条目，
 * 提取文件路径和文件名供快速访问使用。
 */
void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    FILE *file = fopen(config_path, "r");
    if (!file) return;

    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;

    while (fgets(line, sizeof(line), file)) {
        // 支持新的格式 CLOCK_RECENT_FILE_N=path
        if (strncmp(line, "CLOCK_RECENT_FILE_", 18) == 0) {
            char *path = strchr(line + 18, '=');
            if (path) {
                path++; // 跳过等号
                char *newline = strchr(path, '\n');
                if (newline) *newline = '\0';

                if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                    // 检查文件是否存在
                    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
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
        }
        // 保持对老格式的兼容
        else if (strncmp(line, "CLOCK_RECENT_FILE=", 18) == 0) {
            char *path = line + 18;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';

            if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                // 检查文件是否存在
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
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
    }

    fclose(file);
}

/**
 * @brief 保存最近使用文件记录
 * @param filePath 要保存的文件路径
 * 
 * 维护最近文件列表(最多MAX_RECENT_FILES个)，
 * 自动去重并更新配置文件，保持最新文件在列表首位。
 * 处理中文路径时进行UTF8到ANSI编码转换。
 */
void SaveRecentFile(const char* filePath) {
    // 检查文件是否存在
    if (GetFileAttributesA(filePath) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    
    // 检查文件是否已在列表中
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
            // 已经存在，将其移到列表前面
            RecentFile temp = CLOCK_RECENT_FILES[i];
            for (int j = i; j > 0; j--) {
                CLOCK_RECENT_FILES[j] = CLOCK_RECENT_FILES[j-1];
            }
            CLOCK_RECENT_FILES[0] = temp;
            
            // 更新配置文件
            WriteConfig(NULL);
            return;
        }
    }
    
    // 添加新文件到列表头部
    if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
        // 有空间添加新文件
        for (int i = CLOCK_RECENT_FILES_COUNT; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i-1];
        }
        CLOCK_RECENT_FILES_COUNT++;
    } else {
        // 列表已满，移除最后一个
        for (int i = MAX_RECENT_FILES - 1; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i-1];
        }
    }
    
    // 设置第一个文件
    strncpy(CLOCK_RECENT_FILES[0].path, filePath, MAX_PATH - 1);
    CLOCK_RECENT_FILES[0].path[MAX_PATH - 1] = '\0';
    
    // 提取文件名
    const char* fileName = strrchr(filePath, '\\');
    if (fileName) {
        fileName++; // 跳过反斜杠
        strncpy(CLOCK_RECENT_FILES[0].name, fileName, MAX_PATH - 1);
    } else {
        strncpy(CLOCK_RECENT_FILES[0].name, filePath, MAX_PATH - 1);
    }
    
    // 更新配置文件
    WriteConfig(NULL);
}

/**
 * @brief UTF8转ANSI编码
 * @param utf8Str 要转换的UTF8字符串
 * @return char* 转换后的ANSI字符串指针
 * 
 * 用于处理中文路径的编码转换，转换失败返回原字符串副本。
 */
/**
 * @brief 写入所有配置设置到文件
 * @param config_path 配置文件路径
 */
void WriteConfig(const char* config_path) {
    FILE* file = fopen(config_path, "w");
    if (!file) return;
    
    fprintf(file, "CLOCK_TEXT_COLOR=%s\n", CLOCK_TEXT_COLOR);
    fprintf(file, "CLOCK_BASE_FONT_SIZE=%d\n", CLOCK_BASE_FONT_SIZE);
    fprintf(file, "FONT_FILE_NAME=%s\n", FONT_FILE_NAME);
    fprintf(file, "CLOCK_DEFAULT_START_TIME=%d\n", CLOCK_DEFAULT_START_TIME);
    fprintf(file, "CLOCK_WINDOW_POS_X=%d\n", CLOCK_WINDOW_POS_X);
    fprintf(file, "CLOCK_WINDOW_POS_Y=%d\n", CLOCK_WINDOW_POS_Y);
    fprintf(file, "CLOCK_EDIT_MODE=%s\n", CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
    fprintf(file, "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
    fprintf(file, "CLOCK_USE_24HOUR=%s\n", CLOCK_USE_24HOUR ? "TRUE" : "FALSE");
    fprintf(file, "CLOCK_SHOW_SECONDS=%s\n", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE");
    fprintf(file, "WINDOW_TOPMOST=%s\n", CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE");
    
    fprintf(file, "CLOCK_TIME_OPTIONS=");
    for (int i = 0; i < time_options_count; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%d", time_options[i]);
    }
    fprintf(file, "\n");
    
    fprintf(file, "CLOCK_TIMEOUT_TEXT=%s\n", CLOCK_TIMEOUT_TEXT);
    
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        fprintf(file, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        fprintf(file, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH);
    } else {
        switch (CLOCK_TIMEOUT_ACTION) {
            case TIMEOUT_ACTION_MESSAGE:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
                break;
            case TIMEOUT_ACTION_LOCK:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");
                break;
            case TIMEOUT_ACTION_SHUTDOWN:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=SHUTDOWN\n");
                break;
            case TIMEOUT_ACTION_RESTART:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=RESTART\n");
                break;
        }
    }
    
    // 使用CLOCK_RECENT_FILE_1格式保存最近文件
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        fprintf(file, "CLOCK_RECENT_FILE_%d=%s\n", i+1, CLOCK_RECENT_FILES[i].path);
    }
    
    fprintf(file, "COLOR_OPTIONS=");
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%s", COLOR_OPTIONS[i].hexColor);
    }
    fprintf(file, "\n");
    
    fclose(file);
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

// 添加写入番茄钟配置的函数
void WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found_work = 0, found_short = 0, found_long = 0;
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_WORK_TIME=", 19) == 0) {
            fprintf(temp_file, "POMODORO_WORK_TIME=%d\n", work);
            found_work = 1;
        } else if (strncmp(line, "POMODORO_SHORT_BREAK=", 21) == 0) {
            fprintf(temp_file, "POMODORO_SHORT_BREAK=%d\n", short_break);
            found_short = 1;
        } else if (strncmp(line, "POMODORO_LONG_BREAK=", 20) == 0) {
            fprintf(temp_file, "POMODORO_LONG_BREAK=%d\n", long_break);
            found_long = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    // 如果配置文件中没有找到对应的键，则添加
    if (!found_work) {
        fprintf(temp_file, "POMODORO_WORK_TIME=%d\n", work);
    }
    if (!found_short) {
        fprintf(temp_file, "POMODORO_SHORT_BREAK=%d\n", short_break);
    }
    if (!found_long) {
        fprintf(temp_file, "POMODORO_LONG_BREAK=%d\n", long_break);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    POMODORO_WORK_TIME = work;
    POMODORO_SHORT_BREAK = short_break;
    POMODORO_LONG_BREAK = long_break;
}

// 添加写入番茄钟循环次数配置的函数
void WriteConfigPomodoroLoopCount(int loop_count) {
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
        if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
            fprintf(temp_file, "POMODORO_LOOP_COUNT=%d\n", loop_count);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    // 如果配置文件中没有找到对应的键，则添加
    if (!found) {
        fprintf(temp_file, "POMODORO_LOOP_COUNT=%d\n", loop_count);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    POMODORO_LOOP_COUNT = loop_count;
}

// 添加写入置顶状态函数
void WriteConfigTopmost(const char* topmost) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    FILE* temp = fopen(temp_path, "w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL found = FALSE;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "WINDOW_TOPMOST=", 15) == 0) {
            fprintf(temp, "WINDOW_TOPMOST=%s\n", topmost);
            found = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "WINDOW_TOPMOST=%s\n", topmost);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入超时打开文件路径
 * @param filePath 文件路径
 * 
 * 更新配置文件中的超时打开文件路径，同时设置超时动作为打开文件
 */
void WriteConfigTimeoutFile(const char* filePath) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    FILE* temp = fopen(temp_path, "w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL foundAction = FALSE;
    BOOL foundFile = FALSE;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
            foundAction = TRUE;
        } else if (strncmp(line, "CLOCK_TIMEOUT_FILE=", 19) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_FILE=%s\n", filePath);
            foundFile = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!foundAction) {
        fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
    }
    
    if (!foundFile) {
        fprintf(temp, "CLOCK_TIMEOUT_FILE=%s\n", filePath);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
}
