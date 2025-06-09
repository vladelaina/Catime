/**
 * @file config.c
 * @brief 配置文件管理模块实现
 * 
 * 本模块负责配置文件的路径获取、创建、读写等核心管理功能，
 * 包含默认配置生成、配置持久化存储、最近文件记录维护等功能。
 * 支持UTF-8编码与中文路径处理。
 */

#include "../include/config.h"
#include "../include/language.h"
#include "../resource/resource.h"  // 添加这一行以访问CATIME_VERSION常量
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
// 新增配置变量：通知类型
NotificationType NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME; // 默认使用Catime通知窗口
// 新增配置变量：是否禁用通知窗口
BOOL NOTIFICATION_DISABLED = FALSE;  // 默认启用通知

// 新增：通知音频文件路径全局变量
char NOTIFICATION_SOUND_FILE[MAX_PATH] = "";  // 默认为空

// 新增：通知音频音量全局变量
int NOTIFICATION_SOUND_VOLUME = 100;  // 默认音量100%

/**
 * @brief 从INI文件读取字符串值
 * @param section 节名
 * @param key 键名
 * @param defaultValue 默认值
 * @param returnValue 返回值缓冲区
 * @param returnSize 缓冲区大小 
 * @param filePath 文件路径
 * @return 实际读取的字符数
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath) {
    return GetPrivateProfileStringA(section, key, defaultValue, returnValue, returnSize, filePath);
}

/**
 * @brief 写入字符串值到INI文件
 * @param section 节名
 * @param key 键名
 * @param value 值
 * @param filePath 文件路径
 * @return 是否成功
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath) {
    return WritePrivateProfileStringA(section, key, value, filePath);
}

/**
 * @brief 读取INI整数值
 * @param section 节名
 * @param key 键名
 * @param defaultValue 默认值
 * @param filePath 文件路径
 * @return 读取的整数值
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath) {
    return GetPrivateProfileIntA(section, key, defaultValue, filePath);
}

/**
 * @brief 写入整数值到INI文件
 * @param section 节名
 * @param key 键名
 * @param value 值
 * @param filePath 文件路径
 * @return 是否成功
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    return WritePrivateProfileStringA(section, key, valueStr, filePath);
}

/**
 * @brief 写入布尔值到INI文件
 * @param section 节名
 * @param key 键名 
 * @param value 布尔值
 * @param filePath 文件路径
 * @return 是否成功
 */
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
               const char* filePath) {
    return WritePrivateProfileStringA(section, key, value ? "TRUE" : "FALSE", filePath);
}

/**
 * @brief 读取INI布尔值
 * @param section 节名
 * @param key 键名
 * @param defaultValue 默认值
 * @param filePath 文件路径
 * @return 读取的布尔值
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath) {
    char value[8];
    GetPrivateProfileStringA(section, key, defaultValue ? "TRUE" : "FALSE", 
                          value, sizeof(value), filePath);
    return _stricmp(value, "TRUE") == 0;
}

/**
 * @brief 检查配置文件是否存在
 * @param filePath 文件路径
 * @return 文件是否存在
 */
BOOL FileExists(const char* filePath) {
    return GetFileAttributesA(filePath) != INVALID_FILE_ATTRIBUTES;
}

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
        if (snprintf(path, size, "%s\\Catime\\config.ini", appdata_path) >= size) {
            strncpy(path, ".\\asset\\config.ini", size - 1);
            path[size - 1] = '\0';
            return;
        }
        
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime", appdata_path) < sizeof(dir_path)) {
            if (!CreateDirectoryA(dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\asset\\config.ini", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\asset\\config.ini", size - 1);
        path[size - 1] = '\0';
    }
}

/**
 * @brief 创建默认配置文件
 * @param config_path 配置文件完整路径
 * 
 * 生成包含所有必要参数的默认配置文件，配置项按节进行组织：
 * 1. [General] - 基本设置（版本信息、语言设置）
 * 2. [Display] - 显示设置（颜色、字体、窗口位置等）
 * 3. [Timer] - 计时器相关设置（默认时间等）
 * 4. [Pomodoro] - 番茄钟相关设置
 * 5. [Notification] - 通知相关设置
 * 6. [Hotkeys] - 热键设置
 * 7. [RecentFiles] - 最近使用的文件
 * 8. [Colors] - 颜色选项
 * 9. [Options] - 其他选项
 */
