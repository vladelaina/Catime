/**
 * @file config.c
 * @brief 配置文件管理模块实现
 * 
 * 本模块负责配置文件的路径获取、创建、读写等核心管理功能，
 * 包含默认配置生成、配置持久化存储、最近文件记录维护等功能。
 * 支持UTF-8编码与中文路径处理。
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

// 定义番茄钟时间数组的最大容量
#define MAX_POMODORO_TIMES 10

/**
 * 全局变量声明区域
 * 以下变量定义了应用的默认配置值，可被配置文件覆盖
 */
// 修改全局变量的默认值(单位:秒)
extern int POMODORO_WORK_TIME;       // 默认工作时间25分钟(1500秒)
extern int POMODORO_SHORT_BREAK;     // 默认短休息5分钟(300秒)
extern int POMODORO_LONG_BREAK;      // 默认长休息10分钟(600秒)
extern int POMODORO_LOOP_COUNT;      // 默认循环次数1次

// 番茄钟时间序列，格式为：[工作时间, 短休息, 工作时间, 长休息]
int POMODORO_TIMES[MAX_POMODORO_TIMES] = {1500, 300, 1500, 600}; // 默认时间
int POMODORO_TIMES_COUNT = 4;                             // 默认有4个时间段

// 自定义提示信息文本 (使用 UTF-8 编码)
char CLOCK_TIMEOUT_MESSAGE_TEXT[100] = "时间到啦！";
char POMODORO_TIMEOUT_MESSAGE_TEXT[100] = "番茄钟时间到！"; // 新增番茄钟专用提示信息
char POMODORO_CYCLE_COMPLETE_TEXT[100] = "所有番茄钟循环完成！";

// 新增配置变量：通知显示持续时间(毫秒)
int NOTIFICATION_TIMEOUT_MS = 3000;  // 默认3秒
// 新增配置变量：通知窗口最大透明度(百分比)
int NOTIFICATION_MAX_OPACITY = 95;   // 默认95%透明度

/**
 * @brief 获取配置文件路径
 * @param path 存储路径的缓冲区
 * @param size 缓冲区大小
 * 
 * 优先获取LOCALAPPDATA环境变量路径，若不存在则回退至程序目录。
 * 自动创建配置目录结构，若创建失败则使用本地备用路径。
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
 * @param config_path 配置文件完整路径
 * 
 * 生成包含所有必要参数的默认配置文件，配置项按以下顺序组织：
 * 1. 基本设置（颜色、字体、窗口位置等UI设置）
 * 2. 颜色选项列表（用户可选的颜色方案）
 * 3. 超时提示文本与自定义通知消息
 * 4. 番茄钟相关时间设置与循环参数
 * 5. 超时后的动作设置与相关资源路径
 * 6. 最近使用文件列表
 * 7. 预设时间选项
 * 
 * 该顺序与WriteConfig函数保持一致，确保配置文件结构统一。
 */
