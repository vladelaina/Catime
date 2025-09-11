/**
 * @file config.c
 * @brief Configuration management with INI file I/O, system detection, and language localization
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

/** @brief Pomodoro session time intervals in seconds */
int POMODORO_TIMES[MAX_POMODORO_TIMES] = {1500, 300, 1500, 600};
int POMODORO_TIMES_COUNT = 4;

/** @brief Notification message texts */
char CLOCK_TIMEOUT_MESSAGE_TEXT[100] = "时间到啦！";
char POMODORO_TIMEOUT_MESSAGE_TEXT[100] = "番茄钟时间到！";
char POMODORO_CYCLE_COMPLETE_TEXT[100] = "所有番茄钟循环完成！";

/** @brief Notification display settings */
int NOTIFICATION_TIMEOUT_MS = 3000;
int NOTIFICATION_MAX_OPACITY = 95;
NotificationType NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
BOOL NOTIFICATION_DISABLED = FALSE;

/** @brief Notification sound configuration */
char NOTIFICATION_SOUND_FILE[MAX_PATH] = "";
int NOTIFICATION_SOUND_VOLUME = 100;

/** @brief Font license agreement acceptance status */
BOOL FONT_LICENSE_ACCEPTED = FALSE;

/** @brief Accepted font license version from config */
char FONT_LICENSE_VERSION_ACCEPTED[16] = "";

/** @brief Current time format setting */
TimeFormatType CLOCK_TIME_FORMAT = TIME_FORMAT_DEFAULT;

/** @brief Time format preview variables */
BOOL IS_TIME_FORMAT_PREVIEWING = FALSE;
TimeFormatType PREVIEW_TIME_FORMAT = TIME_FORMAT_DEFAULT;

/** @brief Milliseconds display setting */
BOOL CLOCK_SHOW_MILLISECONDS = FALSE;

/** @brief Milliseconds preview variables */
BOOL IS_MILLISECONDS_PREVIEWING = FALSE;
BOOL PREVIEW_SHOW_MILLISECONDS = FALSE;


/**
 * @brief Read string value from INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param defaultValue Default value if key not found
 * @param returnValue Buffer for returned value
 * @param returnSize Size of return buffer
 * @param filePath Path to INI file
 * @return Number of characters copied to buffer
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath) {

    wchar_t wsection[256], wkey[256], wdefaultValue[1024], wfilePath[MAX_PATH];
    wchar_t wreturnValue[1024];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, defaultValue, -1, wdefaultValue, 1024);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    DWORD result = GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wreturnValue, 1024, wfilePath);
    
    /** Convert result back to UTF-8 */
    WideCharToMultiByte(CP_UTF8, 0, wreturnValue, -1, returnValue, returnSize, NULL, NULL);
    
    return result;
}


/**
 * @brief Write string value to INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param value Value to write
 * @param filePath Path to INI file
 * @return TRUE on success, FALSE on failure
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath) {

    wchar_t wsection[256], wkey[256], wvalue[1024], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, 1024);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Read integer value from INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param defaultValue Default value if key not found
 * @param filePath Path to INI file
 * @return Integer value from INI file
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath) {

    wchar_t wsection[256], wkey[256], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return GetPrivateProfileIntW(wsection, wkey, defaultValue, wfilePath);
}


/**
 * @brief Write integer value to INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param value Integer value to write
 * @param filePath Path to INI file
 * @return TRUE on success, FALSE on failure
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    
    /** Convert to Unicode for Windows API */
    wchar_t wsection[256], wkey[256], wvalue[32], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, valueStr, -1, wvalue, 32);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Write boolean value to INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param value Boolean value to write
 * @param filePath Path to INI file
 * @return TRUE on success, FALSE on failure
 */
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
               const char* filePath) {
    const char* valueStr = value ? "TRUE" : "FALSE";
    
    /** Convert to Unicode for Windows API */
    wchar_t wsection[256], wkey[256], wvalue[8], wfilePath[MAX_PATH];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, valueStr, -1, wvalue, 8);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return WritePrivateProfileStringW(wsection, wkey, wvalue, wfilePath);
}


/**
 * @brief Read boolean value from INI file with Unicode support
 * @param section INI section name
 * @param key INI key name
 * @param defaultValue Default value if key not found
 * @param filePath Path to INI file
 * @return Boolean value from INI file
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath) {
    char value[8];
    const char* defaultStr = defaultValue ? "TRUE" : "FALSE";
    
    /** Convert to Unicode for Windows API */
    wchar_t wsection[256], wkey[256], wdefaultValue[8], wfilePath[MAX_PATH];
    wchar_t wvalue[8];
    
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 256);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 256);
    MultiByteToWideChar(CP_UTF8, 0, defaultStr, -1, wdefaultValue, 8);
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    GetPrivateProfileStringW(wsection, wkey, wdefaultValue, wvalue, 8, wfilePath);
    
    /** Convert result back to UTF-8 and compare */
    WideCharToMultiByte(CP_UTF8, 0, wvalue, -1, value, sizeof(value), NULL, NULL);
    
    return _stricmp(value, "TRUE") == 0;
}


/**
 * @brief Check if file exists with Unicode support
 * @param filePath Path to file to check
 * @return TRUE if file exists, FALSE otherwise
 */
BOOL FileExists(const char* filePath) {
    /** Convert to Unicode for Windows API */
    wchar_t wfilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wfilePath, MAX_PATH);
    
    return GetFileAttributesW(wfilePath) != INVALID_FILE_ATTRIBUTES;
}


/**
 * @brief Get configuration file path with automatic directory creation
 * @param path Buffer to store config file path
 * @param size Size of path buffer
 * Tries LOCALAPPDATA\Catime first, falls back to .\asset\ if failed
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
        
        /** Create Catime directory if needed */
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime", appdata_path) < sizeof(dir_path)) {
        
            wchar_t wdir_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, dir_path, -1, wdir_path, MAX_PATH);
            if (!CreateDirectoryW(wdir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\asset\\config.ini", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        /** Fallback to local asset directory */
        strncpy(path, ".\\asset\\config.ini", size - 1);
        path[size - 1] = '\0';
    }
}


/**
 * @brief Create default configuration file with system language detection
 * @param config_path Path where to create the config file
 * Auto-detects system language and sets appropriate defaults
 */
void CreateDefaultConfig(const char* config_path) {
    /** Detect system language for initial localization */
    LANGID systemLangID = GetUserDefaultUILanguage();
    int defaultLanguage = APP_LANG_ENGLISH;
    const char* langName = "English";
    
    /** Map Windows language IDs to application languages */
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
            typeStr = "CATIME";
            break;
    }
    

    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", "FALSE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", config_path);
    

    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "%LOCALAPPDATA%\\Catime\\resources\\fonts\\Wallpoet Essence.ttf", config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 960, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", -1, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", config_path);
    

    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", config_path);
    

    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    

    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", config_path);
    

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
    

    for (int i = 1; i <= 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    

    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                 "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                 config_path);
}


/**
 * @brief Extract filename from full path with UTF-8 support
 * @param path Full file path
 * @param name Buffer for extracted filename
 * @param nameSize Size of name buffer
 */
void ExtractFileName(const char* path, char* name, size_t nameSize) {
    if (!path || !name || nameSize == 0) return;
    
    /** Convert UTF-8 path to Unicode for processing */
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    
    /** Find last directory separator */
    wchar_t* lastSlash = wcsrchr(wPath, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(wPath, L'/');
    
    wchar_t wName[MAX_PATH] = {0};
    if (lastSlash) {
        wcscpy(wName, lastSlash + 1);
    } else {
        wcscpy(wName, wPath);
    }
    
    /** Convert back to UTF-8 for output */
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, nameSize, NULL, NULL);
}