void CreateDefaultConfig(const char* config_path) {
    // 获取系统默认语言ID
    LANGID systemLangID = GetUserDefaultUILanguage();
    int defaultLanguage = APP_LANG_ENGLISH; // 默认为英语
    const char* langName = "English"; // 默认语言名称
    
    // 根据系统语言ID设置默认语言
    switch (PRIMARYLANGID(systemLangID)) {
        case LANG_CHINESE:
            if (SUBLANGID(systemLangID) == SUBLANG_CHINESE_SIMPLIFIED) {
                defaultLanguage = APP_LANG_CHINESE_SIMP;
                langName = "Chinese_Simplified";
            } else {
                defaultLanguage = APP_LANG_CHINESE_TRAD;
                langName = "Chinese_Traditional";
            }
            break;
        case LANG_SPANISH:
            defaultLanguage = APP_LANG_SPANISH;
            langName = "Spanish";
            break;
        case LANG_FRENCH:
            defaultLanguage = APP_LANG_FRENCH;
            langName = "French";
            break;
        case LANG_GERMAN:
            defaultLanguage = APP_LANG_GERMAN;
            langName = "German";
            break;
        case LANG_RUSSIAN:
            defaultLanguage = APP_LANG_RUSSIAN;
            langName = "Russian";
            break;
        case LANG_PORTUGUESE:
            defaultLanguage = APP_LANG_PORTUGUESE;
            langName = "Portuguese";
            break;
        case LANG_JAPANESE:
            defaultLanguage = APP_LANG_JAPANESE;
            langName = "Japanese";
            break;
        case LANG_KOREAN:
            defaultLanguage = APP_LANG_KOREAN;
            langName = "Korean";
            break;
        case LANG_ENGLISH:
        default:
            defaultLanguage = APP_LANG_ENGLISH;
            langName = "English";
            break;
    }
    
    // 根据通知类型选择默认设置
    const char* typeStr;
    switch (NOTIFICATION_TYPE) {
        case NOTIFICATION_TYPE_CATIME:
            typeStr = "CATIME";
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            typeStr = "SYSTEM_MODAL";
            break;
        case NOTIFICATION_TYPE_OS:
            typeStr = "OS";
            break;
        default:
            typeStr = "CATIME"; // 默认值
            break;
    }
    
    // ======== [General] 节 ========
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", config_path);
    
    // ======== [Display] 节 ========
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "Wallpoet Essence.ttf", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 960, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", -1, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", config_path);
    
    // ======== [Timer] 节 ========
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "25,10,5", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", config_path);
    
    // ======== [Pomodoro] 节 ========
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    
    // ======== [Notification] 节 ========
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", config_path);
    
    // ======== [Hotkeys] 节 ========
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", config_path);
    
    // ======== [RecentFiles] 节 ========
    for (int i = 1; i <= 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    // ======== [Colors] 节 ========
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                 "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                 config_path);
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
 * @brief 检查并创建资源文件夹
 * 
 * 检查配置文件同目录下是否存在resources目录结构，如果不存在则创建
 * 创建的目录结构为：resources/audio, resources/images, resources/animations, resources/themes
 */
void CheckAndCreateResourceFolders() {
    char config_path[MAX_PATH];
    char base_path[MAX_PATH];
    char resource_path[MAX_PATH];
    char *last_slash;
    
    // 获取配置文件路径
    GetConfigPath(config_path, MAX_PATH);
    
    // 复制配置文件路径
    strncpy(base_path, config_path, MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';
    
    // 找到最后一个斜杠或反斜杠，即文件名部分的起始位置
    last_slash = strrchr(base_path, '\\');
    if (!last_slash) {
        last_slash = strrchr(base_path, '/');
    }
    
    if (last_slash) {
        // 截断路径到目录部分
        *(last_slash + 1) = '\0';
        
        // 创建resources主目录
        snprintf(resource_path, MAX_PATH, "%sresources", base_path);
        DWORD attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create resources folder: %s (Error: %lu)\n", resource_path, GetLastError());
                return;
            }
        }
        
        // 创建audio子目录
        snprintf(resource_path, MAX_PATH, "%sresources\\audio", base_path);
        attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create audio folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // 创建images子目录
        snprintf(resource_path, MAX_PATH, "%sresources\\images", base_path);
        attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create images folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // 创建animations子目录
        snprintf(resource_path, MAX_PATH, "%sresources\\animations", base_path);
        attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create animations folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // 创建themes子目录
        snprintf(resource_path, MAX_PATH, "%sresources\\themes", base_path);
        attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create themes folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // 创建plug-in子目录
        snprintf(resource_path, MAX_PATH, "%sresources\\plug-in", base_path);
        attrs = GetFileAttributesA(resource_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!CreateDirectoryA(resource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create plug-in folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
    }
}

/**
 * @brief 读取并解析配置文件
 * 
 * 从配置路径读取配置项，若文件不存在则自动创建默认配置。
 * 解析各配置项并更新程序全局状态变量，最后刷新窗口位置。
 */
void ReadConfig() {
    // 检查并创建资源文件夹
    CheckAndCreateResourceFolders();
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 检查配置文件是否存在，不存在则创建默认配置
    if (!FileExists(config_path)) {
        CreateDefaultConfig(config_path);
    }
    
    // 检查配置文件版本
    char version[32] = {0};
    BOOL versionMatched = FALSE;
    
    // 读取当前版本信息
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "", version, sizeof(version), config_path);
    
    // 比较版本是否匹配
    if (strcmp(version, CATIME_VERSION) == 0) {
        versionMatched = TRUE;
    }
    
    // 如果版本不匹配，重新创建配置文件
    if (!versionMatched) {
        CreateDefaultConfig(config_path);
    }

    // 重置时间选项
    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));
    
    // 重置最近文件计数
    CLOCK_RECENT_FILES_COUNT = 0;
    
    // 读取基本设置
    // ======== [General] 节 ========
    char language[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "LANGUAGE", "English", language, sizeof(language), config_path);
    
    // 将语言名称转换为枚举值
    int languageSetting = APP_LANG_ENGLISH; // 默认为英语
    
    if (strcmp(language, "Chinese_Simplified") == 0) {
        languageSetting = APP_LANG_CHINESE_SIMP;
    } else if (strcmp(language, "Chinese_Traditional") == 0) {
        languageSetting = APP_LANG_CHINESE_TRAD;
    } else if (strcmp(language, "English") == 0) {
        languageSetting = APP_LANG_ENGLISH;
    } else if (strcmp(language, "Spanish") == 0) {
        languageSetting = APP_LANG_SPANISH;
    } else if (strcmp(language, "French") == 0) {
        languageSetting = APP_LANG_FRENCH;
    } else if (strcmp(language, "German") == 0) {
        languageSetting = APP_LANG_GERMAN;
    } else if (strcmp(language, "Russian") == 0) {
        languageSetting = APP_LANG_RUSSIAN;
    } else if (strcmp(language, "Portuguese") == 0) {
        languageSetting = APP_LANG_PORTUGUESE;
    } else if (strcmp(language, "Japanese") == 0) {
        languageSetting = APP_LANG_JAPANESE;
    } else if (strcmp(language, "Korean") == 0) {
        languageSetting = APP_LANG_KOREAN;
    } else {
        // 尝试按数字解析（向后兼容）
        int langValue = atoi(language);
        if (langValue >= 0 && langValue < APP_LANG_COUNT) {
            languageSetting = langValue;
        } else {
            languageSetting = APP_LANG_ENGLISH; // 默认英语
        }
    }
    
    // ======== [Display] 节 ========
    ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), config_path);
    CLOCK_BASE_FONT_SIZE = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "Wallpoet Essence.ttf", FONT_FILE_NAME, sizeof(FONT_FILE_NAME), config_path);
    
    // 从字体文件名中截取内部名称
    size_t font_name_len = strlen(FONT_FILE_NAME);
    if (font_name_len > 4 && strcmp(FONT_FILE_NAME + font_name_len - 4, ".ttf") == 0) {
        // 确保目标大小足够，避免依赖源字符串长度
        size_t copy_len = font_name_len - 4;
        if (copy_len >= sizeof(FONT_INTERNAL_NAME))
            copy_len = sizeof(FONT_INTERNAL_NAME) - 1;
        
        memcpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, copy_len);
        FONT_INTERNAL_NAME[copy_len] = '\0';
    } else {
        strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }
    
    CLOCK_WINDOW_POS_X = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 960, config_path);
    CLOCK_WINDOW_POS_Y = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", -1, config_path);
    
    char scaleStr[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", scaleStr, sizeof(scaleStr), config_path);
    CLOCK_WINDOW_SCALE = atof(scaleStr);
    
    CLOCK_WINDOW_TOPMOST = ReadIniBool(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", TRUE, config_path);
    
    // 检查并替换纯黑色
    if (strcasecmp(CLOCK_TEXT_COLOR, "#000000") == 0) {
        strncpy(CLOCK_TEXT_COLOR, "#000001", sizeof(CLOCK_TEXT_COLOR) - 1);
    }
    
    // ======== [Timer] 节 ========
    CLOCK_DEFAULT_START_TIME = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    CLOCK_USE_24HOUR = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", FALSE, config_path);
    CLOCK_SHOW_SECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", FALSE, config_path);
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), config_path);
    
    // 读取超时动作
    char timeoutAction[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", timeoutAction, sizeof(timeoutAction), config_path);
    
    if (strcmp(timeoutAction, "MESSAGE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "LOCK") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
    } else if (strcmp(timeoutAction, "SHUTDOWN") == 0) {
        // 即使配置文件中有SHUTDOWN，也将其视为一次性操作，默认为MESSAGE
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "RESTART") == 0) {
        // 即使配置文件中有RESTART，也将其视为一次性操作，默认为MESSAGE
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "OPEN_FILE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    } else if (strcmp(timeoutAction, "SHOW_TIME") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
    } else if (strcmp(timeoutAction, "COUNT_UP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
    } else if (strcmp(timeoutAction, "OPEN_WEBSITE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    }
    
    // 读取超时文件和网站设置
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CLOCK_TIMEOUT_FILE_PATH, MAX_PATH, config_path);
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH, config_path);
    
    // 如果文件路径有效，确保设置超时动作为打开文件
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
        GetFileAttributesA(CLOCK_TIMEOUT_FILE_PATH) != INVALID_FILE_ATTRIBUTES) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    }
    
    // 如果URL有效，确保设置超时动作为打开网站
    if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    }
    
    // 读取时间选项
    char timeOptions[256] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "25,10,5", timeOptions, sizeof(timeOptions), config_path);
    
    char *token = strtok(timeOptions, ",");
    while (token && time_options_count < MAX_TIME_OPTIONS) {
        while (*token == ' ') token++;
        time_options[time_options_count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    // 读取启动模式
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE), config_path);
    
    // ======== [Pomodoro] 节 ========
    char pomodoroTimeOptions[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    
    // 重置番茄钟时间计数
    POMODORO_TIMES_COUNT = 0;
    
    // 解析所有番茄钟时间值
    token = strtok(pomodoroTimeOptions, ",");
    while (token && POMODORO_TIMES_COUNT < MAX_POMODORO_TIMES) {
        POMODORO_TIMES[POMODORO_TIMES_COUNT++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    // 即使我们现在使用新的数组存储所有时间，
    // 为了向后兼容，依然保留这三个变量的设置
    if (POMODORO_TIMES_COUNT > 0) {
        POMODORO_WORK_TIME = POMODORO_TIMES[0];
        if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
        if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[3]; // 注意这是第4个值
    }
    
    // 读取番茄钟循环次数
    POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
    
    // ======== [Notification] 节 ========
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", 
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
                 
    NOTIFICATION_TIMEOUT_MS = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    NOTIFICATION_MAX_OPACITY = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    
    // 确保透明度在有效范围内(1-100)
    if (NOTIFICATION_MAX_OPACITY < 1) NOTIFICATION_MAX_OPACITY = 1;
    if (NOTIFICATION_MAX_OPACITY > 100) NOTIFICATION_MAX_OPACITY = 100;
    
    char notificationType[32] = {0};
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", notificationType, sizeof(notificationType), config_path);
    
    // 设置通知类型
    if (strcmp(notificationType, "CATIME") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
    } else if (strcmp(notificationType, "SYSTEM_MODAL") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (strcmp(notificationType, "OS") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
    } else {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME; // 默认值
    }
    
    // 读取通知音频文件路径
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", 
                NOTIFICATION_SOUND_FILE, MAX_PATH, config_path);
                
    // 读取通知音频音量
    NOTIFICATION_SOUND_VOLUME = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
                
    // 读取是否禁用通知窗口
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
    
    // 确保音量在有效范围内(0-100)
    if (NOTIFICATION_SOUND_VOLUME < 0) NOTIFICATION_SOUND_VOLUME = 0;
    if (NOTIFICATION_SOUND_VOLUME > 100) NOTIFICATION_SOUND_VOLUME = 100;
    
    // ======== [Colors] 节 ========
    char colorOptions[1024] = {0};
    ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                colorOptions, sizeof(colorOptions), config_path);
                
    // 解析颜色选项
    token = strtok(colorOptions, ",");
    COLOR_OPTIONS_COUNT = 0;
    while (token) {
        COLOR_OPTIONS = realloc(COLOR_OPTIONS, sizeof(PredefinedColor) * (COLOR_OPTIONS_COUNT + 1));
        if (COLOR_OPTIONS) {
            COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = strdup(token);
            COLOR_OPTIONS_COUNT++;
        }
        token = strtok(NULL, ",");
    }
    
    // ======== [RecentFiles] 节 ========
    // 读取最近文件记录
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        ReadIniString(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path);
        
        if (strlen(filePath) > 0) {
            // 转换为宽字符以正确检查文件是否存在
            wchar_t widePath[MAX_PATH] = {0};
            MultiByteToWideChar(CP_UTF8, 0, filePath, -1, widePath, MAX_PATH);
            
            // 检查文件是否存在
            if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, filePath, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';

                ExtractFileName(filePath, CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, MAX_PATH);
                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }
    
    // ======== [Hotkeys] 节 ========
    // 从INI文件读取热键配置
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    // 读取各个热键设置
    char hotkeyStr[32] = {0};
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    showTimeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    countUpHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    countdownHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown1Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown2Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown3Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    pomodoroHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    toggleVisibilityHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    editModeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    pauseResumeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    restartTimerHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    customCountdownHotkey = StringToHotkey(hotkeyStr);
    
    last_config_time = time(NULL);

    // 应用窗口位置
    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    // 应用语言设置
    SetLanguage((AppLanguage)languageSetting);
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
    if (strcmp(action, "RESTART") == 0 || strcmp(action, "SHUTDOWN") == 0 || strcmp(action, "SLEEP") == 0) {
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
 * 按照统一的组织结构写入所有配置项到INI格式文件中，包含以下分节：
 * 1. [General] - 基本设置（版本信息、语言设置）
 * 2. [Display] - 显示设置（颜色、字体、窗口位置等）
 * 3. [Timer] - 计时器相关设置（默认时间等）
 * 4. [Pomodoro] - 番茄钟相关设置
 * 5. [Notification] - 通知相关设置
 * 6. [Hotkeys] - 热键设置
 * 7. [RecentFiles] - 最近使用的文件
 * 8. [Colors] - 颜色选项
 * 9. [Options] - 其他选项
 */
void WriteConfig(const char* config_path) {
    // 获取当前语言的名称
    AppLanguage currentLang = GetCurrentLanguage();
    const char* langName;
    
    switch (currentLang) {
        case APP_LANG_CHINESE_SIMP:
            langName = "Chinese_Simplified";
            break;
        case APP_LANG_CHINESE_TRAD:
            langName = "Chinese_Traditional";
            break;
        case APP_LANG_SPANISH:
            langName = "Spanish";
            break;
        case APP_LANG_FRENCH:
            langName = "French";
            break;
        case APP_LANG_GERMAN:
            langName = "German";
            break;
        case APP_LANG_RUSSIAN:
            langName = "Russian";
            break;
        case APP_LANG_PORTUGUESE:
            langName = "Portuguese";
            break;
        case APP_LANG_JAPANESE:
            langName = "Japanese";
            break;
        case APP_LANG_KOREAN:
            langName = "Korean";
            break;
        case APP_LANG_ENGLISH:
        default:
            langName = "English";
            break;
    }
    
    // 根据通知类型选择字符串表示
    const char* typeStr;
    switch (NOTIFICATION_TYPE) {
        case NOTIFICATION_TYPE_CATIME:
            typeStr = "CATIME";
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            typeStr = "SYSTEM_MODAL";
            break;
        case NOTIFICATION_TYPE_OS:
            typeStr = "OS";
            break;
        default:
            typeStr = "CATIME"; // 默认值
            break;
    }
    
    // 读取热键设置
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                      &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                      &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                      &pauseResumeHotkey, &restartTimerHotkey);
    
    ReadCustomCountdownHotkey(&customCountdownHotkey);
    
    // 将热键值转换为可读格式
    char showTimeStr[64] = {0};
    char countUpStr[64] = {0};
    char countdownStr[64] = {0};
    char quickCountdown1Str[64] = {0};
    char quickCountdown2Str[64] = {0};
    char quickCountdown3Str[64] = {0};
    char pomodoroStr[64] = {0};
    char toggleVisibilityStr[64] = {0};
    char editModeStr[64] = {0};
    char pauseResumeStr[64] = {0};
    char restartTimerStr[64] = {0};
    char customCountdownStr[64] = {0};
    
    HotkeyToString(showTimeHotkey, showTimeStr, sizeof(showTimeStr));
    HotkeyToString(countUpHotkey, countUpStr, sizeof(countUpStr));
    HotkeyToString(countdownHotkey, countdownStr, sizeof(countdownStr));
    HotkeyToString(quickCountdown1Hotkey, quickCountdown1Str, sizeof(quickCountdown1Str));
    HotkeyToString(quickCountdown2Hotkey, quickCountdown2Str, sizeof(quickCountdown2Str));
    HotkeyToString(quickCountdown3Hotkey, quickCountdown3Str, sizeof(quickCountdown3Str));
    HotkeyToString(pomodoroHotkey, pomodoroStr, sizeof(pomodoroStr));
    HotkeyToString(toggleVisibilityHotkey, toggleVisibilityStr, sizeof(toggleVisibilityStr));
    HotkeyToString(editModeHotkey, editModeStr, sizeof(editModeStr));
    HotkeyToString(pauseResumeHotkey, pauseResumeStr, sizeof(pauseResumeStr));
    HotkeyToString(restartTimerHotkey, restartTimerStr, sizeof(restartTimerStr));
    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
    
    // 准备时间选项字符串
    char timeOptionsStr[256] = {0};
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", time_options[i]);
        
        if (i > 0) {
            strcat(timeOptionsStr, ",");
        }
        strcat(timeOptionsStr, buffer);
    }
    
    // 准备番茄钟时间选项字符串
    char pomodoroTimesStr[256] = {0};
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", POMODORO_TIMES[i]);
        
        if (i > 0) {
            strcat(pomodoroTimesStr, ",");
        }
        strcat(pomodoroTimesStr, buffer);
    }
    
    // 准备颜色选项字符串
    char colorOptionsStr[1024] = {0};
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) {
            strcat(colorOptionsStr, ",");
        }
        strcat(colorOptionsStr, COLOR_OPTIONS[i].hexColor);
    }
    
    // 确定超时动作字符串
    const char* timeoutActionStr;
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE:
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_LOCK:
            timeoutActionStr = "LOCK";
            break;
        case TIMEOUT_ACTION_OPEN_FILE:
            timeoutActionStr = "OPEN_FILE";
            break;
        case TIMEOUT_ACTION_OPEN_WEBSITE:
            timeoutActionStr = "OPEN_WEBSITE";
            break;
        case TIMEOUT_ACTION_SHOW_TIME:
            timeoutActionStr = "SHOW_TIME";
            break;
        case TIMEOUT_ACTION_COUNT_UP:
            timeoutActionStr = "COUNT_UP";
            break;
        case TIMEOUT_ACTION_RESTART:
            // 即使是重启操作，也将其视为一次性操作，存储默认值
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_SHUTDOWN:
            // 即使是关机操作，也将其视为一次性操作，存储默认值
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_SLEEP:
            // 即使是休眠操作，也将其视为一次性操作，存储默认值
            timeoutActionStr = "MESSAGE";
            break;
        default:
            timeoutActionStr = "MESSAGE";
            break;
    }
    
    // ======== [General] 节 ========
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", IsShortcutCheckDone() ? "TRUE" : "FALSE", config_path);
    
    // ======== [Display] 节 ========
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", CLOCK_BASE_FONT_SIZE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONT_FILE_NAME, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);
    
    char scaleStr[16];
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr, config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE", config_path);
    
    // ======== [Timer] 节 ========
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", CLOCK_DEFAULT_START_TIME, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", CLOCK_TIMEOUT_TEXT, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", timeoutActionStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", CLOCK_TIMEOUT_FILE_PATH, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", CLOCK_TIMEOUT_WEBSITE_URL, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", timeOptionsStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", CLOCK_STARTUP_MODE, config_path);
    
    // ======== [Pomodoro] 节 ========
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", pomodoroTimesStr, config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", POMODORO_LOOP_COUNT, config_path);
    
    // ======== [Notification] 节 ========
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", CLOCK_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", POMODORO_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", POMODORO_CYCLE_COMPLETE_TEXT, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", NOTIFICATION_TIMEOUT_MS, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", NOTIFICATION_MAX_OPACITY, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", NOTIFICATION_SOUND_FILE, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", NOTIFICATION_SOUND_VOLUME, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", NOTIFICATION_DISABLED ? "TRUE" : "FALSE", config_path);
    
    // ======== [Hotkeys] 节 ========
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", showTimeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", countUpStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", countdownStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", quickCountdown1Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", quickCountdown2Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", quickCountdown3Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", pomodoroStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", toggleVisibilityStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", editModeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", pauseResumeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", restartTimerStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr, config_path);
    
    // ======== [RecentFiles] 节 ========
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, CLOCK_RECENT_FILES[i].path, config_path);
    }
    
    // 清除未使用的文件记录
    for (int i = CLOCK_RECENT_FILES_COUNT; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    // ======== [Colors] 节 ========
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", colorOptionsStr, config_path);
}

