/**
 * @file config.c
 * @brief Configuration file management module implementation
 */

#include "../include/config.h"
#include "../include/language.h"
#include "../resource/resource.h"
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

#define MAX_POMODORO_TIMES 10

extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

int POMODORO_TIMES[MAX_POMODORO_TIMES] = {1500, 300, 1500, 600};
int POMODORO_TIMES_COUNT = 4;

char CLOCK_TIMEOUT_MESSAGE_TEXT[100] = "时间到啦！";
char POMODORO_TIMEOUT_MESSAGE_TEXT[100] = "番茄钟时间到！";
char POMODORO_CYCLE_COMPLETE_TEXT[100] = "所有番茄钟循环完成！";

int NOTIFICATION_TIMEOUT_MS = 3000;
int NOTIFICATION_MAX_OPACITY = 95;
NotificationType NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
BOOL NOTIFICATION_DISABLED = FALSE;

char NOTIFICATION_SOUND_FILE[MAX_PATH] = "";
int NOTIFICATION_SOUND_VOLUME = 100;

/** @brief Read string value from INI file */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath) {
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wdefaultValue[1024], wfilePath[MAX_PATH];
    wchar_t wreturnValue[1024];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, defaultValue, -1, wdefaultValue, 1024);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    DWORD result = GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wreturnValue, 1024, wfilePath);
    
    // Convert result back to ANSI
    WideCharToMultiByte(CP_ACP, 0, wreturnValue, -1, returnValue, returnSize, NULL, NULL);
    
    return result;
}

/** @brief Write string value to INI file */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath) {
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wvalue[1024], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, value, -1, wvalue, 1024);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}

/** @brief Read integer value from INI */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath) {
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return GetPrivateProfileIntW(wsection, wkey, defaultValue, wfilePath);
}

/** @brief Write integer value to INI file */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wvalue[32], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, valueStr, -1, wvalue, 32);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}

/** @brief Write boolean value to INI file */
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
               const char* filePath) {
    const char* valueStr = value ? "TRUE" : "FALSE";
    
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wvalue[8], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, valueStr, -1, wvalue, 8);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}

/** @brief Read boolean value from INI */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath) {
    char value[8];
    const char* defaultStr = defaultValue ? "TRUE" : "FALSE";
    
    // Convert ANSI strings to Unicode for the API call
    wchar_t wsection[256], wkey[256], wdefaultValue[8], wfilePath[MAX_PATH];
    wchar_t wvalue[8];
    
    MultiByteToWideChar(CP_ACP, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_ACP, 0, defaultStr, -1, wdefaultValue, 8);
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wvalue, 8, wfilePath);
    
    // Convert result back to ANSI
    WideCharToMultiByte(CP_ACP, 0, wvalue, -1, value, sizeof(value), NULL, NULL);
    
    return _stricmp(value, "TRUE") == 0;
}

/** @brief Check if configuration file exists */
BOOL FileExists(const char* filePath) {
    // Convert ANSI string to Unicode for the API call
    wchar_t wfilePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return GetFileAttributesW(wfilePath) != INVALID_FILE_ATTRIBUTES;
}

/** @brief Get configuration file path */
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
            // Convert ANSI string to Unicode for the API call
            wchar_t wdir_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, dir_path, -1, wdir_path, MAX_PATH);
            if (!CreateDirectoryW(wdir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\asset\\config.ini", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\asset\\config.ini", size - 1);
        path[size - 1] = '\0';
    }
}

/** @brief Create default configuration file */
void CreateDefaultConfig(const char* config_path) {
    // Get system default language ID
    LANGID systemLangID = GetUserDefaultUILanguage();
    int defaultLanguage = APP_LANG_ENGLISH; // Default to English
    const char* langName = "English"; // Default language name
    
    // Set default language based on system language ID
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
    
    // Choose default settings based on notification type
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
            typeStr = "CATIME"; // Default value
            break;
    }
    
    // ======== [General] Section ========
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", config_path);
    
    // ======== [Display] Section ========
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "Wallpoet Essence.ttf", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 960, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", -1, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", config_path);
    
    // ======== [Timer] Section ========
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", config_path);
    
    // ======== [Pomodoro] Section ========
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    
    // ======== [Notification] Section ========
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", config_path);
    
    // ======== [Hotkeys] Section ========
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
    
    // ======== [RecentFiles] Section ========
    for (int i = 1; i <= 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    // ======== [Colors] Section ========
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                 "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                 config_path);
}