/**
 * @brief Create resource folder structure in config directory
 * Creates audio, images, animations, themes, plug-in, and fonts folders
 */
void CheckAndCreateResourceFolders() {
    char config_path[MAX_PATH];
    char base_path[MAX_PATH];
    char resource_path[MAX_PATH];
    char *last_slash;
    
    /** Get base directory from config path */
    GetConfigPath(config_path, MAX_PATH);
    
    /** Extract directory portion */
    strncpy(base_path, config_path, MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';
    

    last_slash = strrchr(base_path, '\\');
    if (!last_slash) {
        last_slash = strrchr(base_path, '/');
    }
    
    if (last_slash) {

        *(last_slash + 1) = '\0';
        

        snprintf(resource_path, MAX_PATH, "%sresources", base_path);
    
        wchar_t wresource_path_check[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        DWORD attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create resources folder: %s (Error: %lu)\n", resource_path, GetLastError());
                return;
            }
        }
        

        snprintf(resource_path, MAX_PATH, "%sresources\\audio", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create audio folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        

        snprintf(resource_path, MAX_PATH, "%sresources\\images", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create images folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        

        snprintf(resource_path, MAX_PATH, "%sresources\\animations", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create animations folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        

        snprintf(resource_path, MAX_PATH, "%sresources\\themes", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create themes folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        

        snprintf(resource_path, MAX_PATH, "%sresources\\plug-in", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create plug-in folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
        
        /** Create fonts folder */
        snprintf(resource_path, MAX_PATH, "%sresources\\fonts", base_path);
    
        MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path_check, MAX_PATH);
        attrs = GetFileAttributesW(wresource_path_check);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        
            wchar_t wresource_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, resource_path, -1, wresource_path, MAX_PATH);
            if (!CreateDirectoryW(wresource_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                fprintf(stderr, "Failed to create fonts folder: %s (Error: %lu)\n", resource_path, GetLastError());
            }
        }
    }
}


/**
 * @brief Check if this is the first run of the application
 * @return TRUE if first run, FALSE otherwise
 */
BOOL IsFirstRun(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    if (!FileExists(config_path)) {
        return TRUE;  /** No config file means first run */
    }
    
    char firstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", firstRun, sizeof(firstRun), config_path);
    
    return (strcmp(firstRun, "TRUE") == 0);
}

/**
 * @brief Set first run flag to FALSE
 */
void SetFirstRunCompleted(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", config_path);
}

/**
 * @brief Read configuration from INI file with version checking and validation
 * Performs comprehensive config loading including language detection, validation, and UI updates
 */
void ReadConfig() {
    /** Ensure resource folders exist */
    CheckAndCreateResourceFolders();
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Create default config if missing */
    if (!FileExists(config_path)) {
        CreateDefaultConfig(config_path);
    }
    
    /** Version compatibility check */
    char version[32] = {0};
    BOOL versionMatched = FALSE;
    
    /** Read version from config */
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "", version, sizeof(version), config_path);
    
    /** Compare with current version */
    if (strcmp(version, CATIME_VERSION) == 0) {
        versionMatched = TRUE;
    }
    
    /** Recreate config if version mismatch */
    if (!versionMatched) {
        CreateDefaultConfig(config_path);
    }


    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));
    

    CLOCK_RECENT_FILES_COUNT = 0;
    
    char language[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "LANGUAGE", "English", language, sizeof(language), config_path);
    
    /** Load font license agreement status */
    FONT_LICENSE_ACCEPTED = ReadIniBool(INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", FALSE, config_path);
    
    /** Load font license version acceptance status */
    ReadIniString(INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", 
                  FONT_LICENSE_VERSION_ACCEPTED, sizeof(FONT_LICENSE_VERSION_ACCEPTED), config_path);
    
    /** Load time format setting */
    char timeFormat[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", timeFormat, sizeof(timeFormat), config_path);
    
    if (strcmp(timeFormat, "ZERO_PADDED") == 0) {
        CLOCK_TIME_FORMAT = TIME_FORMAT_ZERO_PADDED;
    } else if (strcmp(timeFormat, "FULL_PADDED") == 0) {
        CLOCK_TIME_FORMAT = TIME_FORMAT_FULL_PADDED;
    } else {
        CLOCK_TIME_FORMAT = TIME_FORMAT_DEFAULT;
    }
    
    /** Load milliseconds display setting */
    CLOCK_SHOW_MILLISECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", FALSE, config_path);
    
    /** Parse language string to enum value */
    int languageSetting = APP_LANG_ENGLISH;
    
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
        /** Handle legacy numeric language format */
        int langValue = atoi(language);
        if (langValue >= 0 && langValue < APP_LANG_COUNT) {
            languageSetting = langValue;
        } else {
            languageSetting = APP_LANG_ENGLISH;
        }
    }
    

    /** Load display configuration */
    ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", "#FFB6C1", CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), config_path);
    CLOCK_BASE_FONT_SIZE = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", 20, config_path);
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", "%LOCALAPPDATA%\\Catime\\resources\\fonts\\Wallpoet Essence.ttf", FONT_FILE_NAME, sizeof(FONT_FILE_NAME), config_path);
    
    /** Process font file name and extract internal name */
    char actualFontFileName[MAX_PATH];
    BOOL isFontsFolderFont = FALSE;
    
    /** Check if FONT_FILE_NAME has path prefix */
    const char* localappdata_prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        /** Extract the relative path from fonts folder (including subfolders) */
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
        isFontsFolderFont = TRUE;
    } else {
        /** Use as-is for embedded fonts */
        strncpy(actualFontFileName, FONT_FILE_NAME, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    /** Set FONT_INTERNAL_NAME based on font type */
    if (isFontsFolderFont) {
        /** For fonts folder fonts, try exact path first, then auto-fix if needed */
        char fontPath[MAX_PATH];
        char* appdata_path = getenv("LOCALAPPDATA");
        BOOL fontFound = FALSE;
        
        if (appdata_path) {
            snprintf(fontPath, MAX_PATH, "%s\\Catime\\resources\\fonts\\%s", appdata_path, actualFontFileName);
            
            /** Check if font exists at configured path */
            if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES) {
                fontFound = TRUE;
            } else {
                /** Font not found at configured path, try to find and auto-fix */
                char* lastSlash = strrchr(actualFontFileName, '\\');
                const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                
                /** Search for font in fonts folder recursively */
                extern BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize);
                if (FindFontInFontsFolder(filenameOnly, fontPath, MAX_PATH)) {
                    /** Font found at different location, update config */
                    char fontsFolderPath[MAX_PATH];
                    snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts\\", appdata_path);
                    
                    if (_strnicmp(fontPath, fontsFolderPath, strlen(fontsFolderPath)) == 0) {
                        /** Calculate new relative path */
                        const char* newRelativePath = fontPath + strlen(fontsFolderPath);
                        
                        /** Update FONT_FILE_NAME with new path */
                        char newConfigPath[MAX_PATH];
                        snprintf(newConfigPath, MAX_PATH, "%%LOCALAPPDATA%%\\Catime\\resources\\fonts\\%s", newRelativePath);
                        strncpy(FONT_FILE_NAME, newConfigPath, sizeof(FONT_FILE_NAME) - 1);
                        FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
                        
                        /** Update actualFontFileName for consistency */
                        strncpy(actualFontFileName, newRelativePath, sizeof(actualFontFileName) - 1);
                        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
                        
                        /** Save corrected path to config file */
                        extern void WriteConfigFont(const char* font_file_name);
                        WriteConfigFont(newRelativePath);
                        
                        fontFound = TRUE;
                    }
                }
            }
            
            if (fontFound) {
                /** Try to get real font name from file */
                if (!GetFontNameFromFile(fontPath, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
                    /** Fallback to filename without extension */
                    char* lastSlash = strrchr(actualFontFileName, '\\');
                    const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                    strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
                    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
                    char* dot = strrchr(FONT_INTERNAL_NAME, '.');
                    if (dot) *dot = '\0';
                }
            } else {
                /** Font not found anywhere, fallback to filename without extension */
                char* lastSlash = strrchr(actualFontFileName, '\\');
                const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
                FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
                char* dot = strrchr(FONT_INTERNAL_NAME, '.');
                if (dot) *dot = '\0';
            }
        } else {
            /** No LOCALAPPDATA, fallback to filename without extension */
            char* lastSlash = strrchr(actualFontFileName, '\\');
            const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
            strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
            FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
            char* dot = strrchr(FONT_INTERNAL_NAME, '.');
            if (dot) *dot = '\0';
        }
    } else {
        /** For embedded fonts, extract internal name by removing file extension */
        strncpy(FONT_INTERNAL_NAME, actualFontFileName, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
        char* dot = strrchr(FONT_INTERNAL_NAME, '.');
        if (dot) *dot = '\0';
    }
    
    CLOCK_WINDOW_POS_X = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", 960, config_path);
    CLOCK_WINDOW_POS_Y = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", -1, config_path);
    
    char scaleStr[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", scaleStr, sizeof(scaleStr), config_path);
    CLOCK_WINDOW_SCALE = atof(scaleStr);
    
    CLOCK_WINDOW_TOPMOST = ReadIniBool(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", TRUE, config_path);
    
    /** Avoid pure black color which can cause rendering issues */
    if (strcasecmp(CLOCK_TEXT_COLOR, "#000000") == 0) {
        strncpy(CLOCK_TEXT_COLOR, "#000001", sizeof(CLOCK_TEXT_COLOR) - 1);
    }
    
    /** Load timer configuration */
    CLOCK_DEFAULT_START_TIME = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    CLOCK_USE_24HOUR = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", FALSE, config_path);
    CLOCK_SHOW_SECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", FALSE, config_path);
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), config_path);
    
    /** Parse timeout action string to enum, filtering dangerous system actions */
    char timeoutAction[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", timeoutAction, sizeof(timeoutAction), config_path);
    
    if (strcmp(timeoutAction, "MESSAGE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "LOCK") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
    } else if (strcmp(timeoutAction, "SHUTDOWN") == 0) {
        /** Security: disable system shutdown */
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(timeoutAction, "RESTART") == 0) {
        /** Security: disable system restart */
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
    }
    

    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CLOCK_TIMEOUT_FILE_PATH, MAX_PATH, config_path);
    
    /** Load and convert website URL from UTF-8 to Unicode */
    char tempWebsiteUrl[MAX_PATH] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", tempWebsiteUrl, MAX_PATH, config_path);
    if (tempWebsiteUrl[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, tempWebsiteUrl, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
    } else {
        CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
    }
    
    /** Override timeout action if valid file path exists */
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        /** Verify file exists before setting action */
        wchar_t wfile_path[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wfile_path, MAX_PATH);
        if (GetFileAttributesW(wfile_path) != INVALID_FILE_ATTRIBUTES) {
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
        }
    }
    
    /** Override timeout action if valid website URL exists */
    if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    }
    

    /** Parse comma-separated time options */
    char timeOptions[256] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", timeOptions, sizeof(timeOptions), config_path);
    
    char *token = strtok(timeOptions, ",");
    while (token && time_options_count < MAX_TIME_OPTIONS) {
        while (*token == ' ') token++;  /** Skip leading whitespace */
        time_options[time_options_count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    /** Load startup mode configuration */
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE), config_path);
    
    /** Load pomodoro configuration */
    char pomodoroTimeOptions[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    
    /** Reset pomodoro times count before parsing */
    POMODORO_TIMES_COUNT = 0;
    
    /** Parse comma-separated pomodoro time intervals */
    token = strtok(pomodoroTimeOptions, ",");
    while (token && POMODORO_TIMES_COUNT < MAX_POMODORO_TIMES) {
        POMODORO_TIMES[POMODORO_TIMES_COUNT++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    /** Map parsed pomodoro times to global variables */
    if (POMODORO_TIMES_COUNT > 0) {
        POMODORO_WORK_TIME = POMODORO_TIMES[0];
        if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
        if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[3];
    }
    
    /** Load and validate pomodoro loop count */
    POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
    

    /** Load notification messages */
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", 
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
                 
    NOTIFICATION_TIMEOUT_MS = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    NOTIFICATION_MAX_OPACITY = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    
    /** Validate notification opacity range */
    if (NOTIFICATION_MAX_OPACITY < 1) NOTIFICATION_MAX_OPACITY = 1;
    if (NOTIFICATION_MAX_OPACITY > 100) NOTIFICATION_MAX_OPACITY = 100;
    
    /** Parse notification type string to enum */
    char notificationType[32] = {0};
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", notificationType, sizeof(notificationType), config_path);
    
    /** Map notification type string to enum value */
    if (strcmp(notificationType, "CATIME") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
    } else if (strcmp(notificationType, "SYSTEM_MODAL") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (strcmp(notificationType, "OS") == 0) {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
    } else {
        NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
    }
    
    /** Load notification sound configuration */
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", 
                NOTIFICATION_SOUND_FILE, MAX_PATH, config_path);
                
    /** Load notification sound volume */
    NOTIFICATION_SOUND_VOLUME = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
                
    /** Load notification disabled flag */
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
    
    /** Validate sound volume range */
    if (NOTIFICATION_SOUND_VOLUME < 0) NOTIFICATION_SOUND_VOLUME = 0;
    if (NOTIFICATION_SOUND_VOLUME > 100) NOTIFICATION_SOUND_VOLUME = 100;
    

    /** Load color options */
    char colorOptions[1024] = {0};
    ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", 
                "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174", 
                colorOptions, sizeof(colorOptions), config_path);
                
    /** Parse comma-separated color options and allocate memory dynamically */
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
    


    /** Load recent files and validate their existence */
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        ReadIniString(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path);
        
        if (strlen(filePath) > 0) {
            /** Convert UTF-8 path to Unicode for file validation */
            wchar_t widePath[MAX_PATH] = {0};
            MultiByteToWideChar(CP_UTF8, 0, filePath, -1, widePath, MAX_PATH);
            
            /** Only add file to recent list if it still exists */
            if (GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, filePath, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';
                /** Extract filename for display purposes */
                ExtractFileName(filePath, CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, MAX_PATH);
                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }
    


    /** Initialize hotkey variables for loading */
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
    
    /** Parse hotkey strings to WORD values */
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
    
    /** Update configuration timestamp for change detection */
    last_config_time = time(NULL);

    /** Apply window position changes immediately if window exists */
    HWND hwnd = FindWindowW(L"CatimeWindow", L"Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }


    SetLanguage((AppLanguage)languageSetting);
}


/**
 * @brief Update timeout action in config file with security filtering
 * @param action Timeout action to set
 * Filters dangerous actions (RESTART/SHUTDOWN/SLEEP) to MESSAGE for security
 */
void WriteConfigTimeoutAction(const char* action) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp = _wfopen(wtemp_path, L"w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL found = FALSE;
    
    /** Filter dangerous system actions for security */
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


void WriteConfigTimeOptions(const char* options) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
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
 * @brief Load recent files list from config with file existence validation
 * Validates each file path and extracts display names
 */
void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE *file = _wfopen(wconfig_path, L"r");
    if (!file) return;

    char line[MAX_PATH];
    CLOCK_RECENT_FILES_COUNT = 0;

    while (fgets(line, sizeof(line), file)) {

        if (strncmp(line, "CLOCK_RECENT_FILE_", 18) == 0) {
            char *path = strchr(line + 18, '=');
            if (path) {
                path++;
                char *newline = strchr(path, '\n');
                if (newline) *newline = '\0';

                if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {

                    wchar_t widePath[MAX_PATH] = {0};
                    MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                    
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

        else if (strncmp(line, "CLOCK_RECENT_FILE=", 18) == 0) {
            char *path = line + 18;
            char *newline = strchr(path, '\n');
            if (newline) *newline = '\0';

            if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {

                wchar_t widePath[MAX_PATH] = {0};
                MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
                
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
 * @brief Add file to recent files list with MRU (Most Recently Used) ordering
 * @param filePath Path to file to add to recent list
 * Moves existing files to top, validates file existence, maintains list size limit
 */
void SaveRecentFile(const char* filePath) {
    /** Validate input and file existence */
    if (!filePath || strlen(filePath) == 0) return;
    
    /** Check if file actually exists */
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wPath, MAX_PATH);
    
    if (GetFileAttributesW(wPath) == INVALID_FILE_ATTRIBUTES) {
        /** Don't add non-existent files */
        return;
    }
    
    /** Search for existing entry */
    int existingIndex = -1;
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        if (strcmp(CLOCK_RECENT_FILES[i].path, filePath) == 0) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex == 0) {
        /** Already at top, nothing to do */
        return;
    }
    
    if (existingIndex > 0) {
        /** Move existing entry to top */
        RecentFile temp = CLOCK_RECENT_FILES[existingIndex];
        
        /** Shift entries down */
        for (int i = existingIndex; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        /** Place at top */
        CLOCK_RECENT_FILES[0] = temp;
    } else {
        /** Add new entry */

        /** Expand list if not at limit */
        if (CLOCK_RECENT_FILES_COUNT < MAX_RECENT_FILES) {
            CLOCK_RECENT_FILES_COUNT++;
        }
        
        /** Shift all entries down */
        for (int i = CLOCK_RECENT_FILES_COUNT - 1; i > 0; i--) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i - 1];
        }
        
        /** Add new entry at top */
        strncpy(CLOCK_RECENT_FILES[0].path, filePath, MAX_PATH - 1);
        CLOCK_RECENT_FILES[0].path[MAX_PATH - 1] = '\0';
        
        /** Extract display name */
        ExtractFileName(filePath, CLOCK_RECENT_FILES[0].name, MAX_PATH);
    }
    

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    WriteConfig(configPath);
}


/**
 * @brief Convert UTF-8 string to ANSI (GB2312) with memory allocation
 * @param utf8Str Input UTF-8 string
 * @return Allocated ANSI string (caller must free) or copy on failure
 * Uses Chinese codepage 936 for proper Chinese character handling
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

    /** Convert to Chinese ANSI codepage 936 */
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
 * @brief Write pomodoro timing configuration to config file
 * @param work Work session duration in seconds
 * @param short_break Short break duration in seconds
 * @param long_break Long break duration in seconds
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    /** Update global pomodoro timing variables immediately */
    POMODORO_WORK_TIME = work;
    POMODORO_SHORT_BREAK = short_break;
    POMODORO_LONG_BREAK = long_break;
    
    /** Update POMODORO_TIMES array with new values */
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
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    /** Update existing pomodoro times entry or copy other lines */
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
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
    
    /** Add pomodoro times if not found in config */
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
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
}


/**
 * @brief Write pomodoro loop count to config file
 * @param loop_count Number of pomodoro cycles to repeat
 */
void WriteConfigPomodoroLoopCount(int loop_count) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    /** Update existing loop count entry or copy other lines */
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
            fprintf(temp_file, "POMODORO_LOOP_COUNT=%d\n", loop_count);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    /** Add loop count if not found in config */
    if (!found) {
        fprintf(temp_file, "POMODORO_LOOP_COUNT=%d\n", loop_count);
    }
    
    fclose(file);
    fclose(temp_file);
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
    
    /** Update global variable immediately */
    POMODORO_LOOP_COUNT = loop_count;
}


/**
 * @brief Write window topmost setting to config file
 * @param topmost String value "TRUE" or "FALSE" for window always on top
 */
void WriteConfigTopmost(const char* topmost) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp = _wfopen(wtemp_path, L"w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[256];
    BOOL found = FALSE;
    
    /** Update existing topmost entry or copy other lines */
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "WINDOW_TOPMOST=", 15) == 0) {
            fprintf(temp, "WINDOW_TOPMOST=%s\n", topmost);
            found = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    /** Add topmost setting if not found in config */
    if (!found) {
        fprintf(temp, "WINDOW_TOPMOST=%s\n", topmost);
    }
    
    fclose(file);
    fclose(temp);
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
}


/**
 * @brief Configure timeout action to open file and update global state
 * @param filePath Path to file to open on timeout
 */
void WriteConfigTimeoutFile(const char* filePath) {
    /** Set timeout action and file path immediately */
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    
    /** Write complete config to persist changes */
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteConfig(config_path);
}


/**
 * @brief Write complete configuration to INI file
 * @param config_path Path to config file to write
 * Writes all settings including language, display, timer, pomodoro, notifications, hotkeys, etc.
 */
void WriteConfig(const char* config_path) {
    /** Map current language enum to string representation */
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
    
    /** Map notification type enum to string representation */
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
            typeStr = "CATIME";
            break;
    }
    
    /** Read current hotkey settings for serialization */
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
    
    char timeOptionsStr[256] = {0};
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", time_options[i]);
        
        if (i > 0) {
            strcat(timeOptionsStr, ",");
        }
        strcat(timeOptionsStr, buffer);
    }
    
    char pomodoroTimesStr[256] = {0};
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", POMODORO_TIMES[i]);
        
        if (i > 0) {
            strcat(pomodoroTimesStr, ",");
        }
        strcat(pomodoroTimesStr, buffer);
    }
    
    char colorOptionsStr[1024] = {0};
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) {
            strcat(colorOptionsStr, ",");
        }
        strcat(colorOptionsStr, COLOR_OPTIONS[i].hexColor);
    }
    
    const char* timeoutActionStr;
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE:
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_LOCK:
            timeoutActionStr = "LOCK";
            break;
        case TIMEOUT_ACTION_SHUTDOWN:
            timeoutActionStr = "MESSAGE";
            break;
        case TIMEOUT_ACTION_RESTART:
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
            timeoutActionStr = "MESSAGE";
            break;
        default:
            timeoutActionStr = "MESSAGE";
    }
    

    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", IsShortcutCheckDone() ? "TRUE" : "FALSE", config_path);
    
    /** Read current FIRST_RUN value to preserve it */
    char currentFirstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", currentFirstRun, sizeof(currentFirstRun), config_path);
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", currentFirstRun, config_path);
    

    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", CLOCK_BASE_FONT_SIZE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONT_FILE_NAME, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);
    
    char scaleStr[16];
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr, config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE", config_path);
    

    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", CLOCK_DEFAULT_START_TIME, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE", config_path);
    
    const char* timeFormatStr;
    switch (CLOCK_TIME_FORMAT) {
        case TIME_FORMAT_ZERO_PADDED:
            timeFormatStr = "ZERO_PADDED";
            break;
        case TIME_FORMAT_FULL_PADDED:
            timeFormatStr = "FULL_PADDED";
            break;
        default:
            timeFormatStr = "DEFAULT";
            break;
    }
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", timeFormatStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", CLOCK_SHOW_MILLISECONDS ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", CLOCK_TIMEOUT_TEXT, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", timeoutActionStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", CLOCK_TIMEOUT_FILE_PATH, config_path);

    char tempWebsiteUrl[MAX_PATH * 3] = {0};
    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, tempWebsiteUrl, sizeof(tempWebsiteUrl), NULL, NULL);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", tempWebsiteUrl, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", timeOptionsStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", CLOCK_STARTUP_MODE, config_path);
    

    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", pomodoroTimesStr, config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", POMODORO_LOOP_COUNT, config_path);
    

    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", CLOCK_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", POMODORO_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", POMODORO_CYCLE_COMPLETE_TEXT, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", NOTIFICATION_TIMEOUT_MS, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", NOTIFICATION_MAX_OPACITY, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", NOTIFICATION_SOUND_FILE, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", NOTIFICATION_SOUND_VOLUME, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", NOTIFICATION_DISABLED ? "TRUE" : "FALSE", config_path);
    

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
    

    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, CLOCK_RECENT_FILES[i].path, config_path);
    }
    

    for (int i = CLOCK_RECENT_FILES_COUNT; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    

    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", colorOptionsStr, config_path);
}