/**
 * @brief 写入超时打开网站的URL
 * @param url 目标网站URL
 * 
 * 更新配置文件中的超时打开网站URL，同时设置超时动作为打开网站。
 * 使用临时文件方式确保配置更新过程安全可靠。
 */
void WriteConfigTimeoutWebsite(const char* url) {
    // 只有在提供了有效URL的情况下才设置超时动作为打开网站
    if (url && url[0] != '\0') {
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
    
    FILE *source_file, *temp_file;
    
    // 使用标准C文件操作代替Windows API
    source_file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL timeoutFound = FALSE;
    BOOL pomodoroFound = FALSE;
    BOOL cycleFound = FALSE;
    
    // 逐行读取并写入
    while (fgets(line, sizeof(line), source_file)) {
        // 移除行尾换行符进行比较
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
            if (len > 0 && line[len-1] == '\r')
                line[--len] = '\0';
        }
        
        if (strncmp(line, "CLOCK_TIMEOUT_MESSAGE_TEXT=", 27) == 0) {
            fprintf(temp_file, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\n", timeout_msg);
            timeoutFound = TRUE;
        } else if (strncmp(line, "POMODORO_TIMEOUT_MESSAGE_TEXT=", 30) == 0) {
            fprintf(temp_file, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\n", pomodoro_msg);
            pomodoroFound = TRUE;
        } else if (strncmp(line, "POMODORO_CYCLE_COMPLETE_TEXT=", 29) == 0) {
            fprintf(temp_file, "POMODORO_CYCLE_COMPLETE_TEXT=%s\n", cycle_complete_msg);
            cycleFound = TRUE;
        } else {
            // 还原行尾换行符，原样写回
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!timeoutFound) {
        fprintf(temp_file, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\n", timeout_msg);
    }
    
    if (!pomodoroFound) {
        fprintf(temp_file, "POMODORO_TIMEOUT_MESSAGE_TEXT=%s\n", pomodoro_msg);
    }
    
    if (!cycleFound) {
        fprintf(temp_file, "POMODORO_CYCLE_COMPLETE_TEXT=%s\n", cycle_complete_msg);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
    
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
 * 更新配置文件中的通知显示时间，并更新全局变量。
 */
void WriteConfigNotificationTimeout(int timeout_ms) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    source_file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    
    // 逐行读取并写入
    while (fgets(line, sizeof(line), source_file)) {
        // 移除行尾换行符进行比较
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
            if (len > 0 && line[len-1] == '\r')
                line[--len] = '\0';
        }
        
        if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            fprintf(temp_file, "NOTIFICATION_TIMEOUT_MS=%d\n", timeout_ms);
            found = TRUE;
        } else {
            // 还原行尾换行符，原样写回
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_TIMEOUT_MS=%d\n", timeout_ms);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
    
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

/**
 * @brief 写入通知最大透明度配置
 * @param opacity 透明度百分比值(1-100)
 * 
 * 更新配置文件中的通知最大透明度设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationOpacity(int opacity) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    source_file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    
    // 逐行读取并写入
    while (fgets(line, sizeof(line), source_file)) {
        // 移除行尾换行符进行比较
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
            if (len > 0 && line[len-1] == '\r')
                line[--len] = '\0';
        }
        
        if (strncmp(line, "NOTIFICATION_MAX_OPACITY=", 25) == 0) {
            fprintf(temp_file, "NOTIFICATION_MAX_OPACITY=%d\n", opacity);
            found = TRUE;
        } else {
            // 还原行尾换行符，原样写回
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_MAX_OPACITY=%d\n", opacity);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    NOTIFICATION_MAX_OPACITY = opacity;
}

/**
 * @brief 从配置文件中读取通知类型设置
 *
 * 读取配置文件中的NOTIFICATION_TYPE项，并更新全局变量。
 * 若配置项不存在，保持默认值（Catime通知窗口）。
 */
void ReadNotificationTypeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "NOTIFICATION_TYPE=", 18) == 0) {
                char typeStr[32] = {0};
                sscanf(line + 18, "%31s", typeStr);
                
                // 根据字符串设置通知类型
                if (strcmp(typeStr, "CATIME") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (strcmp(typeStr, "SYSTEM_MODAL") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                } else if (strcmp(typeStr, "OS") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else {
                    // 无效类型使用默认值
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                }
                break;
            }
        }
        fclose(file);
    }
}

/**
 * @brief 写入通知类型配置
 * @param type 通知类型枚举值
 *
 * 更新配置文件中的通知类型设置，采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationType(NotificationType type) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 确保类型值在有效范围内
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME; // 默认值
    }
    
    // 更新全局变量
    NOTIFICATION_TYPE = type;
    
    // 将枚举转换为字符串
    const char* typeStr;
    switch (type) {
        case NOTIFICATION_TYPE_CATIME:
            typeStr = "CATIME";
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            typeStr = "SYSTEM_MODAL";
            break;
        case NOTIFICATION_TYPE_OS:
            typeStr = "OS";
            break;
        default:
            typeStr = "CATIME"; // 默认值
            break;
    }
    
    // 构建临时文件路径
    char temp_path[MAX_PATH];
    strncpy(temp_path, config_path, MAX_PATH - 5);
    strcat(temp_path, ".tmp");
    
    FILE *source = fopen(config_path, "r");
    FILE *target = fopen(temp_path, "w");
    
    if (source && target) {
        char line[256];
        BOOL found = FALSE;
        
        // 复制文件内容，替换目标配置行
        while (fgets(line, sizeof(line), source)) {
            if (strncmp(line, "NOTIFICATION_TYPE=", 18) == 0) {
                fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
                found = TRUE;
            } else {
                fputs(line, target);
            }
        }
        
        // 如果没找到配置项，添加到文件末尾
        if (!found) {
            fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
        }
        
        fclose(source);
        fclose(target);
        
        // 替换原始文件
        remove(config_path);
        rename(temp_path, config_path);
    } else {
        // 清理可能打开的文件
        if (source) fclose(source);
        if (target) fclose(target);
    }
}

/**
 * @brief 获取音频文件夹路径
 * @param path 存储音频文件夹路径的缓冲区
 * @param size 缓冲区大小
 */
void GetAudioFolderPath(char* path, size_t size) {
    if (!path || size == 0) return;

    char* appdata_path = getenv("LOCALAPPDATA");
    if (appdata_path) {
        if (snprintf(path, size, "%s\\Catime\\resources\\audio", appdata_path) >= size) {
            strncpy(path, ".\\resources\\audio", size - 1);
            path[size - 1] = '\0';
            return;
        }
        
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime\\resources\\audio", appdata_path) < sizeof(dir_path)) {
            if (!CreateDirectoryA(dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\resources\\audio", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\resources\\audio", size - 1);
        path[size - 1] = '\0';
    }
}

/**
 * @brief 从配置文件中读取通知音频设置
 */
void ReadNotificationSoundConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            char* value = line + 23;  // 正确的偏移量，跳过"NOTIFICATION_SOUND_FILE="
            // 移除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 确保路径不包含等号
            if (value[0] == '=') {
                value++; // 如果第一个字符是等号，跳过它
            }
            
            // 复制到全局变量，确保清零
            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
            strncpy(NOTIFICATION_SOUND_FILE, value, MAX_PATH - 1);
            NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
            break;
        }
    }
    
    fclose(file);
}

/**
 * @brief 写入通知音频配置
 * @param sound_file 音频文件路径
 */
void WriteConfigNotificationSound(const char* sound_file) {
    if (!sound_file) return;
    
    // 检查路径是否包含等号，如果有则移除
    char clean_path[MAX_PATH] = {0};
    const char* src = sound_file;
    char* dst = clean_path;
    
    while (*src && (dst - clean_path) < (MAX_PATH - 1)) {
        if (*src != '=') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 创建临时文件路径
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE* source = fopen(config_path, "r");
    if (!source) return;
    
    FILE* dest = fopen(temp_path, "w");
    if (!dest) {
        fclose(source);
        return;
    }
    
    char line[1024];
    int found = 0;
    
    // 复制文件内容，替换或添加通知音频设置
    while (fgets(line, sizeof(line), source)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
            found = 1;
        } else {
            fputs(line, dest);
        }
    }
    
    // 如果没有找到配置项，添加到文件末尾
    if (!found) {
        fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
    }
    
    fclose(source);
    fclose(dest);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
    strncpy(NOTIFICATION_SOUND_FILE, clean_path, MAX_PATH - 1);
    NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
}

/**
 * @brief 从配置文件中读取通知音频音量
 */
void ReadNotificationVolumeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "NOTIFICATION_SOUND_VOLUME=", 26) == 0) {
            int volume = atoi(line + 26);
            if (volume >= 0 && volume <= 100) {
                NOTIFICATION_SOUND_VOLUME = volume;
            }
            break;
        }
    }
    
    fclose(file);
}