/** @brief Extract filename from file path */
void ExtractFileName(const char* path, char* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return;
    
    // First convert to wide characters to properly handle Unicode paths
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    
    // Look for the last backslash or forward slash
    wchar_t* lastSlash = wcsrchr(wPath, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(wPath, L'/');
    
    wchar_t wName[MAX_PATH] = {0};
    if (lastSlash) {
        wcscpy(wName, lastSlash + 1);
    } else {
        wcscpy(wName, wPath);
    }
    
    // Convert back to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, nameSize, NULL, NULL);
}

/** @brief Check and create resource folders */
void CheckAndCreateResourceFolders() {
    char config_path[MAX_PATH];
    char base_path[MAX_PATH];
    char resource_path[MAX_PATH];
    char *last_slash;
    
    // Get configuration file path
    GetConfigPath(config_path, MAX_PATH);
    
    // Copy configuration file path
    strncpy(base_path, config_path, MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';
    
    // Find the last slash or backslash, which marks the beginning of the filename
    last_slash = strrchr(base_path, '\\');
    if (!last_slash) {
        last_slash = strrchr(base_path, '/');
    }
    
    if (last_slash) {
        // Truncate path to directory part
        *(last_slash + 1) = '\0';
        
        // Create resources main directory
        snprintf(resource_path, MAX_PATH, "%sresources", base_path);
        // Convert ANSI string to Unicode for the API call
        wchar_t wresource_path_check[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        DWORD attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create resources folder: %s (Error: %lu)\n", resource_path, GetLastError());
                return;
            }
        }
        
        // Create audio subdirectory
        snprintf(resource_path, MAX_PATH, "%sresources\\audio", base_path);
        // Convert ANSI string to Unicode for the API call
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create audio folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // Create images subdirectory
        snprintf(resource_path, MAX_PATH, "%sresources\\images", base_path);
        // Convert ANSI string to Unicode for the API call
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create images folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // Create animations subdirectory
        snprintf(resource_path, MAX_PATH, "%sresources\\animations", base_path);
        // Convert ANSI string to Unicode for the API call
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create animations folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // Create themes subdirectory
        snprintf(resource_path, MAX_PATH, "%sresources\\themes", base_path);
        // Convert ANSI string to Unicode for the API call
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create themes folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        // Create plug-in subdirectory
        snprintf(resource_path, MAX_PATH, "%sresources\\plug-in", base_path);
        // Convert ANSI string to Unicode for the API call
        MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert ANSI string to Unicode for the API call
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create plug-in folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
    }
}

/** @brief Read and parse configuration file */
void ReadConfig() {
    // Check and create resource folders
    CheckAndCreateResourceFolders();
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Check if configuration file exists, create default configuration if it doesn't
    if (!FileExists(config_path)) {
        CreateDefaultConfig(config_path);
    }
    
    // Check configuration file version
    char version[32] = {0};
    BOOL versionMatched = FALSE;
    
    // Read current version information
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "", version, sizeof(version), config_path);
    
    // Compare if version matches
    if (strcmp(version, CATIME_VERSION) == 0) {
        versionMatched = TRUE;
    }
    
    // If version doesn't match, recreate the configuration file
    if (!versionMatched) {
        CreateDefaultConfig(config_path);
    }

    // Reset time options
    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));
    
    // Reset recent files count
    CLOCK_RECENT_FILES_COUNT = 0;
    
    // Read basic settings
    // ======== [General] Section ========
    char language[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "LANGUAGE", "English", language, sizeof(language), config_path);
    
    // Convert language name to enum value
    int languageSetting = APP_LANG_ENGLISH; // Default to English
    
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
        // Try to parse as number (for backward compatibility)
        int langValue = atoi(language);
        if (langValue >= 0 && langValue < APP_LANG_COUNT) {
            languageSetting = langValue;
        } else {
            languageSetting = APP_LANG_ENGLISH; // Default to English
        }
    }
    
    // ======== [Display] Section ========
    ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), config_path);
    CLOCK_BASE_FONT_SIZE = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "Wallpoet Essence.ttf", FONT_FILE_NAME, sizeof(FONT_FILE_NAME), config_path);
    
    // Extract internal name from font filename
    size_t font_name_len = strlen(FONT_FILE_NAME);
    if (font_name_len > 4 && strcmp(FONT_FILE_NAME + font_name_len - 4, ".ttf") == 0) {
        // Ensure target size is sufficient, avoid depending on source string length
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
    
    // Check and replace pure black color
    if (strcasecmp(CLOCK_TEXT_COLOR, "#000000") == 0) {
        strncpy(CLOCK_TEXT_COLOR, "#000001", sizeof(CLOCK_TEXT_COLOR) - 1);
    }
    
    // ======== [Timer] Section ========
    CLOCK_DEFAULT_START_TIME = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    CLOCK_USE_24HOUR = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", FALSE, config_path);
    CLOCK_SHOW_SECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", FALSE, config_path);
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), config_path);
    
    // Read timeout action
    char timeoutAction[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", timeoutAction, sizeof(timeoutAction), config_path);
    
    if (strcmp(timeoutAction, "MESSAGE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "LOCK") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
    } else if (strcmp(timeoutAction, "SHUTDOWN") == 0) {
        // Even if SHUTDOWN exists in the config file, treat it as a one-time operation, default to MESSAGE
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "RESTART") == 0) {
        // Even if RESTART exists in the config file, treat it as a one-time operation, default to MESSAGE
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "OPEN_FILE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    } else if (strcmp(timeoutAction, "SHOW_TIME") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
    } else if (strcmp(timeoutAction, "COUNT_UP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
    } else if (strcmp(timeoutAction, "OPEN_WEBSITE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    } else if (strcmp(timeoutAction, "SLEEP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SLEEP;
    } else if (strcmp(timeoutAction, "RUN_COMMAND") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RUN_COMMAND;
    } else if (strcmp(timeoutAction, "HTTP_REQUEST") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_HTTP_REQUEST;
    }
    
    // Read timeout file and website settings
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CLOCK_TIMEOUT_FILE_PATH, MAX_PATH, config_path);
    
    // Read website URL as UTF-8 then convert to Unicode
    char tempWebsiteUrl[MAX_PATH] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", tempWebsiteUrl, MAX_PATH, config_path);
    if (tempWebsiteUrl[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, tempWebsiteUrl, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
    } else {
        CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
    }
    
    // If file path is valid, ensure timeout action is set to open file
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        // Convert ANSI string to Unicode for the API call
        wchar_t wfile_path[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wfile_path, MAX_PATH);
        if (GetFileAttributesW(wfile_path) != INVALID_FILE_ATTRIBUTES) {
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
        }
    }
    
    // If URL is valid, ensure timeout action is set to open website
    if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    }
    
    // Read time options
    char timeOptions[256] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", timeOptions, sizeof(timeOptions), config_path);
    
    char *token = strtok(timeOptions, ",");
    while (token && time_options_count < MAX_TIME_OPTIONS) {
        while (*token == ' ') token++;
        time_options[time_options_count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    // Read startup mode
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE), config_path);
    
    // ======== [Pomodoro] Section ========
    char pomodoroTimeOptions[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    
    // Reset pomodoro time count
    POMODORO_TIMES_COUNT = 0;
    
    // Parse all pomodoro time values
    token = strtok(pomodoroTimeOptions, ",");
    while (token && POMODORO_TIMES_COUNT < MAX_POMODORO_TIMES) {
        POMODORO_TIMES[POMODORO_TIMES_COUNT++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    // Even though we now use a new array to store all times,
    // keep these three variables for backward compatibility
    if (POMODORO_TIMES_COUNT > 0) {
        POMODORO_WORK_TIME = POMODORO_TIMES[0];
        if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
        if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[3]; // Note this is the 4th value
    }
    
    // Read pomodoro loop count
    POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
    
    // ======== [Notification] Section ========
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", 
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
                 
    NOTIFICATION_TIMEOUT_MS = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    NOTIFICATION_MAX_OPACITY = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    
    // Ensure opacity is within valid range (1-100)
    if (NOTIFICATION_MAX_OPACITY < 1) NOTIFICATION_MAX_OPACITY = 1;
    if (NOTIFICATION_MAX_OPACITY > 100) NOTIFICATION_MAX_OPACITY = 100;
    
    char notificationType[32] = {0};
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", notificationType, sizeof(notificationType), config_path);
    
    // Set notification type
    if (strcmp(notificationType, "CATIME") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
    } else if (strcmp(notificationType, "SYSTEM_MODAL") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (strcmp(notificationType, "OS") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
    } else {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME; // Default value
    }
    
    // Read notification audio file path
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", 
                NOTIFICATION_SOUND_FILE, MAX_PATH, config_path);
                
    // Read notification audio volume
    NOTIFICATION_SOUND_VOLUME = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
                
    // Read whether to disable notification window
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
    
    // Ensure volume is within valid range (0-100)
    if (NOTIFICATION_SOUND_VOLUME < 0) NOTIFICATION_SOUND_VOLUME = 0;
    if (NOTIFICATION_SOUND_VOLUME > 100) NOTIFICATION_SOUND_VOLUME = 100;
    
    // ======== [Colors] Section ========
    char colorOptions[1024] = {0};
    ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                colorOptions, sizeof(colorOptions), config_path);
                
    // Parse color options
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
    
    // ======== [RecentFiles] Section ========
    // Read recent file records
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        ReadIniString(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path);
        
        if (strlen(filePath) > 0) {
            // Convert to wide characters to properly check if the file exists
            wchar_t widePath[MAX_PATH] = {0};
            MultiByteToWideChar(CP_UTF8, 0, filePath, -1, widePath, MAX_PATH);
            
            // Check if file exists
            if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, filePath, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';

                ExtractFileName(filePath, CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, MAX_PATH);
                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }
    
    // ======== [Hotkeys] Section ========
    // Read hotkey configurations from INI file
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
    
    // Read hotkey settings
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

    // Apply window position
    HWND hwnd = FindWindow(L"CatimeWindow", L"Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    // Apply language settings
    SetLanguage((AppLanguage)languageSetting);
}

/** @brief Write timeout action configuration */
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
    
    // For shutdown or restart actions, don't write them to the config file, write "MESSAGE" instead
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

/** @brief Write time options configuration */
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

/** @brief Load recently used file records */
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

/** @brief Save recently used file record */
void SaveRecentFile(const char* filePath) {
    // Check if the file path is valid
    if (!filePath || strlen(filePath) == 0) return;
    
    // Convert to wide characters to check if the file exists
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wPath, MAX_PATH);
    
    if (GetFileAttributesW(wPath) == INVALID_FILE_ATTRIBUTES) {
        // File doesn't exist, don't add it
        return;
    }
    
    // Check if the file is already in the list
    int existingIndex = -1;
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex == 0) {
        // File is already at the top of the list, no action needed
        return;
    }
    
    if (existingIndex > 0) {
        // File is in the list, but not at the top, need to move it
        RecentFile temp = CLOCK_RECENT_FILES[existingIndex];
        
        // Move elements backward
        for (int i = existingIndex; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        // Put it at the first position
        CLOCK_RECENT_FILES[0] = temp;
    } else {
        // File is not in the list, need to add it
        // First ensure the list doesn't exceed 5 items
        if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
            CLOCK_RECENT_FILES_COUNT++;
        }
        
        // Move elements backward
        for (int i = CLOCK_RECENT_FILES_COUNT - 1; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        // Add new file to the first position
        strncpy(CLOCK_RECENT_FILES[0].path, filePath, MAX_PATH - 1);
        CLOCK_RECENT_FILES[0].path[MAX_PATH - 1] = '\0';
        
        // Extract filename
        ExtractFileName(filePath, CLOCK_RECENT_FILES[0].name, MAX_PATH);
    }
    
    // Update configuration file
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    WriteConfig(configPath);
}

/** @brief Convert UTF8 to ANSI encoding */
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

/** @brief Write pomodoro time settings */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    // Update global variables
    // Maintain backward compatibility, while updating the POMODORO_TIMES array
    POMODORO_WORK_TIME = work;
    POMODORO_SHORT_BREAK = short_break;
    POMODORO_LONG_BREAK = long_break;
    
    // Ensure at least these three time values exist
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
        // Look for POMODORO_TIME_OPTIONS line
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            // Write all pomodoro times
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
    
    // If POMODORO_TIME_OPTIONS was not found, add it
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

/** @brief Write pomodoro loop count configuration */
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
    
    // If the key was not found in the configuration file, add it
    if (!found) {
        fprintf(temp_file, "POMODORO_LOOP_COUNT=%d\n", loop_count);
    }
    
    fclose(file);
    fclose(temp_file);
    
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variable
    POMODORO_LOOP_COUNT = loop_count;
}

/** @brief Write window topmost status configuration */
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

/** @brief Write timeout open file path */
void WriteConfigTimeoutFile(const char* filePath) {
    // First update global variables
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    
    // Use WriteConfig to completely rewrite the configuration file, maintaining structural consistency
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteConfig(config_path);
}

/** @brief Write all configuration settings to file */
void WriteConfig(const char* config_path) {
    // Get the name of the current language
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
    
    // Choose string representation based on notification type
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
            typeStr = "CATIME"; // Default value
            break;
    }
    
    // Read hotkey settings
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
    
    // Convert hotkey values to readable format
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
    
    // Prepare time options string
    char timeOptionsStr[256] = {0};
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", time_options[i]);
        
        if (i > 0) {
            strcat(timeOptionsStr, ",");
        }
        strcat(timeOptionsStr, buffer);
    }
    
    // Prepare pomodoro time options string
    char pomodoroTimesStr[256] = {0};
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", POMODORO_TIMES[i]);
        
        if (i > 0) {
            strcat(pomodoroTimesStr, ",");
        }
        strcat(pomodoroTimesStr, buffer);
    }
    
    // Prepare color options string
    char colorOptionsStr[1024] = {0};
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) {
            strcat(colorOptionsStr, ",");
        }
        strcat(colorOptionsStr, COLOR_OPTIONS[i].hexColor);
    }
    
    // Determine timeout action string
    const char* timeoutActionStr;
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE:
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_LOCK:
            timeoutActionStr = "LOCK";
            break;
        case TIMEOUT_ACTION_SHUTDOWN:
            // Don't save one-time operations, revert to MESSAGE
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_RESTART:
            // Don't save one-time operations, revert to MESSAGE
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_OPEN_FILE:
            timeoutActionStr = "OPEN_FILE";
            break;
        case TIMEOUT_ACTION_SHOW_TIME:
            timeoutActionStr = "SHOW_TIME";
            break;
        case TIMEOUT_ACTION_COUNT_UP:
            timeoutActionStr = "COUNT_UP";
            break;
        case TIMEOUT_ACTION_OPEN_WEBSITE:
            timeoutActionStr = "OPEN_WEBSITE";
            break;
        case TIMEOUT_ACTION_SLEEP:
            // Don't save one-time operations, revert to MESSAGE
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_RUN_COMMAND:
            timeoutActionStr = "RUN_COMMAND";
            break;
        case TIMEOUT_ACTION_HTTP_REQUEST:
            timeoutActionStr = "HTTP_REQUEST";
            break;
        default:
            timeoutActionStr = "MESSAGE";
    }
    
    // ======== [General] Section ========
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", IsShortcutCheckDone() ? "TRUE" : "FALSE", config_path);
    
    // ======== [Display] Section ========
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", CLOCK_BASE_FONT_SIZE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONT_FILE_NAME, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);
    
    char scaleStr[16];
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr, config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE", config_path);
    
    // ======== [Timer] Section ========
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", CLOCK_DEFAULT_START_TIME, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", CLOCK_TIMEOUT_TEXT, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", timeoutActionStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", CLOCK_TIMEOUT_FILE_PATH, config_path);
    // Convert Unicode URL to UTF-8 for writing to config
    char tempWebsiteUrl[MAX_PATH * 3] = {0};
    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, tempWebsiteUrl, sizeof(tempWebsiteUrl), NULL, NULL);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", tempWebsiteUrl, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", timeOptionsStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", CLOCK_STARTUP_MODE, config_path);
    
    // ======== [Pomodoro] Section ========
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", pomodoroTimesStr, config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", POMODORO_LOOP_COUNT, config_path);
    
    // ======== [Notification] Section ========
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", CLOCK_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", POMODORO_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", POMODORO_CYCLE_COMPLETE_TEXT, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", NOTIFICATION_TIMEOUT_MS, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", NOTIFICATION_MAX_OPACITY, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", NOTIFICATION_SOUND_FILE, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", NOTIFICATION_SOUND_VOLUME, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", NOTIFICATION_DISABLED ? "TRUE" : "FALSE", config_path);
    
    // ======== [Hotkeys] Section ========
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
    
    // ======== [RecentFiles] Section ========
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, CLOCK_RECENT_FILES[i].path, config_path);
    }
    
    // Clear unused file records
    for (int i = CLOCK_RECENT_FILES_COUNT; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    // ======== [Colors] Section ========
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", colorOptionsStr, config_path);
}