/**
 * @brief Configure timeout action to open website with URL validation
 * @param url Website URL to open on timeout
 */
void WriteConfigTimeoutWebsite(const char* url) {
    /** Only process valid non-empty URLs */
    if (url && url[0] != '\0') {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
        int len = MultiByteToWideChar(CP_UTF8, 0, url, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
        if (len == 0) {
            CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
        }
        
        char config_path[MAX_PATH];
        GetConfigPath(config_path, MAX_PATH);
        
        /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
        if (!file) return;
        
        char temp_path[MAX_PATH];
        strcpy(temp_path, config_path);
        strcat(temp_path, ".tmp");
        
        wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp = _wfopen(wtemp_path, L"w");
        if (!temp) {
            fclose(file);
            return;
        }
        
        char line[MAX_PATH];
        BOOL actionFound = FALSE;
        BOOL urlFound = FALSE;
        
        /** Update existing config entries or copy unchanged lines */
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "CLOCK_TIMEOUT_ACTION=", 21) == 0) {
                fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
                actionFound = TRUE;
            } else if (strncmp(line, "CLOCK_TIMEOUT_WEBSITE=", 22) == 0) {
                fprintf(temp, "CLOCK_TIMEOUT_WEBSITE=%s\n", url);
                urlFound = TRUE;
            } else {
                fputs(line, temp);
            }
        }
        
        /** Add missing config entries */
        if (!actionFound) {
            fprintf(temp, "CLOCK_TIMEOUT_ACTION=OPEN_WEBSITE\n");
        }
        if (!urlFound) {
            fprintf(temp, "CLOCK_TIMEOUT_WEBSITE=%s\n", url);
        }
        
        fclose(file);
        fclose(temp);
        
        /** Replace original config with updated version */
        remove(config_path);
        rename(temp_path, config_path);
    }
}