/**
 * @brief 写入通知音频音量配置
 * @param volume 音量百分比值(0-100)
 */
void WriteConfigNotificationVolume(int volume) {
    // 验证音量范围
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // 更新全局变量
    NOTIFICATION_SOUND_VOLUME = volume;
    
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
        if (strncmp(line, "NOTIFICATION_SOUND_VOLUME=", 26) == 0) {
            fprintf(temp, "NOTIFICATION_SOUND_VOLUME=%d\n", volume);
            found = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "NOTIFICATION_SOUND_VOLUME=%d\n", volume);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 从配置文件中读取热键设置
 * @param showTimeHotkey 存储显示时间热键的指针
 * @param countUpHotkey 存储正计时热键的指针
 * @param countdownHotkey 存储倒计时热键的指针
 * @param quickCountdown1Hotkey 存储快捷倒计时1热键的指针
 * @param quickCountdown2Hotkey 存储快捷倒计时2热键的指针
 * @param quickCountdown3Hotkey 存储快捷倒计时3热键的指针
 * @param pomodoroHotkey 存储番茄钟热键的指针
 * @param toggleVisibilityHotkey 存储隐藏/显示热键的指针
 * @param editModeHotkey 存储编辑模式热键的指针
 * @param pauseResumeHotkey 存储暂停/继续热键的指针
 * @param restartTimerHotkey 存储重新开始热键的指针
 * 
 * 专门读取热键配置项并更新相应的参数值。
 * 支持解析可读性格式的热键字符串。
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                       WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                       WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                       WORD* pauseResumeHotkey, WORD* restartTimerHotkey)
{
    // 参数校验
    if (!showTimeHotkey || !countUpHotkey || !countdownHotkey || 
        !quickCountdown1Hotkey || !quickCountdown2Hotkey || !quickCountdown3Hotkey ||
        !pomodoroHotkey || !toggleVisibilityHotkey || !editModeHotkey || 
        !pauseResumeHotkey || !restartTimerHotkey) return;
    
    // 初始化为0（表示未设置热键）
    *showTimeHotkey = 0;
    *countUpHotkey = 0;
    *countdownHotkey = 0;
    *quickCountdown1Hotkey = 0;
    *quickCountdown2Hotkey = 0;
    *quickCountdown3Hotkey = 0;
    *pomodoroHotkey = 0;
    *toggleVisibilityHotkey = 0;
    *editModeHotkey = 0;
    *pauseResumeHotkey = 0;
    *restartTimerHotkey = 0;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "HOTKEY_SHOW_TIME=", 17) == 0) {
            char* value = line + 17;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *showTimeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNT_UP=", 16) == 0) {
            char* value = line + 16;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *countUpHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNTDOWN=", 17) == 0) {
            char* value = line + 17;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *countdownHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN1=", 24) == 0) {
            char* value = line + 24;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *quickCountdown1Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN2=", 24) == 0) {
            char* value = line + 24;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *quickCountdown2Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN3=", 24) == 0) {
            char* value = line + 24;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *quickCountdown3Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_POMODORO=", 16) == 0) {
            char* value = line + 16;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *pomodoroHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_TOGGLE_VISIBILITY=", 25) == 0) {
            char* value = line + 25;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *toggleVisibilityHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_EDIT_MODE=", 17) == 0) {
            char* value = line + 17;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *editModeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_PAUSE_RESUME=", 20) == 0) {
            char* value = line + 20;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *pauseResumeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_RESTART_TIMER=", 21) == 0) {
            char* value = line + 21;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *restartTimerHotkey = StringToHotkey(value);
        }
    }
    
    fclose(file);
}