/** @brief Write timeout open website URL */
void WriteConfigTimeoutWebsite(const char* url) {
    // Only set timeout action to open website if a valid URL is provided
    if (url && url[0] != '\0') {
        // First update global variables
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
        // Convert UTF-8 URL to Unicode
        int len = MultiByteToWideChar(CP_UTF8, 0, url, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
        if (len == 0) {
            CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
        }
        
        // Then update the configuration file
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
        
        // Read original configuration file, update timeout action and URL
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
                fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
                actionFound = TRUE;
            } else if (strncmp(line, "CLOCK_TIMEOUT_WEBSITE=", 22) == 0) {
                fprintf(temp, "CLOCK_TIMEOUT_WEBSITE=%s\n", url);
                urlFound = TRUE;
            } else {
                // Preserve all other configurations
                fputs(line, temp);
            }
        }
        
        // If these items are not in the configuration, add them
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

/** @brief Write startup mode configuration */
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
    
    // Update global variable
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

/** @brief Write pomodoro time options */
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
    
    // Read original configuration file, update pomodoro time options
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            // Write new time options
            fprintf(temp, "POMODORO_TIME_OPTIONS=");
            for (int i = 0; i < count; i++) {
                fprintf(temp, "%d", times[i]);
                if (i < count - 1) fprintf(temp, ",");
            }
            fprintf(temp, "\n");
            optionsFound = TRUE;
        } else {
            // Preserve all other configurations
            fputs(line, temp);
        }
    }
    
    // If this item is not in the configuration, add it
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