/**
 * @brief Update startup mode configuration and global state
 * @param mode Startup mode string (COUNTDOWN, COUNT_UP, NO_DISPLAY, etc.)
 */
void WriteConfigStartupMode(const char* mode) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *file, *temp_file;
    char line[256];
    int found = 0;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!file || !temp_file) {
        if (file) fclose(file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    /** Update global startup mode immediately */
    strncpy(CLOCK_STARTUP_MODE, mode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
    
    /** Update existing entry or copy unchanged lines */
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
            fprintf(temp_file, "STARTUP_MODE=%s\n", mode);
            found = 1;
        } else {
            fputs(line, temp_file);
        }
    }
    
    /** Add entry if not found in existing config */
    if (!found) {
        fprintf(temp_file, "STARTUP_MODE=%s\n", mode);
    }
    
    fclose(file);
    fclose(temp_file);
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
}


/**
 * @brief Write custom pomodoro time intervals to config
 * @param times Array of time intervals in seconds
 * @param count Number of time intervals
 */
void WriteConfigPomodoroTimeOptions(int* times, int count) {
    if (!times || count <= 0) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp = _wfopen(wtemp_path, L"w");
    if (!temp) {
        fclose(file);
        return;
    }
    
    char line[MAX_PATH];
    BOOL optionsFound = FALSE;
    
    /** Update existing pomodoro time options or copy other lines */
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            fprintf(temp, "POMODORO_TIME_OPTIONS=");
            for (int i = 0; i < count; i++) {
                fprintf(temp, "%d", times[i]);
                if (i < count - 1) fprintf(temp, ",");
            }
            fprintf(temp, "\n");
            optionsFound = TRUE;
        } else {
            fputs(line, temp);
        }
    }
    
    /** Add pomodoro options if not found in config */
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
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
}