/**
 * @brief 写入热键配置
 * @param showTimeHotkey 显示时间热键值
 * @param countUpHotkey 正计时热键值
 * @param countdownHotkey 倒计时热键值
 * @param quickCountdown1Hotkey 快捷倒计时1热键值
 * @param quickCountdown2Hotkey 快捷倒计时2热键值
 * @param quickCountdown3Hotkey 快捷倒计时3热键值
 * @param pomodoroHotkey 番茄钟热键值
 * @param toggleVisibilityHotkey 隐藏/显示热键值
 * @param editModeHotkey 编辑模式热键值
 * @param pauseResumeHotkey 暂停/继续热键值
 * @param restartTimerHotkey 重新开始热键值
 * 
 * 更新配置文件中的热键设置，
 * 采用临时文件方式确保配置更新安全。
 * 将热键值转换为可读性更好的格式再保存。
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) {
        // 如果文件不存在，则创建新文件
        file = fopen(config_path, "w");
        if (!file) return;
        
        // 将热键值转换为可读格式
        char showTimeStr[64] = {0};
        char countUpStr[64] = {0};
        char countdownStr[64] = {0};
        char quickCountdown1Str[64] = {0};
        char quickCountdown2Str[64] = {0};
        char quickCountdown3Str[64] = {0};
        char pomodoroStr[64] = {0};
        char toggleVisibilityStr[64] = {0};
        char editModeStr[64] = {0};
        char pauseResumeStr[64] = {0};
        char restartTimerStr[64] = {0};
        char customCountdownStr[64] = {0}; // 新增自定义倒计时热键
        
        // 转换各个热键
        HotkeyToString(showTimeHotkey, showTimeStr, sizeof(showTimeStr));
        HotkeyToString(countUpHotkey, countUpStr, sizeof(countUpStr));
        HotkeyToString(countdownHotkey, countdownStr, sizeof(countdownStr));
        HotkeyToString(quickCountdown1Hotkey, quickCountdown1Str, sizeof(quickCountdown1Str));
        HotkeyToString(quickCountdown2Hotkey, quickCountdown2Str, sizeof(quickCountdown2Str));
        HotkeyToString(quickCountdown3Hotkey, quickCountdown3Str, sizeof(quickCountdown3Str));
        HotkeyToString(pomodoroHotkey, pomodoroStr, sizeof(pomodoroStr));
        HotkeyToString(toggleVisibilityHotkey, toggleVisibilityStr, sizeof(toggleVisibilityStr));
        HotkeyToString(editModeHotkey, editModeStr, sizeof(editModeStr));
        HotkeyToString(pauseResumeHotkey, pauseResumeStr, sizeof(pauseResumeStr));
        HotkeyToString(restartTimerHotkey, restartTimerStr, sizeof(restartTimerStr));
        // 获取自定义倒计时热键的值
        WORD customCountdownHotkey = 0;
        ReadCustomCountdownHotkey(&customCountdownHotkey);
        HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
        
        // 写入热键配置
        fprintf(file, "HOTKEY_SHOW_TIME=%s\n", showTimeStr);
        fprintf(file, "HOTKEY_COUNT_UP=%s\n", countUpStr);
        fprintf(file, "HOTKEY_COUNTDOWN=%s\n", countdownStr);
        fprintf(file, "HOTKEY_QUICK_COUNTDOWN1=%s\n", quickCountdown1Str);
        fprintf(file, "HOTKEY_QUICK_COUNTDOWN2=%s\n", quickCountdown2Str);
        fprintf(file, "HOTKEY_QUICK_COUNTDOWN3=%s\n", quickCountdown3Str);
        fprintf(file, "HOTKEY_POMODORO=%s\n", pomodoroStr);
        fprintf(file, "HOTKEY_TOGGLE_VISIBILITY=%s\n", toggleVisibilityStr);
        fprintf(file, "HOTKEY_EDIT_MODE=%s\n", editModeStr);
        fprintf(file, "HOTKEY_PAUSE_RESUME=%s\n", pauseResumeStr);
        fprintf(file, "HOTKEY_RESTART_TIMER=%s\n", restartTimerStr);
        fprintf(file, "HOTKEY_CUSTOM_COUNTDOWN=%s\n", customCountdownStr); // 添加新的热键
        
        fclose(file);
        return;
    }
    
    // 文件存在，读取所有行并更新热键设置
    char temp_path[MAX_PATH];
    sprintf(temp_path, "%s.tmp", config_path);
    FILE* temp_file = fopen(temp_path, "w");
    
    if (!temp_file) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL foundShowTime = FALSE;
    BOOL foundCountUp = FALSE;
    BOOL foundCountdown = FALSE;
    BOOL foundQuickCountdown1 = FALSE;
    BOOL foundQuickCountdown2 = FALSE;
    BOOL foundQuickCountdown3 = FALSE;
    BOOL foundPomodoro = FALSE;
    BOOL foundToggleVisibility = FALSE;
    BOOL foundEditMode = FALSE;
    BOOL foundPauseResume = FALSE;
    BOOL foundRestartTimer = FALSE;
    
    // 将热键值转换为可读格式
    char showTimeStr[64] = {0};
    char countUpStr[64] = {0};
    char countdownStr[64] = {0};
    char quickCountdown1Str[64] = {0};
    char quickCountdown2Str[64] = {0};
    char quickCountdown3Str[64] = {0};
    char pomodoroStr[64] = {0};
    char toggleVisibilityStr[64] = {0};
    char editModeStr[64] = {0};
    char pauseResumeStr[64] = {0};
    char restartTimerStr[64] = {0};
    
    // 转换各个热键
    HotkeyToString(showTimeHotkey, showTimeStr, sizeof(showTimeStr));
    HotkeyToString(countUpHotkey, countUpStr, sizeof(countUpStr));
    HotkeyToString(countdownHotkey, countdownStr, sizeof(countdownStr));
    HotkeyToString(quickCountdown1Hotkey, quickCountdown1Str, sizeof(quickCountdown1Str));
    HotkeyToString(quickCountdown2Hotkey, quickCountdown2Str, sizeof(quickCountdown2Str));
    HotkeyToString(quickCountdown3Hotkey, quickCountdown3Str, sizeof(quickCountdown3Str));
    HotkeyToString(pomodoroHotkey, pomodoroStr, sizeof(pomodoroStr));
    HotkeyToString(toggleVisibilityHotkey, toggleVisibilityStr, sizeof(toggleVisibilityStr));
    HotkeyToString(editModeHotkey, editModeStr, sizeof(editModeStr));
    HotkeyToString(pauseResumeHotkey, pauseResumeStr, sizeof(pauseResumeStr));
    HotkeyToString(restartTimerHotkey, restartTimerStr, sizeof(restartTimerStr));
    
    // 处理每一行
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "HOTKEY_SHOW_TIME=", 17) == 0) {
            fprintf(temp_file, "HOTKEY_SHOW_TIME=%s\n", showTimeStr);
            foundShowTime = TRUE;
        }
        else if (strncmp(line, "HOTKEY_COUNT_UP=", 16) == 0) {
            fprintf(temp_file, "HOTKEY_COUNT_UP=%s\n", countUpStr);
            foundCountUp = TRUE;
        }
        else if (strncmp(line, "HOTKEY_COUNTDOWN=", 17) == 0) {
            fprintf(temp_file, "HOTKEY_COUNTDOWN=%s\n", countdownStr);
            foundCountdown = TRUE;
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN1=", 24) == 0) {
            fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN1=%s\n", quickCountdown1Str);
            foundQuickCountdown1 = TRUE;
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN2=", 24) == 0) {
            fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN2=%s\n", quickCountdown2Str);
            foundQuickCountdown2 = TRUE;
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN3=", 24) == 0) {
            fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN3=%s\n", quickCountdown3Str);
            foundQuickCountdown3 = TRUE;
        }
        else if (strncmp(line, "HOTKEY_POMODORO=", 16) == 0) {
            fprintf(temp_file, "HOTKEY_POMODORO=%s\n", pomodoroStr);
            foundPomodoro = TRUE;
        }
        else if (strncmp(line, "HOTKEY_TOGGLE_VISIBILITY=", 25) == 0) {
            fprintf(temp_file, "HOTKEY_TOGGLE_VISIBILITY=%s\n", toggleVisibilityStr);
            foundToggleVisibility = TRUE;
        }
        else if (strncmp(line, "HOTKEY_EDIT_MODE=", 17) == 0) {
            fprintf(temp_file, "HOTKEY_EDIT_MODE=%s\n", editModeStr);
            foundEditMode = TRUE;
        }
        else if (strncmp(line, "HOTKEY_PAUSE_RESUME=", 20) == 0) {
            fprintf(temp_file, "HOTKEY_PAUSE_RESUME=%s\n", pauseResumeStr);
            foundPauseResume = TRUE;
        }
        else if (strncmp(line, "HOTKEY_RESTART_TIMER=", 21) == 0) {
            fprintf(temp_file, "HOTKEY_RESTART_TIMER=%s\n", restartTimerStr);
            foundRestartTimer = TRUE;
        }
        else {
            // 保留其他行
            fputs(line, temp_file);
        }
    }
    
    // 添加未找到的热键配置项
    if (!foundShowTime) {
        fprintf(temp_file, "HOTKEY_SHOW_TIME=%s\n", showTimeStr);
    }
    if (!foundCountUp) {
        fprintf(temp_file, "HOTKEY_COUNT_UP=%s\n", countUpStr);
    }
    if (!foundCountdown) {
        fprintf(temp_file, "HOTKEY_COUNTDOWN=%s\n", countdownStr);
    }
    if (!foundQuickCountdown1) {
        fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN1=%s\n", quickCountdown1Str);
    }
    if (!foundQuickCountdown2) {
        fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN2=%s\n", quickCountdown2Str);
    }
    if (!foundQuickCountdown3) {
        fprintf(temp_file, "HOTKEY_QUICK_COUNTDOWN3=%s\n", quickCountdown3Str);
    }
    if (!foundPomodoro) {
        fprintf(temp_file, "HOTKEY_POMODORO=%s\n", pomodoroStr);
    }
    if (!foundToggleVisibility) {
        fprintf(temp_file, "HOTKEY_TOGGLE_VISIBILITY=%s\n", toggleVisibilityStr);
    }
    if (!foundEditMode) {
        fprintf(temp_file, "HOTKEY_EDIT_MODE=%s\n", editModeStr);
    }
    if (!foundPauseResume) {
        fprintf(temp_file, "HOTKEY_PAUSE_RESUME=%s\n", pauseResumeStr);
    }
    if (!foundRestartTimer) {
        fprintf(temp_file, "HOTKEY_RESTART_TIMER=%s\n", restartTimerStr);
    }
    
    fclose(file);
    fclose(temp_file);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
}

/**
 * @brief 将热键值转换为可读字符串
 * @param hotkey 热键值
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * 
 * 将WORD格式的热键值转换为可读字符串格式，例如"Ctrl+Alt+A"
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    // 如果热键为0，表示未设置
    if (hotkey == 0) {
        strncpy(buffer, "None", bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }
    
    BYTE vk = LOBYTE(hotkey);    // 虚拟键码
    BYTE mod = HIBYTE(hotkey);   // 修饰键
    
    buffer[0] = '\0';
    size_t len = 0;
    
    // 添加修饰键
    if (mod & HOTKEYF_CONTROL) {
        strncpy(buffer, "Ctrl", bufferSize - 1);
        len = strlen(buffer);
    }
    
    if (mod & HOTKEYF_SHIFT) {
        if (len > 0 && len < bufferSize - 1) {
            buffer[len++] = '+';
            buffer[len] = '\0';
        }
        strncat(buffer, "Shift", bufferSize - len - 1);
        len = strlen(buffer);
    }
    
    if (mod & HOTKEYF_ALT) {
        if (len > 0 && len < bufferSize - 1) {
            buffer[len++] = '+';
            buffer[len] = '\0';
        }
        strncat(buffer, "Alt", bufferSize - len - 1);
        len = strlen(buffer);
    }
    
    // 添加虚拟键
    if (len > 0 && len < bufferSize - 1 && vk != 0) {
        buffer[len++] = '+';
        buffer[len] = '\0';
    }
    
    // 获取虚拟键名称
    if (vk >= 'A' && vk <= 'Z') {
        // 字母键
        char keyName[2] = {vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= '0' && vk <= '9') {
        // 数字键
        char keyName[2] = {vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        // 功能键
        char keyName[4];
        sprintf(keyName, "F%d", vk - VK_F1 + 1);
        strncat(buffer, keyName, bufferSize - len - 1);
    } else {
        // 其他特殊键
        switch (vk) {
            case VK_BACK:       strncat(buffer, "Backspace", bufferSize - len - 1); break;
            case VK_TAB:        strncat(buffer, "Tab", bufferSize - len - 1); break;
            case VK_RETURN:     strncat(buffer, "Enter", bufferSize - len - 1); break;
            case VK_ESCAPE:     strncat(buffer, "Esc", bufferSize - len - 1); break;
            case VK_SPACE:      strncat(buffer, "Space", bufferSize - len - 1); break;
            case VK_PRIOR:      strncat(buffer, "PageUp", bufferSize - len - 1); break;
            case VK_NEXT:       strncat(buffer, "PageDown", bufferSize - len - 1); break;
            case VK_END:        strncat(buffer, "End", bufferSize - len - 1); break;
            case VK_HOME:       strncat(buffer, "Home", bufferSize - len - 1); break;
            case VK_LEFT:       strncat(buffer, "Left", bufferSize - len - 1); break;
            case VK_UP:         strncat(buffer, "Up", bufferSize - len - 1); break;
            case VK_RIGHT:      strncat(buffer, "Right", bufferSize - len - 1); break;
            case VK_DOWN:       strncat(buffer, "Down", bufferSize - len - 1); break;
            case VK_INSERT:     strncat(buffer, "Insert", bufferSize - len - 1); break;
            case VK_DELETE:     strncat(buffer, "Delete", bufferSize - len - 1); break;
            case VK_NUMPAD0:    strncat(buffer, "Num0", bufferSize - len - 1); break;
            case VK_NUMPAD1:    strncat(buffer, "Num1", bufferSize - len - 1); break;
            case VK_NUMPAD2:    strncat(buffer, "Num2", bufferSize - len - 1); break;
            case VK_NUMPAD3:    strncat(buffer, "Num3", bufferSize - len - 1); break;
            case VK_NUMPAD4:    strncat(buffer, "Num4", bufferSize - len - 1); break;
            case VK_NUMPAD5:    strncat(buffer, "Num5", bufferSize - len - 1); break;
            case VK_NUMPAD6:    strncat(buffer, "Num6", bufferSize - len - 1); break;
            case VK_NUMPAD7:    strncat(buffer, "Num7", bufferSize - len - 1); break;
            case VK_NUMPAD8:    strncat(buffer, "Num8", bufferSize - len - 1); break;
            case VK_NUMPAD9:    strncat(buffer, "Num9", bufferSize - len - 1); break;
            case VK_MULTIPLY:   strncat(buffer, "Num*", bufferSize - len - 1); break;
            case VK_ADD:        strncat(buffer, "Num+", bufferSize - len - 1); break;
            case VK_SUBTRACT:   strncat(buffer, "Num-", bufferSize - len - 1); break;
            case VK_DECIMAL:    strncat(buffer, "Num.", bufferSize - len - 1); break;
            case VK_DIVIDE:     strncat(buffer, "Num/", bufferSize - len - 1); break;
            case VK_OEM_1:      strncat(buffer, ";", bufferSize - len - 1); break;
            case VK_OEM_PLUS:   strncat(buffer, "=", bufferSize - len - 1); break;
            case VK_OEM_COMMA:  strncat(buffer, ",", bufferSize - len - 1); break;
            case VK_OEM_MINUS:  strncat(buffer, "-", bufferSize - len - 1); break;
            case VK_OEM_PERIOD: strncat(buffer, ".", bufferSize - len - 1); break;
            case VK_OEM_2:      strncat(buffer, "/", bufferSize - len - 1); break;
            case VK_OEM_3:      strncat(buffer, "`", bufferSize - len - 1); break;
            case VK_OEM_4:      strncat(buffer, "[", bufferSize - len - 1); break;
            case VK_OEM_5:      strncat(buffer, "\\", bufferSize - len - 1); break;
            case VK_OEM_6:      strncat(buffer, "]", bufferSize - len - 1); break;
            case VK_OEM_7:      strncat(buffer, "'", bufferSize - len - 1); break;
            default:            
                // 对于其他未知键，使用十六进制表示
                {
                char keyName[8];
                sprintf(keyName, "0x%02X", vk);
                strncat(buffer, keyName, bufferSize - len - 1);
                }
                break;
        }
    }
}

/**
 * @brief 将字符串转换为热键值
 * @param str 热键字符串
 * @return WORD 热键值
 * 
 * 将可读字符串格式的热键（如"Ctrl+Alt+A"）转换为WORD格式的热键值
 */