/** @brief Write notification message configuration */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    // Use standard C file operations instead of Windows API
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
    
    // Read and write line by line
    while (fgets(line, sizeof(line), source_file)) {
        // Remove trailing newline characters for comparison
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
            // Restore newline and write back as is
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // If corresponding items are not found in the configuration, add them
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
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variables
    strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, timeout_msg, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
    CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, pomodoro_msg, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
    POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_CYCLE_COMPLETE_TEXT, cycle_complete_msg, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
    POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
}

/** @brief Read notification message text from configuration file */
void ReadNotificationMessagesConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    // Convert ANSI string to Unicode for the API call
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    HANDLE hFile = CreateFileW(
        wconfig_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // File cannot be opened, keep current values in memory or default values
        return;
    }

    // Skip UTF-8 BOM marker (if present)
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // Not a BOM, need to rewind file pointer
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[1024];
    BOOL timeoutMsgFound = FALSE;
    BOOL pomodoroTimeoutMsgFound = FALSE;
    BOOL cycleCompleteMsgFound = FALSE;
    
    // Read file content line by line
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // Read byte by byte, build line
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // End of file
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // End of line
                break;
            }
            
            if (ch != '\r') { // Ignore carriage return
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // Prevent buffer overflow
            }
        }
        
        line[pos] = '\0'; // Ensure string termination
        
        // If no content and file has ended, exit loop
        if (pos == 0 && !readingLine) {
            break;
        }
        
        // Process this line
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
        
        // If all messages have been found, can exit loop early
        if (timeoutMsgFound && pomodoroTimeoutMsgFound && cycleCompleteMsgFound) {
            break;
        }
    }
    
    CloseHandle(hFile);
    
    // If corresponding configuration items are not found in the file, ensure variables have default values
    if (!timeoutMsgFound) {
        strcpy(CLOCK_TIMEOUT_MESSAGE_TEXT, "时间到啦！"); // Default value
    }
    if (!pomodoroTimeoutMsgFound) {
        strcpy(POMODORO_TIMEOUT_MESSAGE_TEXT, "番茄钟时间到！"); // Default value
    }
    if (!cycleCompleteMsgFound) {
        strcpy(POMODORO_CYCLE_COMPLETE_TEXT, "所有番茄钟循环完成！"); // Default value
    }
}