/**
 * @brief Update notification message texts in config and global state
 * @param timeout_msg Timer timeout notification text
 * @param pomodoro_msg Pomodoro timeout notification text
 * @param cycle_complete_msg Pomodoro cycle completion text
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Use standard WriteIniString for consistent encoding handling */
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", timeout_msg, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", pomodoro_msg, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", cycle_complete_msg, config_path);
    
    /** Update global message variables immediately */
    strncpy(CLOCK_TIMEOUT_MESSAGE_TEXT, timeout_msg, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1);
    CLOCK_TIMEOUT_MESSAGE_TEXT[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_TIMEOUT_MESSAGE_TEXT, pomodoro_msg, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1);
    POMODORO_TIMEOUT_MESSAGE_TEXT[sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT) - 1] = '\0';
    
    strncpy(POMODORO_CYCLE_COMPLETE_TEXT, cycle_complete_msg, sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1);
    POMODORO_CYCLE_COMPLETE_TEXT[sizeof(POMODORO_CYCLE_COMPLETE_TEXT) - 1] = '\0';
}


/**
 * @brief Read notification message texts from config using standard Windows API
 * This ensures consistent encoding handling with other configuration items
 */
void ReadNotificationMessagesConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    /** Use standard ReadIniString for consistent encoding handling */
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", "时间到啦！", 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
    
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", "番茄钟时间到！", 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
    
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", "所有番茄钟循环完成！", 
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
}


/**
 * @brief Read notification timeout setting from config file
 * Updates global NOTIFICATION_TIMEOUT_MS variable
 */
void ReadNotificationTimeoutConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    

    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
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

        return;
    }
    

    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {

        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL timeoutFound = FALSE;
    

    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {

        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) {
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') {
                break;
            }
            
            if (ch != '\r') {
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break;
            }
        }
        
        line[pos] = '\0';
        

        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_TIMEOUT_MS=", 24) == 0) {
            int timeout = atoi(line + 24);
            if (timeout > 0) {
                NOTIFICATION_TIMEOUT_MS = timeout;
            }
            timeoutFound = TRUE;
            break;
        }
    }
    
    CloseHandle(hFile);
    
    /** Set default timeout if not found in config */
    if (!timeoutFound) {
        NOTIFICATION_TIMEOUT_MS = 3000;
    }
}