void CreateDefaultConfig(const char* config_path) {
    FILE *file = fopen(config_path, "w");
    if (file) {
        // 基本设置区块 - 与WriteConfig函数保持相同顺序
        fprintf(file, "CLOCK_TEXT_COLOR=#FFB6C1\n");
        fprintf(file, "CLOCK_BASE_FONT_SIZE=20\n");
        fprintf(file, "FONT_FILE_NAME=Wallpoet Essence.ttf\n");
        fprintf(file, "CLOCK_DEFAULT_START_TIME=1500\n");
        fprintf(file, "CLOCK_WINDOW_POS_X=960\n");
        fprintf(file, "CLOCK_WINDOW_POS_Y=-1\n");
        fprintf(file, "CLOCK_EDIT_MODE=FALSE\n");
        fprintf(file, "WINDOW_SCALE=1.62\n");
        fprintf(file, "CLOCK_USE_24HOUR=FALSE\n");
        fprintf(file, "CLOCK_SHOW_SECONDS=FALSE\n");
        fprintf(file, "WINDOW_TOPMOST=TRUE\n");
        
        // 颜色选项区块
        fprintf(file, "COLOR_OPTIONS=#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174\n");
        
        // 超时文本区块
        fprintf(file, "CLOCK_TIMEOUT_TEXT=0\n");
        
        // 新增：自定义通知消息
        fprintf(file, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\n", CLOCK_TIMEOUT_MESSAGE_TEXT);
        fprintf(file, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\n", POMODORO_TIMEOUT_MESSAGE_TEXT); // 添加番茄钟专用提示
        fprintf(file, "POMODORO_CYCLE_COMPLETE_TEXT=%s\n", POMODORO_CYCLE_COMPLETE_TEXT);
        
        // 新增：通知显示时间
        fprintf(file, "NOTIFICATION_TIMEOUT_MS=3000\n");  // 默认3秒
        
        // 新增：通知窗口最大透明度
        fprintf(file, "NOTIFICATION_MAX_OPACITY=95\n");   // 默认95%
        
        // 番茄钟设置区块
        fprintf(file, "POMODORO_TIME_OPTIONS=1500,300,1500,600\n"); // 时间1,时间2,时间3,时间4...
        fprintf(file, "POMODORO_LOOP_COUNT=1\n");       // 循环次数
        
        // 超时动作设置区块
        fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
        fprintf(file, "CLOCK_TIMEOUT_FILE=\n");         // 空文件路径占位
        fprintf(file, "CLOCK_TIMEOUT_WEBSITE=\n");      // 空网站URL占位
        
        // 最近文件列表区块
        for (int i = 1; i <= 5; i++) {
            fprintf(file, "CLOCK_RECENT_FILE_%d=\n", i); // 空文件记录占位
        }
        
        // 时间选项区块
        fprintf(file, "CLOCK_TIME_OPTIONS=25,10,5\n");
        
        fclose(file);
    }
}

/**
 * @brief 从文件路径中提取文件名
 * @param path 完整文件路径
 * @param name 输出文件名缓冲区
 * @param nameSize 缓冲区大小
 * 
 * 从完整文件路径中提取文件名部分，支持UTF-8编码的中文路径。
 * 使用Windows API转换编码以确保正确处理Unicode字符。
 */
void ExtractFileName(const char* path, char* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return;
    
    // 首先转换为宽字符以正确处理Unicode路径
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    
    // 查找最后一个反斜杠或正斜杠
    wchar_t* lastSlash = wcsrchr(wPath, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(wPath, L'/');
    
    wchar_t wName[MAX_PATH] = {0};
    if (lastSlash) {
        wcscpy(wName, lastSlash + 1);
    } else {
        wcscpy(wName, wPath);
    }
    
    // 转换回UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, nameSize, NULL, NULL);
}

/**
 * @brief 读取并解析配置文件
 * 
 * 从配置路径读取配置项，若文件不存在则自动创建默认配置。
 * 解析各配置项并更新程序全局状态变量，最后刷新窗口位置。
 * 支持兼容性处理，确保新旧版本配置文件均可正确读取。
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
            
            // 检查并替换纯黑色
            if (strcasecmp(CLOCK_TEXT_COLOR, "#000000") == 0) {
                strncpy(CLOCK_TEXT_COLOR, "#000001", sizeof(CLOCK_TEXT_COLOR) - 1);
            }
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
                // 即使配置文件中有SHUTDOWN，也将其视为一次性操作，默认为MESSAGE
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            } else if (strcmp(line + 21, "RESTART") == 0) {
                // 即使配置文件中有RESTART，也将其视为一次性操作，默认为MESSAGE
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
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
                    // Convert to wide characters for proper file existence check
                    wchar_t widePath[MAX_PATH] = {0};
                    MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                    
                    // Check if file exists using wide character function
                    if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
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
                // Convert to wide characters for proper file existence check
                wchar_t widePath[MAX_PATH] = {0};
                MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                
                // Check if file exists using wide character function
                if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
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
            
            // 如果文件路径有效，确保设置超时动作为打开文件
            if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
                GetFileAttributesA(CLOCK_TIMEOUT_FILE_PATH) != INVALID_FILE_ATTRIBUTES) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
            }
        }
        else if (strncmp(line, "CLOCK_TIMEOUT_WEBSITE=", 22) == 0) {
            strncpy(CLOCK_TIMEOUT_WEBSITE_URL, line + 22, MAX_PATH - 1);
            CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = '\0';
            
            // 移除尾部的换行符
            size_t len = strlen(CLOCK_TIMEOUT_WEBSITE_URL);
            if (len > 0 && CLOCK_TIMEOUT_WEBSITE_URL[len-1] == '\n') {
                CLOCK_TIMEOUT_WEBSITE_URL[len-1] = '\0';
            }
            
            // 如果URL有效，确保设置超时动作为打开网站
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
            }
        }
        else if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            char* options = line + 22;
            char* token;
            
            // 重置番茄钟时间计数
            POMODORO_TIMES_COUNT = 0;
            
            // 解析所有时间值
            token = strtok(options, ",");
            while (token && POMODORO_TIMES_COUNT < MAX_POMODORO_TIMES) {
                POMODORO_TIMES[POMODORO_TIMES_COUNT++] = atoi(token);
                token = strtok(NULL, ",");
            }
            
            // 即使我们现在使用新的数组存储所有时间，
            // 为了向后兼容，依然保留这三个变量的设置
            if (POMODORO_TIMES_COUNT > 0) {
                POMODORO_WORK_TIME = POMODORO_TIMES[0];
                if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
                if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
            }
        }
        // 新增：读取自定义通知消息
        else if (strncmp(line, "CLOCK_TIMEOUT_MESSAGE_TEXT=", 27) == 0) {
            strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, line + 27, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
            CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
        }
        else if (strncmp(line, "POMODORO_CYCLE_COMPLETE_TEXT=", 29) == 0) {
            strncpy(POMODORO_CYCLE_COMPLETE_TEXT, line + 29, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
            POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
        }
        // 添加读取通知显示时间的代码
        else if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            int timeout = atoi(line + 24);
            if (timeout > 0) {
                NOTIFICATION_TIMEOUT_MS = timeout;
            }
        }
        // 添加读取通知最大透明度的代码
        else if (strncmp(line, "NOTIFICATION_MAX_OPACITY=", 25) == 0) {
            int opacity = atoi(line + 25);
            // 确保透明度在有效范围内(1-100)
            if (opacity >= 1 && opacity <= 100) {
                NOTIFICATION_MAX_OPACITY = opacity;
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
 * @param action 要写入的超时动作类型
 * 
 * 使用临时文件方式安全更新配置文件中的超时动作设置，
 * 处理OPEN_FILE动作时自动关联超时文件路径。
 * 注意："RESTART"和"SHUTDOWN"选项为一次性操作，不会持久化保存。
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
    
    // 如果是关机或重启，不写入配置文件，而是写入"MESSAGE"
    const char* actual_action = action;
    if (strcmp(action, "RESTART") == 0 || strcmp(action, "SHUTDOWN") == 0) {
        actual_action = "MESSAGE";
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", actual_action);
            found = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "CLOCK_TIMEOUT_ACTION=%s\n", actual_action);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入编辑模式配置
 * @param mode 编辑模式状态值("TRUE"/"FALSE")
 * 
 * 通过临时文件方式安全更新配置文件中的编辑模式设置，
 * 确保配置项存在时更新，不存在时自动追加到文件末尾。
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
 * 采用临时文件方式确保写入过程的原子性和安全性。
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
 * 支持新旧两种格式的最近文件记录，自动过滤不存在的文件。
 */
void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    FILE *file = fopen(config_path, "r");
    if (!file) return;

    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;

    while (fgets(line, sizeof(line), file)) {
        // Support for the CLOCK_RECENT_FILE_N=path format
        if (strncmp(line, "CLOCK_RECENT_FILE_", 18) == 0) {
            char *path = strchr(line + 18, '=');
            if (path) {
                path++; // Skip the equals sign
                char *newline = strchr(path, '\n');
                if (newline) *newline = '\0';

                if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                    // Convert to wide characters for proper file existence check
                    wchar_t widePath[MAX_PATH] = {0};
                    MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                    
                    // Check if file exists using wide character function
                    if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
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
        // Also update the old format for compatibility
        else if (strncmp(line, "CLOCK_RECENT_FILE=", 18) == 0) {
            char *path = line + 18;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';

            if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
                // Convert to wide characters for proper file existence check
                wchar_t widePath[MAX_PATH] = {0};
                MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                
                // Check if file exists using wide character function
                if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
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
 * 自动去重并更新配置文件，保持最新使用的文件在列表首位。
 * 自动处理中文路径，支持UTF8编码，确保文件存在后再添加。
 */
void SaveRecentFile(const char* filePath) {
    // 检查文件路径是否有效
    if (!filePath || strlen(filePath) == 0) return;
    
    // 转换为宽字符以检查文件是否存在
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wPath, MAX_PATH);
    
    if (GetFileAttributesW(wPath) == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，不添加
        return;
    }
    
    // 检查文件是否已在列表中
    int existingIndex = -1;
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex == 0) {
        // 文件已经在列表最前面，无需操作
        return;
    }
    
    if (existingIndex > 0) {
        // 文件已在列表中，但不在最前面，需要移动
        RecentFile temp = CLOCK_RECENT_FILES[existingIndex];
        
        // 向后移动元素
        for (int i = existingIndex; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        // 放到第一位
        CLOCK_RECENT_FILES[0] = temp;
    } else {
        // 文件不在列表中，需要添加
        // 首先确保列表不超过5个
        if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
            CLOCK_RECENT_FILES_COUNT++;
        }
        
        // 向后移动元素
        for (int i = CLOCK_RECENT_FILES_COUNT - 1; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        // 添加新文件到第一位
        strncpy(CLOCK_RECENT_FILES[0].path, filePath, MAX_PATH - 1);
        CLOCK_RECENT_FILES[0].path[MAX_PATH - 1] = '\0';
        
        // 提取文件名
        ExtractFileName(filePath, CLOCK_RECENT_FILES[0].name, MAX_PATH);
    }
    
    // 更新配置文件
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    WriteConfig(configPath);
}

/**
 * @brief UTF8转ANSI编码
 * @param utf8Str 要转换的UTF8字符串
 * @return char* 转换后的ANSI字符串指针（需手动释放）
 * 
 * 用于处理中文路径的编码转换，确保Windows API能正确处理路径。
 * 转换失败时会返回原字符串的副本，需手动释放返回的内存。
 */
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

/**
 * @brief 写入番茄钟时间设置
 * @param work 工作时间(秒)
 * @param short_break 短休息时间(秒)
 * @param long_break 长休息时间(秒)
 * 
 * 更新番茄钟相关时间设置并保存到配置文件，
 * 同时更新全局变量与POMODORO_TIMES数组，保持一致性。
 * 采用临时文件方式确保写入过程安全可靠。
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    // 更新全局变量
    // 保持向后兼容，同时更新POMODORO_TIMES数组
    POMODORO_WORK_TIME = work;
    POMODORO_SHORT_BREAK = short_break;
    POMODORO_LONG_BREAK = long_break;
    
    // 确保至少有这三个时间值
    POMODORO_TIMES[0] = work;
    if (POMODORO_TIMES_COUNT < 1) POMODORO_TIMES_COUNT = 1;
    
    if (POMODORO_TIMES_COUNT > 1) {
        POMODORO_TIMES[1] = short_break;
    } else if (short_break > 0) {
        POMODORO_TIMES[1] = short_break;
        POMODORO_TIMES_COUNT = 2;
    }
    
    if (POMODORO_TIMES_COUNT > 2) {
        POMODORO_TIMES[2] = long_break;
    } else if (long_break > 0) {
        POMODORO_TIMES[2] = long_break;
        POMODORO_TIMES_COUNT = 3;
    }
    
    file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    while (fgets(line, sizeof(line), file)) {
        // 查找POMODORO_TIME_OPTIONS行
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            // 写入所有番茄钟时间
            fprintf(temp_file, "POMODORO_TIME_OPTIONS=");
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                if (i > 0) fprintf(temp_file, ",");
                fprintf(temp_file, "%d", POMODORO_TIMES[i]);
            }
            fprintf(temp_file, "\n");
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    // 如果没有找到POMODORO_TIME_OPTIONS，则添加它
    if (!found) {
        fprintf(temp_file, "POMODORO_TIME_OPTIONS=");
        for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
            if (i > 0) fprintf(temp_file, ",");
            fprintf(temp_file, "%d", POMODORO_TIMES[i]);
        }
        fprintf(temp_file, "\n");
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入番茄钟循环次数配置
 * @param loop_count 循环次数
 * 
 * 更新番茄钟循环次数并保存到配置文件，
 * 采用临时文件方式确保配置更新过程不会损坏原文件。
 * 若配置项不存在则会自动添加到文件中。
 */
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

/**
 * @brief 写入窗口置顶状态配置
 * @param topmost 置顶状态("TRUE"/"FALSE")
 * 
 * 更新窗口是否置顶的配置并保存到文件，
 * 使用临时文件方式确保写入过程安全完整。
 */
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
 * @param filePath 目标文件路径
 * 
 * 更新配置文件中的超时打开文件路径，同时设置超时动作为打开文件。
 * 使用WriteConfig函数完全重写配置文件，确保：
 * 1. 保留所有现有设置
 * 2. 维持配置文件结构一致性
 * 3. 不会丢失其他已配置设置
 */
void WriteConfigTimeoutFile(const char* filePath) {
    // 首先更新全局变量
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    
    // 使用WriteConfig完全重写配置文件，保持结构一致
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteConfig(config_path);
}

/**
 * @brief 写入所有配置设置到文件
 * @param config_path 配置文件路径
 * 
 * 按照统一的顺序写入所有配置项，确保配置文件结构一致：
 * 1. 基本设置（颜色、字体、窗口位置等）
 * 2. 颜色选项列表
 * 3. 超时文本与通知消息
 * 4. 番茄钟设置
 * 5. 超时动作及相关资源路径
 * 6. 最近文件列表
 * 7. 时间选项
 * 
 * 该顺序与CreateDefaultConfig函数保持一致，确保配置文件结构统一。
 */
void WriteConfig(const char* config_path) {
    FILE* file = fopen(config_path, "w");
    if (!file) return;
    
    // 基本设置区块
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
    
    // 颜色选项区块
    fprintf(file, "COLOR_OPTIONS=");
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%s", COLOR_OPTIONS[i].hexColor);
    }
    fprintf(file, "\n");
    
    // 超时文本区块
    fprintf(file, "CLOCK_TIMEOUT_TEXT=%s\n", CLOCK_TIMEOUT_TEXT);
    
    // 新增：自定义通知消息
    fprintf(file, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\n", CLOCK_TIMEOUT_MESSAGE_TEXT);
    fprintf(file, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\n", POMODORO_TIMEOUT_MESSAGE_TEXT); // 添加番茄钟专用提示
    fprintf(file, "POMODORO_CYCLE_COMPLETE_TEXT=%s\n", POMODORO_CYCLE_COMPLETE_TEXT);
    
    // 新增：通知显示时间
    fprintf(file, "NOTIFICATION_TIMEOUT_MS=%d\n", NOTIFICATION_TIMEOUT_MS);
    
    // 新增：通知最大透明度
    fprintf(file, "NOTIFICATION_MAX_OPACITY=%d\n", NOTIFICATION_MAX_OPACITY);
    
    // 番茄钟设置区块
    fprintf(file, "POMODORO_TIME_OPTIONS=");
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%d", POMODORO_TIMES[i]);
    }
    fprintf(file, "\n");
    fprintf(file, "POMODORO_LOOP_COUNT=%d\n", POMODORO_LOOP_COUNT);
    
    // 超时动作设置区块
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        fprintf(file, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        fprintf(file, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH);
    } else if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE && strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
        fprintf(file, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
        fprintf(file, "CLOCK_TIMEOUT_WEBSITE=%s\n", CLOCK_TIMEOUT_WEBSITE_URL);
    } else {
        // 确保关机和重启选项不会被永久保存到配置文件中
        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN || 
            CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART) {
            fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
        } else {
            switch (CLOCK_TIMEOUT_ACTION) {
                case TIMEOUT_ACTION_MESSAGE:
                    fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
                    break;
                case TIMEOUT_ACTION_LOCK:
                    fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");
                    break;
                case TIMEOUT_ACTION_SHOW_TIME:
                    fprintf(file, "CLOCK_TIMEOUT_ACTION=SHOW_TIME\n");
                    break;
                case TIMEOUT_ACTION_COUNT_UP:
                    fprintf(file, "CLOCK_TIMEOUT_ACTION=COUNT_UP\n");
                    break;
            }
        }
    }
    
    // 最近文件列表区块
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        fprintf(file, "CLOCK_RECENT_FILE_%d=%s\n", i+1, CLOCK_RECENT_FILES[i].path);
    }
    
    // 时间选项区块
    fprintf(file, "CLOCK_TIME_OPTIONS=");
    for (int i = 0; i < time_options_count; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%d", time_options[i]);
    }
    fprintf(file, "\n");
    
    fclose(file);
}

/**
 * @brief 写入超时打开网站的URL
 * @param url 目标网站URL
 * 
 * 更新配置文件中的超时打开网站URL，同时设置超时动作为打开网站。
 * 使用临时文件方式确保配置更新过程安全可靠。
 */
void WriteConfigTimeoutWebsite(const char* url) {
    // 首先更新全局变量
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    strncpy(CLOCK_TIMEOUT_WEBSITE_URL, url, MAX_PATH - 1);
    CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = '\0';
    
    // 然后更新配置文件
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
    
    char line[MAX_PATH];
    BOOL actionFound = FALSE;
    BOOL urlFound = FALSE;
    
    // 读取原配置文件，更新超时动作和URL
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
            actionFound = TRUE;
        } else if (strncmp(line, "CLOCK_TIMEOUT_WEBSITE=", 22) == 0) {
            fprintf(temp, "CLOCK_TIMEOUT_WEBSITE=%s\n", url);
            urlFound = TRUE;
        } else {
            // 保留其他所有配置
            fputs(line, temp);
        }
    }
    
    // 如果配置中没有这些项，添加它们
    if (!actionFound) {
        fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
    }
    if (!urlFound) {
        fprintf(temp, "CLOCK_TIMEOUT_WEBSITE=%s\n", url);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入启动模式配置
 * @param mode 启动模式字符串("COUNTDOWN"/"COUNT_UP"/"SHOW_TIME"/"NO_DISPLAY")
 * 
 * 修改配置文件中的STARTUP_MODE项，控制程序启动时的默认计时模式。
 * 同时更新全局变量，确保设置立即生效。
 */
void WriteConfigStartupMode(const char* mode) {
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
    
    // 更新全局变量
    strncpy(CLOCK_STARTUP_MODE, mode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
            fprintf(temp_file, "STARTUP_MODE=%s\n", mode);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    if (!found) {
        fprintf(temp_file, "STARTUP_MODE=%s\n", mode);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入番茄钟时间选项
 * @param times 时间数组（秒）
 * @param count 时间数组长度
 * 
 * 将番茄钟自定义时间序列写入配置文件，
 * 格式为逗号分隔的时间值列表。
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigPomodoroTimeOptions(int* times, int count) {
    if (!times || count <= 0) return;
    
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
    
    char line[MAX_PATH];
    BOOL optionsFound = FALSE;
    
    // 读取原配置文件，更新番茄钟时间选项
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            // 写入新的时间选项
            fprintf(temp, "POMODORO_TIME_OPTIONS=");
            for (int i = 0; i < count; i++) {
                fprintf(temp, "%d", times[i]);
                if (i < count - 1) fprintf(temp, ",");
            }
            fprintf(temp, "\n");
            optionsFound = TRUE;
        } else {
            // 保留其他所有配置
            fputs(line, temp);
        }
    }
    
    // 如果配置中没有这一项，添加它
    if (!optionsFound) {
        fprintf(temp, "POMODORO_TIME_OPTIONS=");
        for (int i = 0; i < count; i++) {
            fprintf(temp, "%d", times[i]);
            if (i < count - 1) fprintf(temp, ",");
        }
        fprintf(temp, "\n");
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 写入通知消息配置
 * @param timeout_msg 倒计时超时提示文本
 * @param pomodoro_msg 番茄钟超时提示文本
 * @param cycle_complete_msg 番茄钟循环完成提示文本
 * 
 * 更新配置文件中的通知消息设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    HANDLE hSourceFile = CreateFileA(
        config_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    HANDLE hTempFile = CreateFileA(
        temp_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hSourceFile == INVALID_HANDLE_VALUE) {
        // 源文件不存在，创建新文件并写入默认内容
        if (hTempFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hTempFile);
        }
        
        HANDLE hNewFile = CreateFileA(
            config_path,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hNewFile == INVALID_HANDLE_VALUE) {
            return; // 无法创建文件
        }
        
        // 写入UTF-8 BOM标记
        unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
        DWORD bytesWritten;
        WriteFile(hNewFile, bom, 3, &bytesWritten, NULL);
        
        // 写入配置项
        char buffer[512];
        
        sprintf(buffer, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\r\n", timeout_msg);
        WriteFile(hNewFile, buffer, strlen(buffer), &bytesWritten, NULL);
        
        sprintf(buffer, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\r\n", pomodoro_msg);
        WriteFile(hNewFile, buffer, strlen(buffer), &bytesWritten, NULL);
        
        sprintf(buffer, "POMODORO_CYCLE_COMPLETE_TEXT=%s\r\n", cycle_complete_msg);
        WriteFile(hNewFile, buffer, strlen(buffer), &bytesWritten, NULL);
        
        CloseHandle(hNewFile);
        
        // 更新全局变量
        strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, timeout_msg, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
        CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
        
        strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, pomodoro_msg, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
        POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
        
        strncpy(POMODORO_CYCLE_COMPLETE_TEXT, cycle_complete_msg, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
        POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
        
        return;
    }
    
    if (hTempFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hSourceFile);
        return;
    }
    
    // 写入UTF-8 BOM标记到临时文件
    unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    DWORD bytesWritten;
    WriteFile(hTempFile, bom, 3, &bytesWritten, NULL);
    
    // 跳过源文件的UTF-8 BOM标记（如果有）
    char bomCheck[3];
    DWORD bytesRead;
    ReadFile(hSourceFile, bomCheck, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bomCheck[0] != 0xEF || bomCheck[1] != 0xBB || bomCheck[2] != 0xBF) {
        // 不是BOM，回退文件指针
        SetFilePointer(hSourceFile, 0, NULL, FILE_BEGIN);
    }
    
    // 三个标志位，用于标记是否已经找到并更新了对应的配置项
    BOOL foundTimeout = FALSE;
    BOOL foundPomodoro = FALSE;
    BOOL foundCycle = FALSE;
    
    // 逐行复制文件内容
    char line[1024];
    BOOL readingLine = TRUE;
    int pos = 0;
    char buffer[1024];
    
    while (readingLine) {
        // 逐字节读取，构建行
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hSourceFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // 文件结束
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // 行结束
                break;
            }
            
            if (ch != '\r') { // 忽略回车符
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // 防止缓冲区溢出
            }
        }
        
        line[pos] = '\0'; // 确保字符串结束
        
        // 如果没有内容且文件已结束，退出循环
        if (pos == 0 && !readingLine) {
            break;
        }
        
        // 处理这一行
        if (strncmp(line, "CLOCK_TIMEOUT_MESSAGE_TEXT=", 27) == 0) {
            sprintf(buffer, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\r\n", timeout_msg);
            WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
            foundTimeout = TRUE;
        } else if (strncmp(line, "POMODORO_TIMEOUT_MESSAGE_TEXT=", 30) == 0) {
            sprintf(buffer, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\r\n", pomodoro_msg);
            WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
            foundPomodoro = TRUE;
        } else if (strncmp(line, "POMODORO_CYCLE_COMPLETE_TEXT=", 29) == 0) {
            sprintf(buffer, "POMODORO_CYCLE_COMPLETE_TEXT=%s\r\n", cycle_complete_msg);
            WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
            foundCycle = TRUE;
        } else {
            // 写回原始行，加上换行符
            strcat(line, "\r\n");
            WriteFile(hTempFile, line, strlen(line), &bytesWritten, NULL);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!foundTimeout) {
        sprintf(buffer, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\r\n", timeout_msg);
        WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
    }
    
    if (!foundPomodoro) {
        sprintf(buffer, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\r\n", pomodoro_msg);
        WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
    }
    
    if (!foundCycle) {
        sprintf(buffer, "POMODORO_CYCLE_COMPLETE_TEXT=%s\r\n", cycle_complete_msg);
        WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
    }
    
    CloseHandle(hSourceFile);
    CloseHandle(hTempFile);
    
    // 替换原文件
    DeleteFileA(config_path);
    MoveFileA(temp_path, config_path);
    
    // 更新全局变量
    strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, timeout_msg, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
    CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, pomodoro_msg, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
    POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_CYCLE_COMPLETE_TEXT, cycle_complete_msg, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
    POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
}

/**
 * @brief 从配置文件中读取通知消息文本
 * 
 * 专门读取 CLOCK_TIMEOUT_MESSAGE_TEXT、POMODORO_TIMEOUT_MESSAGE_TEXT 和 POMODORO_CYCLE_COMPLETE_TEXT
 * 并更新相应的全局变量。若配置不存在则保持默认值不变。
 * 支持UTF-8编码的中文消息文本。
 */
void ReadNotificationMessagesConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    HANDLE hFile = CreateFileA(
        config_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // 文件无法打开，保留内存中的当前值或默认值
        return;
    }

    // 跳过UTF-8 BOM标记（如果有）
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // 不是BOM，需要回退文件指针
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[1024];
    BOOL timeoutMsgFound = FALSE;
    BOOL pomodoroTimeoutMsgFound = FALSE;
    BOOL cycleCompleteMsgFound = FALSE;
    
    // 逐行读取文件内容
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // 逐字节读取，构建行
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // 文件结束
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // 行结束
                break;
            }
            
            if (ch != '\r') { // 忽略回车符
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // 防止缓冲区溢出
            }
        }
        
        line[pos] = '\0'; // 确保字符串结束
        
        // 如果没有内容且文件已结束，退出循环
        if (pos == 0 && !readingLine) {
            break;
        }
        
        // 处理这一行
        if (strncmp(line, "CLOCK_TIMEOUT_MESSAGE_TEXT=", 27) == 0) {
            strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, line + 27, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
            CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
            timeoutMsgFound = TRUE;
        } 
        else if (strncmp(line, "POMODORO_TIMEOUT_MESSAGE_TEXT=", 30) == 0) {
            strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, line + 30, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
            POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
            pomodoroTimeoutMsgFound = TRUE;
        }
        else if (strncmp(line, "POMODORO_CYCLE_COMPLETE_TEXT=", 29) == 0) {
            strncpy(POMODORO_CYCLE_COMPLETE_TEXT, line + 29, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
            POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
            cycleCompleteMsgFound = TRUE;
        }
        
        // 如果所有消息都找到了，可以提前退出循环
        if (timeoutMsgFound && pomodoroTimeoutMsgFound && cycleCompleteMsgFound) {
            break;
        }
    }
    
    CloseHandle(hFile);
    
    // 如果文件中没有找到对应的配置项，确保变量有默认值
    if (!timeoutMsgFound) {
        strcpy(CLOCK_TIMEOUT_MESSAGE_TEXT, "时间到啦！"); // 默认值
    }
    if (!pomodoroTimeoutMsgFound) {
        strcpy(POMODORO_TIMEOUT_MESSAGE_TEXT, "番茄钟时间到！"); // 默认值
    }
    if (!cycleCompleteMsgFound) {
        strcpy(POMODORO_CYCLE_COMPLETE_TEXT, "所有番茄钟循环完成！"); // 默认值
    }
}

/**
 * @brief 从配置文件中读取通知显示时间
 * 
 * 专门读取 NOTIFICATION_TIMEOUT_MS 配置项
 * 并更新相应的全局变量。若配置不存在则保持默认值不变。
 */
void ReadNotificationTimeoutConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    HANDLE hFile = CreateFileA(
        config_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // 文件无法打开，保留当前默认值
        return;
    }
    
    // 跳过UTF-8 BOM标记（如果有）
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // 不是BOM，需要回退文件指针
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL timeoutFound = FALSE;
    
    // 逐行读取文件内容
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // 逐字节读取，构建行
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // 文件结束
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // 行结束
                break;
            }
            
            if (ch != '\r') { // 忽略回车符
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // 防止缓冲区溢出
            }
        }
        
        line[pos] = '\0'; // 确保字符串结束
        
        // 如果没有内容且文件已结束，退出循环
        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            int timeout = atoi(line + 24);
            if (timeout > 0) {
                NOTIFICATION_TIMEOUT_MS = timeout;
            }
            timeoutFound = TRUE;
            break; // 找到后就可以退出循环了
        }
    }
    
    CloseHandle(hFile);
    
    // 如果配置中没找到，保留默认值
    if (!timeoutFound) {
        NOTIFICATION_TIMEOUT_MS = 3000; // 确保有默认值
    }
}

/**
 * @brief 写入通知显示时间配置
 * @param timeout_ms 通知显示时间(毫秒)
 * 
 * 更新配置文件中的通知显示时间设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationTimeout(int timeout_ms) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    HANDLE hSourceFile = CreateFileA(
        config_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    HANDLE hTempFile = CreateFileA(
        temp_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hSourceFile == INVALID_HANDLE_VALUE) {
        // 源文件不存在，创建新文件并写入默认内容
        if (hTempFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hTempFile);
        }
        
        HANDLE hNewFile = CreateFileA(
            config_path,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hNewFile == INVALID_HANDLE_VALUE) {
            return; // 无法创建文件
        }
        
        // 写入UTF-8 BOM标记
        unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
        DWORD bytesWritten;
        WriteFile(hNewFile, bom, 3, &bytesWritten, NULL);
        
        // 写入配置项
        char buffer[128];
        sprintf(buffer, "NOTIFICATION_TIMEOUT_MS=%d\r\n", timeout_ms);
        WriteFile(hNewFile, buffer, strlen(buffer), &bytesWritten, NULL);
        
        CloseHandle(hNewFile);
        
        // 更新全局变量
        NOTIFICATION_TIMEOUT_MS = timeout_ms;
        return;
    }
    
    if (hTempFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hSourceFile);
        return;
    }
    
    // 写入UTF-8 BOM标记到临时文件
    unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    DWORD bytesWritten;
    WriteFile(hTempFile, bom, 3, &bytesWritten, NULL);
    
    // 跳过源文件的UTF-8 BOM标记（如果有）
    char bomCheck[3];
    DWORD bytesRead;
    ReadFile(hSourceFile, bomCheck, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bomCheck[0] != 0xEF || bomCheck[1] != 0xBB || bomCheck[2] != 0xBF) {
        // 不是BOM，回退文件指针
        SetFilePointer(hSourceFile, 0, NULL, FILE_BEGIN);
    }
    
    // 逐行复制文件内容
    char line[1024];
    BOOL found = FALSE;
    BOOL readingLine = TRUE;
    int pos = 0;
    char buffer[1024];
    
    while (readingLine) {
        // 逐字节读取，构建行
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hSourceFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // 文件结束
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // 行结束
                break;
            }
            
            if (ch != '\r') { // 忽略回车符
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // 防止缓冲区溢出
            }
        }
        
        line[pos] = '\0'; // 确保字符串结束
        
        // 如果没有内容且文件已结束，退出循环
        if (pos == 0 && !readingLine) {
            break;
        }
        
        // 处理这一行
        if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            sprintf(buffer, "NOTIFICATION_TIMEOUT_MS=%d\r\n", timeout_ms);
            WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
            found = TRUE;
        } else {
            // 写回原始行，加上换行符
            strcat(line, "\r\n");
            WriteFile(hTempFile, line, strlen(line), &bytesWritten, NULL);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!found) {
        sprintf(buffer, "NOTIFICATION_TIMEOUT_MS=%d\r\n", timeout_ms);
        WriteFile(hTempFile, buffer, strlen(buffer), &bytesWritten, NULL);
    }
    
    CloseHandle(hSourceFile);
    CloseHandle(hTempFile);
    
    // 替换原文件
    DeleteFileA(config_path);
    MoveFileA(temp_path, config_path);
    
    // 更新全局变量
    NOTIFICATION_TIMEOUT_MS = timeout_ms;
}

/**
 * @brief 从配置文件中读取通知最大透明度
 * 
 * 专门读取 NOTIFICATION_MAX_OPACITY 配置项
 * 并更新相应的全局变量。若配置不存在则保持默认值不变。
 */
void ReadNotificationOpacityConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    HANDLE hFile = CreateFileA(
        config_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // 文件无法打开，保留当前默认值
        return;
    }
    
    // 跳过UTF-8 BOM标记（如果有）
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // 不是BOM，需要回退文件指针
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL opacityFound = FALSE;
    
    // 逐行读取文件内容
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // 逐字节读取，构建行
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // 文件结束
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // 行结束
                break;
            }
            
            if (ch != '\r') { // 忽略回车符
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // 防止缓冲区溢出
            }
        }
        
        line[pos] = '\0'; // 确保字符串结束
        
        // 如果没有内容且文件已结束，退出循环
        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_MAX_OPACITY=", 25) == 0) {
            int opacity = atoi(line + 25);
            // 确保透明度在有效范围内(1-100)
            if (opacity >= 1 && opacity <= 100) {
                NOTIFICATION_MAX_OPACITY = opacity;
            }
            opacityFound = TRUE;
            break; // 找到后就可以退出循环了
        }
    }
    
    CloseHandle(hFile);
    
    // 如果配置中没找到，保留默认值
    if (!opacityFound) {
        NOTIFICATION_MAX_OPACITY = 95; // 确保有默认值
    }
}