/** @brief Read notification display time from configuration file */
void ReadNotificationTimeoutConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Convert ANSI string to Unicode for the API call
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    HANDLE hFile = CreateFileW(
        wconfig_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // File cannot be opened, keep current default value
        return;
    }
    
    // Skip UTF-8 BOM marker (if present)
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // Not a BOM, need to rewind file pointer
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL timeoutFound = FALSE;
    
    // Read file content line by line
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // Read byte by byte, build line
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // End of file
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // End of line
                break;
            }
            
            if (ch != '\r') { // Ignore carriage return
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // Prevent buffer overflow
            }
        }
        
        line[pos] = '\0'; // Ensure string termination
        
        // If no content and file has ended, exit loop
        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            int timeout = atoi(line + 24);
            if (timeout > 0) {
                NOTIFICATION_TIMEOUT_MS = timeout;
            }
            timeoutFound = TRUE;
            break; // Found what we're looking for, can exit the loop
        }
    }
    
    CloseHandle(hFile);
    
    // If not found in configuration, keep default value
    if (!timeoutFound) {
        NOTIFICATION_TIMEOUT_MS = 3000; // Ensure there's a default value
    }
}

/** @brief Write notification display time configuration */
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
    
    // Read file content line by line
    while (fgets(line, sizeof(line), source_file)) {
        // Remove trailing newline characters for comparison
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
            // Restore newline and write back as is
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // If not found in configuration, add new line
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_TIMEOUT_MS=%d\n", timeout_ms);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variable
    NOTIFICATION_TIMEOUT_MS = timeout_ms;
}