/**
 * @brief Write notification timeout setting to config file
 * @param timeout_ms Notification display timeout in milliseconds
 */
void WriteConfigNotificationTimeout(int timeout_ms) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    source_file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    

    while (fgets(line, sizeof(line), source_file)) {

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
            /** Copy unchanged line */
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    /** Add timeout setting if not found */
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_TIMEOUT_MS=%d\n", timeout_ms);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
    
    /** Update global variable immediately */
    NOTIFICATION_TIMEOUT_MS = timeout_ms;
}


/**
 * @brief Read notification opacity setting from config file
 * Updates global NOTIFICATION_MAX_OPACITY variable with validation
 */
void ReadNotificationOpacityConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    

    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
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

        return;
    }
    

    char bom[3];
    DWORD bytesRead;
    ReadFile(hFile, bom, 3, &bytesRead, NULL);
    
    if (bytesRead != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF) {

        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    }
    
    char line[256];
    BOOL opacityFound = FALSE;
    

    BOOL readingLine = TRUE;
    int pos = 0;
    
    while (readingLine) {

        bytesRead = 0;
        pos = 0;
        memset(line, 0, sizeof(line));
        
        while (TRUE) {
            char ch;
            ReadFile(hFile, &ch, 1, &bytesRead, NULL);
            
            if (bytesRead == 0) {
                readingLine = FALSE;
                break;
            }
            
            if (ch == '\n') {
                break;
            }
            
            if (ch != '\r') {
                line[pos++] = ch;
                if (pos >= sizeof(line) - 1) break;
            }
        }
        
        line[pos] = '\0';
        

        if (pos == 0 && !readingLine) {
            break;
        }
        
        if (strncmp(line, "NOTIFICATION_MAX_OPACITY=", 25) == 0) {
            int opacity = atoi(line + 25);
            /** Validate opacity range before applying */
            if (opacity >= 1 && opacity <= 100) {
                NOTIFICATION_MAX_OPACITY = opacity;
            }
            opacityFound = TRUE;
            break;
        }
    }
    
    CloseHandle(hFile);
    
    /** Set default opacity if not found in config */
    if (!opacityFound) {
        NOTIFICATION_MAX_OPACITY = 95;
    }
}


/**
 * @brief Write notification opacity setting to config file
 * @param opacity Opacity percentage (1-100)
 */
void WriteConfigNotificationOpacity(int opacity) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    source_file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    

    while (fgets(line, sizeof(line), source_file)) {

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
            /** Copy unchanged line */
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    /** Add opacity setting if not found */
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_MAX_OPACITY=%d\n", opacity);
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    /** Replace original with updated config */
    remove(config_path);
    rename(temp_path, config_path);
    
    /** Update global variable immediately */
    NOTIFICATION_MAX_OPACITY = opacity;
}


/**
 * @brief Read notification type setting from config file
 * Updates global NOTIFICATION_TYPE variable with fallback to default
 */
void ReadNotificationTypeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE *file = _wfopen(wconfig_path, L"r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "NOTIFICATION_TYPE=", 18) == 0) {
                char typeStr[32] = {0};
                sscanf(line + 18, "%31s", typeStr);
                
                /** Parse notification type string to enum value */
                if (strcmp(typeStr, "CATIME") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (strcmp(typeStr, "SYSTEM_MODAL") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                } else if (strcmp(typeStr, "OS") == 0) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else {
                    /** Unknown type, fallback to default */
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                }
                break;
            }
        }
        fclose(file);
    }
}


/**
 * @brief Write notification type setting to config file with validation
 * @param type Notification type enum value
 */
void WriteConfigNotificationType(NotificationType type) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Validate notification type range */
    if (type < NOTIFICATION_TYPE_CATIME || type > NOTIFICATION_TYPE_OS) {
        type = NOTIFICATION_TYPE_CATIME;
    }
    
    /** Update global state immediately */
    NOTIFICATION_TYPE = type;
    
    /** Convert enum to string representation */
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
            typeStr = "CATIME";
            break;
    }
    

    char temp_path[MAX_PATH];
    strncpy(temp_path, config_path, MAX_PATH - 5);
    strcat(temp_path, ".tmp");
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE *source = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE *target = _wfopen(wtemp_path, L"w");
    
    if (source && target) {
        char line[256];
        BOOL found = FALSE;
        
        /** Update existing notification type or copy other lines */
        while (fgets(line, sizeof(line), source)) {
            if (strncmp(line, "NOTIFICATION_TYPE=", 18) == 0) {
                fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
                found = TRUE;
            } else {
                fputs(line, target);
            }
        }
        
        /** Add notification type if not found in config */
        if (!found) {
            fprintf(target, "NOTIFICATION_TYPE=%s\n", typeStr);
        }
        
        fclose(source);
        fclose(target);
        
        /** Replace original with updated config */
        remove(config_path);
        rename(temp_path, config_path);
    } else {
        /** Cleanup on file operation failure */
        if (source) fclose(source);
        if (target) fclose(target);
    }
}


/**
 * @brief Get audio resources folder path with automatic directory creation
 * @param path Buffer to store audio folder path
 * @param size Size of path buffer
 * Tries LOCALAPPDATA\Catime\resources\audio first, falls back to .\resources\audio
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
        
        /** Create audio directory if needed */
        char dir_path[MAX_PATH];
        if (snprintf(dir_path, sizeof(dir_path), "%s\\Catime\\resources\\audio", appdata_path) < sizeof(dir_path)) {
        
            wchar_t wdir_path[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, dir_path, -1, wdir_path, MAX_PATH);
            if (!CreateDirectoryW(wdir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                strncpy(path, ".\\resources\\audio", size - 1);
                path[size - 1] = '\0';
            }
        }
    } else {
        /** Fallback to local resources directory */
        strncpy(path, ".\\resources\\audio", size - 1);
        path[size - 1] = '\0';
    }
}


/**
 * @brief Read notification sound file path from config
 * Loads sound file path into global NOTIFICATION_SOUND_FILE variable
 */
void ReadNotificationSoundConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            char* value = line + 23;
            /** Remove newline */
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            
            /** Skip extra equals sign if present */
            if (value[0] == '=') {
                value++;
            }
            
            /** Clear and copy sound file path */
            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
            strncpy(NOTIFICATION_SOUND_FILE, value, MAX_PATH - 1);
            NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
            break;
        }
    }
    
    fclose(file);
}


/**
 * @brief Write notification sound file path to config with path sanitization
 * @param sound_file Path to sound file to save
 * Removes potentially problematic equals signs from path
 */
void WriteConfigNotificationSound(const char* sound_file) {
    if (!sound_file) return;
    
    /** Clean path by removing equals signs */
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
    

    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* source = _wfopen(wconfig_path, L"r");
    if (!source) return;
    
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* dest = _wfopen(wtemp_path, L"w");
    if (!dest) {
        fclose(source);
        return;
    }
    
    char line[1024];
    int found = 0;
    

    while (fgets(line, sizeof(line), source)) {
        if (strncmp(line, "NOTIFICATION_SOUND_FILE=", 23) == 0) {
            fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
            found = 1;
        } else {
            fputs(line, dest);
        }
    }
    

    if (!found) {
        fprintf(dest, "NOTIFICATION_SOUND_FILE=%s\n", clean_path);
    }
    
    fclose(source);
    fclose(dest);
    

    remove(config_path);
    rename(temp_path, config_path);
    

    memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
    strncpy(NOTIFICATION_SOUND_FILE, clean_path, MAX_PATH - 1);
    NOTIFICATION_SOUND_FILE[MAX_PATH - 1] = '\0';
}


void ReadNotificationVolumeConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
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


void WriteConfigNotificationVolume(int volume) {

    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    

    NOTIFICATION_SOUND_VOLUME = volume;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char temp_path[MAX_PATH];
    strcpy(temp_path, config_path);
    strcat(temp_path, ".tmp");
    
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp = _wfopen(wtemp_path, L"w");
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
 * @brief Read all hotkey configurations from config file
 * @param showTimeHotkey Pointer to store show time hotkey
 * @param countUpHotkey Pointer to store count up hotkey
 * @param countdownHotkey Pointer to store countdown hotkey
 * @param quickCountdown1Hotkey Pointer to store quick countdown 1 hotkey
 * @param quickCountdown2Hotkey Pointer to store quick countdown 2 hotkey
 * @param quickCountdown3Hotkey Pointer to store quick countdown 3 hotkey
 * @param pomodoroHotkey Pointer to store pomodoro hotkey
 * @param toggleVisibilityHotkey Pointer to store toggle visibility hotkey
 * @param editModeHotkey Pointer to store edit mode hotkey
 * @param pauseResumeHotkey Pointer to store pause/resume hotkey
 * @param restartTimerHotkey Pointer to store restart timer hotkey
 * Parses hotkey strings and converts them to Windows hotkey codes
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                       WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                       WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                       WORD* pauseResumeHotkey, WORD* restartTimerHotkey)
{
    /** Validate all pointers */
    if (!showTimeHotkey || !countUpHotkey || !countdownHotkey || 
        !quickCountdown1Hotkey || !quickCountdown2Hotkey || !quickCountdown3Hotkey ||
        !pomodoroHotkey || !toggleVisibilityHotkey || !editModeHotkey || 
        !pauseResumeHotkey || !restartTimerHotkey) return;
    
    /** Initialize all hotkeys to empty */
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
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "HOTKEY_SHOW_TIME=", 17) == 0) {
            char* value = line + 17;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *showTimeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNT_UP=", 16) == 0) {
            char* value = line + 16;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *countUpHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_COUNTDOWN=", 17) == 0) {
            char* value = line + 17;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *countdownHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN1=", 24) == 0) {
            char* value = line + 24;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *quickCountdown1Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN2=", 24) == 0) {
            char* value = line + 24;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *quickCountdown2Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_QUICK_COUNTDOWN3=", 24) == 0) {
            char* value = line + 24;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *quickCountdown3Hotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_POMODORO=", 16) == 0) {
            char* value = line + 16;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *pomodoroHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_TOGGLE_VISIBILITY=", 25) == 0) {
            char* value = line + 25;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *toggleVisibilityHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_EDIT_MODE=", 17) == 0) {
            char* value = line + 17;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *editModeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_PAUSE_RESUME=", 20) == 0) {
            char* value = line + 20;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *pauseResumeHotkey = StringToHotkey(value);
        }
        else if (strncmp(line, "HOTKEY_RESTART_TIMER=", 21) == 0) {
            char* value = line + 21;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *restartTimerHotkey = StringToHotkey(value);
        }
    }
    
    fclose(file);
}


void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) {

        wchar_t wconfig_path[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
        
        file = _wfopen(wconfig_path, L"w");
        if (!file) return;
        

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

        WORD customCountdownHotkey = 0;
        ReadCustomCountdownHotkey(&customCountdownHotkey);
        HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
        

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
        fprintf(file, "HOTKEY_CUSTOM_COUNTDOWN=%s\n", customCountdownStr);
        
        fclose(file);
        return;
    }
    

    char temp_path[MAX_PATH];
    sprintf(temp_path, "%s.tmp", config_path);
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    FILE* temp_file = _wfopen(wtemp_path, L"w");
    
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

            fputs(line, temp_file);
        }
    }
    

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
    

    remove(config_path);
    rename(temp_path, config_path);
}


/**
 * @brief Convert hotkey code to human-readable string
 * @param hotkey Windows hotkey code (LOWORD=VK, HIWORD=modifiers)
 * @param buffer Output buffer for string representation
 * @param bufferSize Size of output buffer
 * Formats as "Ctrl+Shift+Alt+Key" or "None" for empty hotkeys
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    /** Handle empty hotkey */
    if (hotkey == 0) {
        strncpy(buffer, "None", bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }
    
    BYTE vk = LOBYTE(hotkey);
    BYTE mod = HIBYTE(hotkey);
    
    buffer[0] = '\0';
    size_t len = 0;
    
    /** Build modifier string */
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
    
    /** Add separator before key name */
    if (len > 0 && len < bufferSize - 1 && vk != 0) {
        buffer[len++] = '+';
        buffer[len] = '\0';
    }
    

    if (vk >= 'A' && vk <= 'Z') {

        char keyName[2] = {vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= '0' && vk <= '9') {

        char keyName[2] = {vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= VK_F1 && vk <= VK_F24) {

        char keyName[4];
        sprintf(keyName, "F%d", vk - VK_F1 + 1);
        strncat(buffer, keyName, bufferSize - len - 1);
    } else {

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
 * @brief Parse human-readable hotkey string to Windows hotkey code
 * @param str Hotkey string like "Ctrl+Shift+A" or "None"
 * @return Windows hotkey code or 0 for invalid/empty
 * Supports modifiers (Ctrl/Shift/Alt), function keys, and special keys
 */
WORD StringToHotkey(const char* str) {
    if (!str || str[0] == '\0' || strcmp(str, "None") == 0) {
        return 0;
    }
    
    /** Handle legacy numeric format */
    if (isdigit(str[0])) {
        return (WORD)atoi(str);
    }
    
    BYTE vk = 0;
    BYTE mod = 0;
    
    /** Create mutable copy for tokenization */
    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /** Parse modifier and key components */
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
            /** Last token is the key name */
            lastToken = token;
        }
        token = strtok(NULL, "+");
    }
    

    if (lastToken) {

        if (strlen(lastToken) == 1) {
            char ch = toupper(lastToken[0]);
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                vk = ch;
            }
        } 

        else if (lastToken[0] == 'F' && isdigit(lastToken[1])) {
            int fNum = atoi(lastToken + 1);
            if (fNum >= 1 && fNum <= 24) {
                vk = VK_F1 + fNum - 1;
            }
        }

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

        else if (strncmp(lastToken, "0x", 2) == 0) {
            vk = (BYTE)strtol(lastToken, NULL, 16);
        }
    }
    
    return MAKEWORD(vk, mod);
}


void ReadCustomCountdownHotkey(WORD* hotkey) {
    if (!hotkey) return;
    
    *hotkey = 0;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert paths to wide character for Unicode support */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* file = _wfopen(wconfig_path, L"r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "HOTKEY_CUSTOM_COUNTDOWN=", 24) == 0) {
            char* value = line + 24;

            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';
            

            *hotkey = StringToHotkey(value);
            break;
        }
    }
    
    fclose(file);
}


/**
 * @brief Write arbitrary key-value pair to appropriate config section
 * @param key Configuration key name
 * @param value Configuration value
 * Auto-determines appropriate INI section based on key prefix
 */
void WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Determine appropriate section based on key prefix */
    const char* section;
    
    if (strcmp(key, "CONFIG_VERSION") == 0 ||
        strcmp(key, "LANGUAGE") == 0 ||
        strcmp(key, "SHORTCUT_CHECK_DONE") == 0 ||
        strcmp(key, "FIRST_RUN") == 0 ||
        strcmp(key, "FONT_LICENSE_ACCEPTED") == 0 ||
        strcmp(key, "FONT_LICENSE_VERSION_ACCEPTED") == 0) {
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
           strncmp(key, "CLOCK_TIME_FORMAT", 17) == 0 ||
           strncmp(key, "CLOCK_SHOW_MILLISECONDS", 23) == 0 ||
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
        /** Default section for unknown keys */
        section = INI_SECTION_OPTIONS;
    }
    
    /** Write to appropriate section */
    WriteIniString(section, key, value, config_path);
}

/**
 * @brief Set font license agreement acceptance status
 * @param accepted TRUE if user accepted the license agreement, FALSE otherwise
 */
void SetFontLicenseAccepted(BOOL accepted) {
    FONT_LICENSE_ACCEPTED = accepted;
    WriteConfigKeyValue("FONT_LICENSE_ACCEPTED", accepted ? "TRUE" : "FALSE");
}

/**
 * @brief Set font license version acceptance status
 * @param version Version string that was accepted
 */
void SetFontLicenseVersionAccepted(const char* version) {
    if (!version) return;
    
    strncpy(FONT_LICENSE_VERSION_ACCEPTED, version, sizeof(FONT_LICENSE_VERSION_ACCEPTED) - 1);
    FONT_LICENSE_VERSION_ACCEPTED[sizeof(FONT_LICENSE_VERSION_ACCEPTED) - 1] = '\0';
    
    WriteConfigKeyValue("FONT_LICENSE_VERSION_ACCEPTED", version);
}

/**
 * @brief Check if font license version needs acceptance
 * @return TRUE if current version needs user acceptance, FALSE otherwise
 */
BOOL NeedsFontLicenseVersionAcceptance(void) {
    /** If license was never accepted, need acceptance */
    if (!FONT_LICENSE_ACCEPTED) {
        return TRUE;
    }
    
    /** If no version was previously accepted, need acceptance */
    if (strlen(FONT_LICENSE_VERSION_ACCEPTED) == 0) {
        return TRUE;
    }
    
    /** If current version differs from accepted version, need acceptance */
    if (strcmp(FONT_LICENSE_VERSION, FONT_LICENSE_VERSION_ACCEPTED) != 0) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Get current font license version
 * @return Current font license version string
 */
const char* GetCurrentFontLicenseVersion(void) {
    return FONT_LICENSE_VERSION;
}


/**
 * @brief Write language setting to config file
 * @param language Language ID from AppLanguage enum
 * Converts language ID to string name and saves to config
 */
void WriteConfigLanguage(int language) {
    const char* langName;
    
    /** Map language ID to string representation */
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
            langName = "English";
            break;
    }
    
    WriteConfigKeyValue("LANGUAGE", langName);
}


/**
 * @brief Check if desktop shortcut verification has been completed
 * @return TRUE if shortcut check was done, FALSE otherwise
 */
bool IsShortcutCheckDone(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Read shortcut check status from general section */
    return ReadIniBool(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", FALSE, config_path);
}

/**
 * @brief Mark desktop shortcut verification as completed
 * @param done TRUE to mark as done, FALSE to reset
 */
void SetShortcutCheckDone(bool done) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Write shortcut check status to general section */
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", done ? "TRUE" : "FALSE", config_path);
}


/**
 * @brief Read notification disabled setting from config
 * Updates global NOTIFICATION_DISABLED variable
 */
void ReadNotificationDisabledConfig(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Read notification disabled status */
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
}


/**
 * @brief Write notification disabled setting to config file
 * @param disabled TRUE to disable notifications, FALSE to enable
 * Updates both config file and global NOTIFICATION_DISABLED variable
 */
void WriteConfigNotificationDisabled(BOOL disabled) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    snprintf(temp_path, MAX_PATH, "%s.tmp", config_path);
    
    FILE *source_file, *temp_file;
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    source_file = _wfopen(wconfig_path, L"r");
    wchar_t wtemp_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, temp_path, -1, wtemp_path, MAX_PATH);
    
    temp_file = _wfopen(wtemp_path, L"w");
    
    if (!source_file || !temp_file) {
        if (source_file) fclose(source_file);
        if (temp_file) fclose(temp_file);
        return;
    }
    
    char line[1024];
    BOOL found = FALSE;
    
    /** Process each line, updating notification disabled setting */
    while (fgets(line, sizeof(line), source_file)) {
        /** Strip trailing newlines */
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
            /** Copy other lines unchanged */
            fprintf(temp_file, "%s\n", line);
        }
    }
    
    /** Add setting if not found */
    if (!found) {
        fprintf(temp_file, "NOTIFICATION_DISABLED=%s\n", disabled ? "TRUE" : "FALSE");
    }
    
    fclose(source_file);
    fclose(temp_file);
    
    /** Replace original file */
    remove(config_path);
    rename(temp_path, config_path);
    
    /** Update global variable */
    NOTIFICATION_DISABLED = disabled;
}

/**
 * @brief Write time format setting to config file
 * @param format Time format type to set
 */
void WriteConfigTimeFormat(TimeFormatType format) {
    CLOCK_TIME_FORMAT = format;
    
    const char* formatStr;
    switch (format) {
        case TIME_FORMAT_ZERO_PADDED:
            formatStr = "ZERO_PADDED";
            break;
        case TIME_FORMAT_FULL_PADDED:
            formatStr = "FULL_PADDED";
            break;
        default:
            formatStr = "DEFAULT";
            break;
    }
    WriteConfigKeyValue("CLOCK_TIME_FORMAT", formatStr);
}

/**
 * @brief Write milliseconds display setting to config file
 * @param showMilliseconds TRUE to show milliseconds, FALSE to hide
 */
void WriteConfigShowMilliseconds(BOOL showMilliseconds) {
    CLOCK_SHOW_MILLISECONDS = showMilliseconds;
    WriteConfigKeyValue("CLOCK_SHOW_MILLISECONDS", showMilliseconds ? "TRUE" : "FALSE");
}

/**
 * @brief Get appropriate timer interval based on milliseconds display setting
 * @return Timer interval in milliseconds (10ms if showing milliseconds, 1000ms otherwise)
 */
UINT GetTimerInterval(void) {
    /** Check if we're in milliseconds preview mode */
    if (IS_MILLISECONDS_PREVIEWING && PREVIEW_SHOW_MILLISECONDS) {
        return 10;  /** Use 10ms for smooth preview while maintaining performance */
    }
    
    /** Use actual setting */
    return CLOCK_SHOW_MILLISECONDS ? 10 : 1000;
}

/**
 * @brief Reset timer with appropriate interval based on milliseconds display setting
 * @param hwnd Window handle
 */
void ResetTimerWithInterval(HWND hwnd) {
    KillTimer(hwnd, 1);
    SetTimer(hwnd, 1, GetTimerInterval(), NULL);
    
    /** Initialize millisecond timing when timer restarts */
    extern void ResetTimerMilliseconds(void);
    ResetTimerMilliseconds();
}

/**
 * @brief Force flush configuration changes to disk immediately
 * Ensures all pending configuration writes are committed to the config file
 */
void FlushConfigToDisk(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Convert to wide character for Windows API */
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    /** Force flush file system buffers to ensure immediate disk write */
    HANDLE hFile = CreateFileW(wconfig_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
}