WORD StringToHotkey(const char* str) {
    if (!str || str[0] == '\0' || strcmp(str, "None") == 0) {
        return 0;  // 未设置热键
    }
    
    // 尝试直接解析为数字（兼容旧格式）
    if (isdigit(str[0])) {
        return (WORD)atoi(str);
    }
    
    BYTE vk = 0;    // 虚拟键码
    BYTE mod = 0;   // 修饰键
    
    // 复制字符串以便使用strtok
    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // 分割字符串，查找修饰键和主键
    char* token = strtok(buffer, "+");
    char* lastToken = NULL;
    
    while (token) {
        if (stricmp(token, "Ctrl") == 0) {
            mod |= HOTKEYF_CONTROL;
        } else if (stricmp(token, "Shift") == 0) {
            mod |= HOTKEYF_SHIFT;
        } else if (stricmp(token, "Alt") == 0) {
            mod |= HOTKEYF_ALT;
        } else {
            // 可能是主键
            lastToken = token;
        }
        token = strtok(NULL, "+");
    }
    
    // 解析主键
    if (lastToken) {
        // 检查是否是单个字符的字母或数字
        if (strlen(lastToken) == 1) {
            char ch = toupper(lastToken[0]);
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                vk = ch;
            }
        } 
        // 检查是否是功能键
        else if (lastToken[0] == 'F' && isdigit(lastToken[1])) {
            int fNum = atoi(lastToken + 1);
            if (fNum >= 1 && fNum <= 24) {
                vk = VK_F1 + fNum - 1;
            }
        }
        // 检查特殊键名
        else if (stricmp(lastToken, "Backspace") == 0) vk = VK_BACK;
        else if (stricmp(lastToken, "Tab") == 0) vk = VK_TAB;
        else if (stricmp(lastToken, "Enter") == 0) vk = VK_RETURN;
        else if (stricmp(lastToken, "Esc") == 0) vk = VK_ESCAPE;
        else if (stricmp(lastToken, "Space") == 0) vk = VK_SPACE;
        else if (stricmp(lastToken, "PageUp") == 0) vk = VK_PRIOR;
        else if (stricmp(lastToken, "PageDown") == 0) vk = VK_NEXT;
        else if (stricmp(lastToken, "End") == 0) vk = VK_END;
        else if (stricmp(lastToken, "Home") == 0) vk = VK_HOME;
        else if (stricmp(lastToken, "Left") == 0) vk = VK_LEFT;
        else if (stricmp(lastToken, "Up") == 0) vk = VK_UP;
        else if (stricmp(lastToken, "Right") == 0) vk = VK_RIGHT;
        else if (stricmp(lastToken, "Down") == 0) vk = VK_DOWN;
        else if (stricmp(lastToken, "Insert") == 0) vk = VK_INSERT;
        else if (stricmp(lastToken, "Delete") == 0) vk = VK_DELETE;
        else if (stricmp(lastToken, "Num0") == 0) vk = VK_NUMPAD0;
        else if (stricmp(lastToken, "Num1") == 0) vk = VK_NUMPAD1;
        else if (stricmp(lastToken, "Num2") == 0) vk = VK_NUMPAD2;
        else if (stricmp(lastToken, "Num3") == 0) vk = VK_NUMPAD3;
        else if (stricmp(lastToken, "Num4") == 0) vk = VK_NUMPAD4;
        else if (stricmp(lastToken, "Num5") == 0) vk = VK_NUMPAD5;
        else if (stricmp(lastToken, "Num6") == 0) vk = VK_NUMPAD6;
        else if (stricmp(lastToken, "Num7") == 0) vk = VK_NUMPAD7;
        else if (stricmp(lastToken, "Num8") == 0) vk = VK_NUMPAD8;
        else if (stricmp(lastToken, "Num9") == 0) vk = VK_NUMPAD9;
        else if (stricmp(lastToken, "Num*") == 0) vk = VK_MULTIPLY;
        else if (stricmp(lastToken, "Num+") == 0) vk = VK_ADD;
        else if (stricmp(lastToken, "Num-") == 0) vk = VK_SUBTRACT;
        else if (stricmp(lastToken, "Num.") == 0) vk = VK_DECIMAL;
        else if (stricmp(lastToken, "Num/") == 0) vk = VK_DIVIDE;
        // 检查十六进制格式
        else if (strncmp(lastToken, "0x", 2) == 0) {
            vk = (BYTE)strtol(lastToken, NULL, 16);
        }
    }
    
    return MAKEWORD(vk, mod);
}