/** @brief Read maximum notification opacity from configuration file */
void ReadNotificationOpacityConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Convert ANSI string to Unicode for the API call
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    HANDLE hFile = CreateFileW(
        wconfig_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // File cannot be opened, keep current default value
        return;
    }
    
    // Skip UTF-8 BOM marker (if present)
    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {
        // Not a BOM, need to rewind file pointer
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL opacityFound = FALSE;
    
    // Read file content line by line
    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {
        // Read byte by byte, build line
        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) { // End of file
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') { // End of line
                break;
            }
            
            if (ch != '\r') { // Ignore carriage return
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break; // Prevent buffer overflow
            }
        }
        
        line[pos] = '\0'; // Ensure string termination
        
        // If no content and file has ended, exit loop
        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_MAX_OPACITY=", 25) == 0) {
            int opacity = atoi(line + 25);
            // Ensure opacity is within valid range (1-100)
            if (opacity >= 1 && opacity <= 100) {
                NOTIFICATION_MAX_OPACITY = opacity;
            }
            opacityFound = TRUE;
            break; // Found what we're looking for, can exit the loop
        }
    }
    
    CloseHandle(hFile);
    
    // If not found in configuration, keep default value
    if (!opacityFound) {
        NOTIFICATION_MAX_OPACITY = 95; // Ensure there's a default value
    }
}

/** @brief Write maximum notification opacity configuration */
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
    
    // Read file content line by line
    while (fgets(line, sizeof(line), source_file)) {
        // Remove trailing newline characters for comparison
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
            // Restore newline and write back as is
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // If not found in configuration, add new line
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_MAX_OPACITY=%d\n", opacity);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variable
    NOTIFICATION_MAX_OPACITY = opacity;
}

/** @brief Read notification type setting from configuration file */
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
                
                // Set notification type based on the string
                if (strcmp(typeStr, "CATIME") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (strcmp(typeStr, "SYSTEM_MODAL") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                } else if (strcmp(typeStr, "OS") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else {
                    // Use default value for invalid type
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                }
                break;
            }
        }
        fclose(file);
    }
}

/** @brief Write notification type configuration */
void WriteConfigNotificationType(NotificationType type) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Ensure type value is within valid range
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME; // Default value
    }
    
    // Update global variable
    NOTIFICATION_TYPE = type;
    
    // Convert enum to string
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
            typeStr = "CATIME"; // Default value
            break;
    }
    
    // Create temporary file path
    char temp_path[MAX_PATH];
    strncpy(temp_path, config_path, MAX_PATH - 5);
    strcat(temp_path, ".tmp");
    
    FILE *source = fopen(config_path, "r");
    FILE *target = fopen(temp_path, "w");
    
    if (source && target) {
        char line[256];
        BOOL found = FALSE;
        
        // Copy file content, replace target configuration line
        while (fgets(line, sizeof(line), source)) {
            if (strncmp(line, "NOTIFICATION_TYPE=", 18) == 0) {
                fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
                found = TRUE;
            } else {
                fputs(line, target);
            }
        }
        
        // If configuration item not found, add it to the end of file
        if (!found) {
            fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
        }
        
        fclose(source);
        fclose(target);
        
        // Replace original file
        remove(config_path);
        rename(temp_path, config_path);
    } else {
        // Clean up potentially open files
        if (source) fclose(source);
        if (target) fclose(target);
    }
}

/** @brief Get audio folder path */
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
            // Convert ANSI string to Unicode for the API call
            wchar_t wdir_path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, dir_path, -1, wdir_path, MAX_PATH);
            if (!CreateDirectoryW(wdir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\resources\\audio", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        strncpy(path, ".\\resources\\audio", size - 1);
        path[size - 1] = '\0';
    }
}

/** @brief Read notification audio settings from configuration file */
void ReadNotificationSoundConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) return;
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            char* value = line + 23;  // Correct offset, skip "NOTIFICATION_SOUND_FILE="
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Ensure path doesn't contain equals sign
            if (value[0] == '=') {
                value++; // If first character is equals sign, skip it
            }
            
            // Copy to global variable, ensure cleared
            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
            strncpy(NOTIFICATION_SOUND_FILE, value, MAX_PATH - 1);
            NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
            break;
        }
    }
    
    fclose(file);
}

/** @brief Write notification audio configuration */
void WriteConfigNotificationSound(const char* sound_file) {
    if (!sound_file) return;
    
    // Check if the path contains equals sign, remove if present
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
    
    // Create temporary file path
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
    
    // Copy file content, replace or add notification audio settings
    while (fgets(line, sizeof(line), source)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
            found = 1;
        } else {
            fputs(line, dest);
        }
    }
    
    // If configuration item not found, add to end of file
    if (!found) {
        fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
    }
    
    fclose(source);
    fclose(dest);
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variable
    memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
    strncpy(NOTIFICATION_SOUND_FILE, clean_path, MAX_PATH - 1);
    NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
}

/** @brief Read notification audio volume from configuration file */
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