/**
 * @brief 从配置文件中读取自定义倒计时热键设置
 * @param hotkey 存储热键的指针
 */
void ReadCustomCountdownHotkey(WORD* hotkey) {
    if (!hotkey) return;
    
    *hotkey = 0; // 默认为0（未设置）
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "HOTKEY_CUSTOM_COUNTDOWN=", 24) == 0) {
            char* value = line + 24;
            // 去除末尾的换行符
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // 解析热键字符串
            *hotkey = StringToHotkey(value);
            break;
        }
    }
    
    fclose(file);
}

/**
 * @brief 写入单个配置项到配置文件
 * @param key 配置项键名
 * @param value 配置项值
 * 
 * 在配置文件中添加或更新单个配置项，根据键名自动选择节(section)
 */
void WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 根据键名确定应该放在哪个节
    const char* section;
    
    if (strcmp(key, "CONFIG_VERSION") == 0 ||
        strcmp(key, "LANGUAGE") == 0 ||
        strcmp(key, "SHORTCUT_CHECK_DONE") == 0) {
        section = INI_SECTION_GENERAL;
    }
    else if (strncmp(key, "CLOCK_TEXT_COLOR", 16) == 0 ||
           strncmp(key, "FONT_FILE_NAME", 14) == 0 ||
           strncmp(key, "CLOCK_BASE_FONT_SIZE", 20) == 0 ||
           strncmp(key, "WINDOW_SCALE", 12) == 0 ||
           strncmp(key, "CLOCK_WINDOW_POS_X", 18) == 0 ||
           strncmp(key, "CLOCK_WINDOW_POS_Y", 18) == 0 ||
           strncmp(key, "WINDOW_TOPMOST", 14) == 0) {
        section = INI_SECTION_DISPLAY;
    }
    else if (strncmp(key, "CLOCK_DEFAULT_START_TIME", 24) == 0 ||
           strncmp(key, "CLOCK_USE_24HOUR", 16) == 0 ||
           strncmp(key, "CLOCK_SHOW_SECONDS", 18) == 0 ||
           strncmp(key, "CLOCK_TIME_OPTIONS", 18) == 0 ||
           strncmp(key, "STARTUP_MODE", 12) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_TEXT", 18) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_ACTION", 20) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_FILE", 18) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_WEBSITE", 21) == 0) {
        section = INI_SECTION_TIMER;
    }
    else if (strncmp(key, "POMODORO_", 9) == 0) {
        section = INI_SECTION_POMODORO;
    }
    else if (strncmp(key, "NOTIFICATION_", 13) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_MESSAGE_TEXT", 26) == 0) {
        section = INI_SECTION_NOTIFICATION;
    }
    else if (strncmp(key, "HOTKEY_", 7) == 0) {
        section = INI_SECTION_HOTKEYS;
    }
    else if (strncmp(key, "CLOCK_RECENT_FILE", 17) == 0) {
        section = INI_SECTION_RECENTFILES;
    }
    else if (strncmp(key, "COLOR_OPTIONS", 13) == 0) {
        section = INI_SECTION_COLORS;
    }
    else {
        // 其他设置放在OPTIONS节
        section = INI_SECTION_OPTIONS;
    }
    
    // 写入配置
    WriteIniString(section, key, value, config_path);
}