/** @brief Write notification audio volume configuration */
void WriteConfigNotificationVolume(int volume) {
    // Validate volume range
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // Update global variable
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

/** @brief Read hotkey settings from configuration file */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                       WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                       WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                       WORD* pauseResumeHotkey, WORD* restartTimerHotkey)
{
    // Parameter validation
    if (!showTimeHotkey || !countUpHotkey || !countdownHotkey || 
        !quickCountdown1Hotkey || !quickCountdown2Hotkey || !quickCountdown3Hotkey ||
        !pomodoroHotkey || !toggleVisibilityHotkey || !editModeHotkey || 
        !pauseResumeHotkey || !restartTimerHotkey) return;
    
    // Initialize to 0 (indicates no hotkey set)
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
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *showTimeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNT_UP=", 16) == 0) {
            char* value = line + 16;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *countUpHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNTDOWN=", 17) == 0) {
            char* value = line + 17;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *countdownHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN1=", 24) == 0) {
            char* value = line + 24;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *quickCountdown1Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN2=", 24) == 0) {
            char* value = line + 24;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *quickCountdown2Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN3=", 24) == 0) {
            char* value = line + 24;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *quickCountdown3Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_POMODORO=", 16) == 0) {
            char* value = line + 16;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *pomodoroHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_TOGGLE_VISIBILITY=", 25) == 0) {
            char* value = line + 25;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *toggleVisibilityHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_EDIT_MODE=", 17) == 0) {
            char* value = line + 17;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *editModeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_PAUSE_RESUME=", 20) == 0) {
            char* value = line + 20;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *pauseResumeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_RESTART_TIMER=", 21) == 0) {
            char* value = line + 21;
            // Remove trailing newline
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            // Parse hotkey string
            *restartTimerHotkey = StringToHotkey(value);
        }
    }
    
    fclose(file);
}

/** @brief Write hotkey configuration */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE* file = fopen(config_path, "r");
    if (!file) {
        // If file doesn't exist, create new file
        file = fopen(config_path, "w");
        if (!file) return;
        
        // Convert hotkey values to readable format
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
        char customCountdownStr[64] = {0}; // Add custom countdown hotkey
        
        // Convert each hotkey
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
        // Get custom countdown hotkey value
        WORD customCountdownHotkey = 0;
        ReadCustomCountdownHotkey(&customCountdownHotkey);
        HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
        
        // Write hotkey configuration
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
        fprintf(file, "HOTKEY_CUSTOM_COUNTDOWN=%s\n", customCountdownStr); // Add new hotkey
        
        fclose(file);
        return;
    }
    
    // File exists, read all lines and update hotkey settings
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
    
    // Convert hotkey values to readable format
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
    
    // Convert each hotkey
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
    
    // Process each line
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
            // Keep other lines
            fputs(line, temp_file);
        }
    }
    
    // Add hotkey configuration items not found
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
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
}

/** @brief Convert hotkey value to readable string */
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

/** @brief Convert string to hotkey value */
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
 * @brief Read custom countdown hotkey setting from configuration file
 * @param hotkey Pointer to store the hotkey
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
 * @brief Write a single configuration item to the configuration file
 * @param key Configuration item key name
 * @param value Configuration item value
 * 
 * Adds or updates a single configuration item in the configuration file, automatically selects section based on key name
 */
void WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Determine which section to place in based on the key name
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

/** @brief Write current language setting to configuration file */
void WriteConfigLanguage(int language) {
    const char* langName;
    
    // Convert language enum value to readable language name
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
            langName = "English"; // Default to English
            break;
    }
    
    WriteConfigKeyValue("LANGUAGE", langName);
}

/** @brief Determine if shortcut check has been performed */
bool IsShortcutCheckDone(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Use INI reading method to get settings
    return ReadIniBool(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", FALSE, config_path);
}

/** @brief Set shortcut check status */
void SetShortcutCheckDone(bool done) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 使用INI写入方式设置状态
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", done ? "TRUE" : "FALSE", config_path);
}

/** @brief Read whether to disable notification setting from configuration file */
void ReadNotificationDisabledConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // Use INI reading method to get settings
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
}

/** @brief Write whether to disable notification configuration */
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
    
    // Read and write line by line
    while (fgets(line, sizeof(line), source_file)) {
        // Remove trailing newline characters for comparison
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
            // Restore newline and write back as is
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    // If configuration item not found in the configuration, add it
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_DISABLED=%s\n", disabled ? "TRUE" : "FALSE");
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    // Replace original file
    remove(config_path);
    rename(temp_path, config_path);
    
    // Update global variable
    NOTIFICATION_DISABLED = disabled;
}