/**
 * @brief 将当前语言设置写入配置文件
 * 
 * @param language 语言枚举值(APP_LANG_ENUM)
 */
void WriteConfigLanguage(int language) {
    const char* langName;
    
    // 将语言枚举值转换为可读的语言名称
    switch (language) {
        case APP_LANG_CHINESE_SIMP:
            langName = "Chinese_Simplified";
            break;
        case APP_LANG_CHINESE_TRAD:
            langName = "Chinese_Traditional";
            break;
        case APP_LANG_ENGLISH:
            langName = "English";
            break;
        case APP_LANG_SPANISH:
            langName = "Spanish";
            break;
        case APP_LANG_FRENCH:
            langName = "French";
            break;
        case APP_LANG_GERMAN:
            langName = "German";
            break;
        case APP_LANG_RUSSIAN:
            langName = "Russian";
            break;
        case APP_LANG_PORTUGUESE:
            langName = "Portuguese";
            break;
        case APP_LANG_JAPANESE:
            langName = "Japanese";
            break;
        case APP_LANG_KOREAN:
            langName = "Korean";
            break;
        default:
            langName = "English"; // 默认为英语
            break;
    }
    
    WriteConfigKeyValue("LANGUAGE", langName);
}

/**
 * @brief 判断是否已经执行过快捷方式检查
 * 
 * 读取配置文件，判断是否有SHORTCUT_CHECK_DONE=TRUE标记
 * 
 * @return bool true表示已检查过，false表示未检查过
 */
bool IsShortcutCheckDone(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 使用INI读取方式获取设置
    return ReadIniBool(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", FALSE, config_path);
}

/**
 * @brief 设置快捷方式检查状态
 * 
 * 在配置文件中写入SHORTCUT_CHECK_DONE=TRUE/FALSE
 * 
 * @param done 是否已检查完成
 */
void SetShortcutCheckDone(bool done) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 使用INI写入方式设置状态
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", done ? "TRUE" : "FALSE", config_path);
}

/**
 * @brief 从配置文件中读取是否禁用通知设置
 * 
 * 专门读取 NOTIFICATION_DISABLED 配置项并更新相应的全局变量。
 */
void ReadNotificationDisabledConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 使用INI读取方式获取设置
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
}

/**
 * @brief 写入是否禁用通知配置
 * @param disabled 是否禁用通知 (TRUE/FALSE)
 * 
 * 更新配置文件中的是否禁用通知设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationDisabled(BOOL disabled) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    source_file = fopen(config_path, "r");
    temp_file = fopen(temp_path, "w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    
    // 逐行读取并写入
    while (fgets(line, sizeof(line), source_file)) {
        // 移除行尾换行符进行比较
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
            if (len > 0 && line[len-1] == '\r')
                line[--len] = '\0';
        }
        
        if (strncmp(line, "NOTIFICATION_DISABLED=", 22) == 0) {
            fprintf(temp_file, "NOTIFICATION_DISABLED=%s\n", disabled ? "TRUE" : "FALSE");
            found = TRUE;
        } else {
            // 还原行尾换行符，原样写回
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // 如果配置中没找到相应项，则添加
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_DISABLED=%s\n", disabled ? "TRUE" : "FALSE");
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // 替换原文件
    remove(config_path);
    rename(temp_path, config_path);
    
    // 更新全局变量
    NOTIFICATION_DISABLED = disabled;
}
