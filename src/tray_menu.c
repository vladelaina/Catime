/**
 * @file tray_menu.c
 * @brief Complex popup menu system for system tray right-click and left-click menus
 * Handles extensive configuration options, font/color selection, and multilingual support
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "../include/log.h"
#include "../include/language.h"
#include "../include/tray_menu.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/drag_scale.h"
#include "../include/pomodoro.h"
#include "../include/timer.h"
#include "../include/config.h"
#include "../resource/resource.h"

/** @brief Timer state and display configuration externals */
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_EDIT_MODE;
extern char CLOCK_STARTUP_MODE[20];
extern char CLOCK_TEXT_COLOR[10];
extern char FONT_FILE_NAME[];
extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;
extern int time_options[];
extern int time_options_count;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];
extern char CLOCK_TIMEOUT_TEXT[50];
extern BOOL CLOCK_WINDOW_TOPMOST;
extern TimeFormatType CLOCK_TIME_FORMAT;
extern BOOL CLOCK_SHOW_MILLISECONDS;

/** @brief Pomodoro technique configuration externals */
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES];
extern int POMODORO_TIMES_COUNT;

extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];
extern int current_pomodoro_time_index;
extern POMODORO_PHASE current_pomodoro_phase;

extern void GetConfigPath(char* path, size_t size);
extern BOOL IsAutoStartEnabled(void);
extern void WriteConfigStartupMode(const char* mode);
extern void ClearColorOptions(void);
extern void AddColorOption(const char* color);

/**
 * @brief Read timeout action setting from configuration file
 * Parses TIMEOUT_ACTION value and updates global timeout action type
 */
void ReadTimeoutActionFromConfig() {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    FILE *configFile = _wfopen(wconfigPath, L"r");
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "TIMEOUT_ACTION=", 15) == 0) {
                int action = 0;
                sscanf(line, "TIMEOUT_ACTION=%d", &action);
                CLOCK_TIMEOUT_ACTION = (TimeoutActionType)action;
                break;
            }
        }
        fclose(configFile);
    }
}

/** @brief Recent file tracking externals (defined in config.h) */

/**
 * @brief Format time duration for menu display with intelligent precision
 * @param seconds Time duration in seconds
 * @param buffer Output buffer for formatted time string
 * @param bufferSize Size of output buffer in wide characters
 * Shows hours if > 1 hour, minutes only if whole minutes, or minutes:seconds
 */
static void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    int hours = minutes / 60;
    minutes %= 60;
    
    if (hours > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, secs);
    } else if (secs == 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d", minutes);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", minutes, secs);
    }
}

/**
 * @brief Intelligently truncate long filenames for menu display
 * @param fileName Original filename to truncate
 * @param truncated Output buffer for truncated filename
 * @param maxLen Maximum length for display
 * Preserves extension and uses middle truncation with ellipsis for readability
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!fileName || !truncated || maxLen <= 7) return;
    
    size_t nameLen = wcslen(fileName);
    if (nameLen <= maxLen) {
        wcscpy(truncated, fileName);
        return;
    }
    
    /** Separate filename and extension for smart truncation */
    const wchar_t* lastDot = wcsrchr(fileName, L'.');
    const wchar_t* fileNameNoExt = fileName;
    const wchar_t* ext = L"";
    size_t nameNoExtLen = nameLen;
    size_t extLen = 0;
    
    if (lastDot && lastDot != fileName) {
        ext = lastDot;
        extLen = wcslen(ext);
        nameNoExtLen = lastDot - fileName;
    }
    
    /** Simple truncation for shorter names */
    if (nameNoExtLen <= 27) {
        wcsncpy(truncated, fileName, maxLen - extLen - 3);
        truncated[maxLen - extLen - 3] = L'\0';
        wcscat(truncated, L"...");
        wcscat(truncated, ext);
        return;
    }
    
    /** Middle truncation: show beginning and end with ellipsis */
    wchar_t buffer[MAX_PATH];
    
    wcsncpy(buffer, fileName, 12);
    buffer[12] = L'\0';
    
    wcscat(buffer, L"...");
    
    wcsncat(buffer, fileName + nameNoExtLen - 12, 12);
    
    wcscat(buffer, ext);
    
    wcscpy(truncated, buffer);
}

/**
 * @brief Build and display comprehensive configuration menu (right-click menu)
 * @param hwnd Main window handle for menu operations
 * Creates complex nested menu system with timeout actions, fonts, colors, and settings
 */
void ShowColorMenu(HWND hwnd) {
    ReadTimeoutActionFromConfig();
    
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hTimeoutMenu = CreatePopupMenu();
    
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(L"显示消息", L"Show Message"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHOW_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_SHOW_TIME, 
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_COUNT_UP ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_COUNT_UP, 
               GetLocalizedString(L"正计时", L"Count Up"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_LOCK_SCREEN,
               GetLocalizedString(L"锁定屏幕", L"Lock Screen"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    HMENU hFileMenu = CreatePopupMenu();

    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        wchar_t wFileName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].name, -1, wFileName, MAX_PATH);
        
        wchar_t truncatedName[MAX_PATH];
        TruncateFileName(wFileName, truncatedName, 25);
        
        BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                             strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
                             strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
        
        AppendMenuW(hFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : 0), 
                   CLOCK_IDM_RECENT_FILE_1 + i, truncatedName);
    }
               
    if (CLOCK_RECENT_FILES_COUNT > 0) {
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    }

    AppendMenuW(hFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE,
               GetLocalizedString(L"浏览...", L"Browse..."));

    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hFileMenu, 
               GetLocalizedString(L"打开文件/软件", L"Open File/Software"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_OPEN_WEBSITE,
               GetLocalizedString(L"打开网站", L"Open Website"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTimeoutMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 
               0,
               GetLocalizedString(L"以下超时动作为一次性", L"Following actions are one-time only"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHUTDOWN,
               GetLocalizedString(L"关机", L"Shutdown"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RESTART,
               GetLocalizedString(L"重启", L"Restart"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SLEEP,
               GetLocalizedString(L"睡眠", L"Sleep"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));

    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"倒计时预设", L"Modify Quick Countdown Options"));
    
    HMENU hStartupSettingsMenu = CreatePopupMenu();

    char currentStartupMode[20] = "COUNTDOWN";
    char configPath[MAX_PATH];  
    GetConfigPath(configPath, MAX_PATH);
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    FILE *configFile = _wfopen(wconfigPath, L"r");  
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                break;
            }
        }
        fclose(configFile);
    }
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNTDOWN") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_SET_COUNTDOWN_TIME,
                GetLocalizedString(L"倒计时", L"Countdown"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNT_UP") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_COUNT_UP,
                GetLocalizedString(L"正计时", L"Stopwatch"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "SHOW_TIME") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_SHOW_TIME,
                GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "NO_DISPLAY") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_NO_DISPLAY,
                GetLocalizedString(L"不显示", L"No Display"));
    
    AppendMenuW(hStartupSettingsMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
            (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
            CLOCK_IDC_AUTO_START,
            GetLocalizedString(L"开机自启动", L"Start with Windows"));

    AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                GetLocalizedString(L"启动设置", L"Startup Settings"));

    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDM_NOTIFICATION_SETTINGS,
                GetLocalizedString(L"通知设置", L"Notification Settings"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(L"预设管理", L"Preset Management"));
    
    /** Add format submenu before topmost option */
    HMENU hFormatMenu = CreatePopupMenu();
    
    AppendMenuW(hFormatMenu, MF_STRING | (CLOCK_TIME_FORMAT == TIME_FORMAT_DEFAULT ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_DEFAULT,
                GetLocalizedString(L"默认格式", L"Default Format"));
    
    AppendMenuW(hFormatMenu, MF_STRING | (CLOCK_TIME_FORMAT == TIME_FORMAT_ZERO_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_ZERO_PADDED,
                GetLocalizedString(L"09:59格式", L"09:59 Format"));
    
    AppendMenuW(hFormatMenu, MF_STRING | (CLOCK_TIME_FORMAT == TIME_FORMAT_FULL_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_FULL_PADDED,
                GetLocalizedString(L"00:09:59格式", L"00:09:59 Format"));
    
    /** Add separator line before milliseconds option */
    AppendMenuW(hFormatMenu, MF_SEPARATOR, 0, NULL);
    
    /** Add milliseconds display option */
    AppendMenuW(hFormatMenu, MF_STRING | (CLOCK_SHOW_MILLISECONDS ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS,
                GetLocalizedString(L"显示毫秒", L"Show Milliseconds"));
    
    AppendMenuW(hTimeOptionsMenu, MF_SEPARATOR, 0, NULL);
    
    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(L"置顶", L"Always on Top"));

    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(L"热键设置", L"Hotkey Settings"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

        HMENU hFontSubMenu = CreatePopupMenu();
    
    /** Helper function to recursively build font submenus */
    int g_advancedFontId = 2000; /** Global counter for font IDs */
    
    /** Helper function to scan for font files using ANSI API as fallback */
    int ScanFontFolderAnsi(const char* folderPath, HMENU parentMenu, int* fontId) {
        WriteLog(LOG_LEVEL_DEBUG, "ScanFontFolderAnsi: Starting ANSI scan of folder '%s'", folderPath);

        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*.*", folderPath);

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath, &findData);
        int folderStatus = 0;
        int fontCount = 0;

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }

                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char* ext = strrchr(findData.cFileName, '.');
                    if (ext && (_stricmp(ext, ".ttf") == 0 || _stricmp(ext, ".otf") == 0)) {
                        WriteLog(LOG_LEVEL_DEBUG, "ScanFontFolderAnsi: Found font file '%s'", findData.cFileName);

                        /** Convert to wide char for menu display */
                        wchar_t wDisplayName[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, findData.cFileName, -1, wDisplayName, MAX_PATH);

                        /** Remove extension */
                        wchar_t* dotPos = wcsrchr(wDisplayName, L'.');
                        if (dotPos) *dotPos = L'\0';

                        AppendMenuW(parentMenu, MF_STRING, (*fontId)++, wDisplayName);
                        folderStatus = 1;
                        fontCount++;
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        } else {
             WriteLog(LOG_LEVEL_WARNING, "ScanFontFolderAnsi: FindFirstFileA failed for path '%s', error: %lu", searchPath, GetLastError());
        }

        WriteLog(LOG_LEVEL_DEBUG, "ScanFontFolderAnsi: Found %d font files, status: %d", fontCount, folderStatus);
        return folderStatus;
    }
    
    /** Recursive function to scan folder and create submenus */
    /** Returns: 0 = no content, 1 = has content but no current font, 2 = contains current font */
    int ScanFontFolder(const char* folderPath, HMENU parentMenu, int* fontId) {
        /** Convert folder path to wide character */
        wchar_t wFolderPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, folderPath, -1, wFolderPath, MAX_PATH);
        
        wchar_t wSearchPath[MAX_PATH];
        swprintf(wSearchPath, MAX_PATH, L"%s\\*", wFolderPath);
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(wSearchPath, &findData);
        int folderStatus = 0; /** 0 = no content, 1 = has content, 2 = contains current font */
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                /** Skip . and .. entries */
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                    continue;
                }
                
                wchar_t wFullItemPath[MAX_PATH];
                swprintf(wFullItemPath, MAX_PATH, L"%s\\%s", wFolderPath, findData.cFileName);
                
                /** Handle regular font files */
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t* ext = wcsrchr(findData.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                        /** Remove extension for display */
                        wchar_t wDisplayName[MAX_PATH];
                        wcsncpy(wDisplayName, findData.cFileName, MAX_PATH - 1);
                        wDisplayName[MAX_PATH - 1] = L'\0';
                        wchar_t* dotPos = wcsrchr(wDisplayName, L'.');
                        if (dotPos) *dotPos = L'\0';
                        
                        /** Check if this is the current font */
                        BOOL isCurrentFont = FALSE;
                        
                        /** Build current font file full path for comparison */
                        char currentFontFullPath[MAX_PATH];
                        char currentFileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, currentFileName, MAX_PATH, NULL, NULL);
                        snprintf(currentFontFullPath, MAX_PATH, "%s\\%s", folderPath, currentFileName);
                        
                        /** Convert FONT_FILE_NAME to actual path for comparison */
                        char actualCurrentFontPath[MAX_PATH];
                        if (strstr(FONT_FILE_NAME, "%LOCALAPPDATA%") == FONT_FILE_NAME) {
                            /** Replace %LOCALAPPDATA% with actual path */
                            const char* afterLocalAppData = FONT_FILE_NAME + strlen("%LOCALAPPDATA%");
                            char* appdata_path = getenv("LOCALAPPDATA");
                            if (appdata_path) {
                                snprintf(actualCurrentFontPath, MAX_PATH, "%s%s", appdata_path, afterLocalAppData);
                            } else {
                                strncpy(actualCurrentFontPath, FONT_FILE_NAME, MAX_PATH - 1);
                                actualCurrentFontPath[MAX_PATH - 1] = '\0';
                            }
                        } else {
                            strncpy(actualCurrentFontPath, FONT_FILE_NAME, MAX_PATH - 1);
                            actualCurrentFontPath[MAX_PATH - 1] = '\0';
                        }
                        
                        /** Compare full paths (case insensitive) */
                        isCurrentFont = (_stricmp(currentFontFullPath, actualCurrentFontPath) == 0);
                        
                        AppendMenuW(parentMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                                  (*fontId)++, wDisplayName);
                        
                        /** Update folder status */
                        if (isCurrentFont) {
                            folderStatus = 2; /** This folder contains the current font */
                        } else if (folderStatus == 0) {
                            folderStatus = 1; /** This folder has content but not the current font */
                        }
                    }
                }
                /** Handle subdirectories recursively */
                else if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    /** Create submenu for this folder */
                    HMENU hSubFolderMenu = CreatePopupMenu();
                    
                    /** Convert wide path back to UTF-8 for recursive call */
                    char fullItemPathUtf8[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0, wFullItemPath, -1, fullItemPathUtf8, MAX_PATH, NULL, NULL);
                    
                    /** Recursively scan this subdirectory */
                    int subFolderStatus = ScanFontFolder(fullItemPathUtf8, hSubFolderMenu, fontId);
                    
                    /** Use wide character folder name directly (no conversion needed) */
                    wchar_t wFolderName[MAX_PATH];
                    wcsncpy(wFolderName, findData.cFileName, MAX_PATH - 1);
                    wFolderName[MAX_PATH - 1] = L'\0';
                    
                    /** Always add the submenu, even if empty */
                    if (subFolderStatus == 0) {
                        /** Add "Empty folder" indicator */
                        AppendMenuW(hSubFolderMenu, MF_STRING | MF_GRAYED, 0, L"(Empty folder)");
                        AppendMenuW(parentMenu, MF_POPUP, (UINT_PTR)hSubFolderMenu, wFolderName);
                    } else {
                        /** Add folder with check mark if it contains current font */
                        UINT folderFlags = MF_POPUP;
                        if (subFolderStatus == 2) {
                            folderFlags |= MF_CHECKED; /** Folder contains current font */
                        }
                        AppendMenuW(parentMenu, folderFlags, (UINT_PTR)hSubFolderMenu, wFolderName);
                    }
                    
                    /** Update current folder status based on subfolder status */
                    if (subFolderStatus == 2) {
                        folderStatus = 2; /** This folder contains the current font (in subfolder) */
                    } else if (subFolderStatus == 1 && folderStatus == 0) {
                        folderStatus = 1; /** This folder has content but not the current font */
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        return folderStatus;
    }
    
    /** Load fonts from user's fonts folder directly into main font menu */
    extern BOOL NeedsFontLicenseVersionAcceptance(void);
    
    if (NeedsFontLicenseVersionAcceptance()) {
        /** Show license agreement option if version needs acceptance */
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(L"点击同意许可协议后继续", L"Click to agree to license agreement"));
    } else {
        /** Normal font menu when license version is accepted */
        char fontsFolderPath[MAX_PATH];
        char* appdata_path = getenv("LOCALAPPDATA");
        if (appdata_path) {
            snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
            
            g_advancedFontId = 2000; /** Reset global font ID counter */
            
            /** Use recursive function to scan all folders and subfolders directly in main font menu */
            /** Try Unicode scan first, fallback to ANSI if needed */
            int fontFolderStatus = ScanFontFolder(fontsFolderPath, hFontSubMenu, &g_advancedFontId);

            /** If Unicode scan failed, try ANSI scan as fallback */
            if (fontFolderStatus == 0) {
                WriteLog(LOG_LEVEL_INFO, "Unicode scan found no fonts, trying ANSI scan as fallback...");
                fontFolderStatus = ScanFontFolderAnsi(fontsFolderPath, hFontSubMenu, &g_advancedFontId);
            }

            /** Additional debug: manually check some known font files */
            if (fontFolderStatus == 0) {
                WriteLog(LOG_LEVEL_INFO, "Both scans failed, manually checking known font files...");
                char testFontPath[MAX_PATH];
                snprintf(testFontPath, MAX_PATH, "%s\\Wallpoet Essence.ttf", fontsFolderPath);
                DWORD attribs = GetFileAttributesA(testFontPath);
                if (attribs != INVALID_FILE_ATTRIBUTES) {
                    WriteLog(LOG_LEVEL_WARNING, "Manual check: Wallpoet Essence.ttf EXISTS but scan failed to find it!");
                } else {
                    WriteLog(LOG_LEVEL_INFO, "Manual check: Wallpoet Essence.ttf does not exist");
                }
            }
            WriteLog(LOG_LEVEL_INFO, "Font folder scan result: %d (0=no content, 1=has content, 2=contains current font)", fontFolderStatus);
            
            /** Add browse option if no fonts found or as additional option */
            if (fontFolderStatus == 0) {
                AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0, 
                           GetLocalizedString(L"未找到字体文件", L"No font files found"));
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            } else {
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
            
            AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED, 
                       GetLocalizedString(L"打开字体文件夹", L"Open fonts folder"));
        }
    }

    HMENU hColorSubMenu = CreatePopupMenu();

    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        
        wchar_t hexColorW[16];
        MultiByteToWideChar(CP_UTF8, 0, hexColor, -1, hexColorW, 16);
        
        MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.fState = strcmp(CLOCK_TEXT_COLOR, hexColor) == 0 ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = 201 + i;
        mii.dwTypeData = hexColorW;
        
        InsertMenuItem(hColorSubMenu, i, TRUE, &mii);
    }
    AppendMenuW(hColorSubMenu, MF_SEPARATOR, 0, NULL);

    HMENU hCustomizeMenu = CreatePopupMenu();
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_VALUE, 
                GetLocalizedString(L"颜色值", L"Color Value"));
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL, 
                GetLocalizedString(L"颜色面板", L"Color Panel"));

    AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu, 
                GetLocalizedString(L"自定义", L"Customize"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFormatMenu,
                GetLocalizedString(L"格式", L"Format"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(L"字体", L"Font"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(L"颜色", L"Color"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hAboutMenu = CreatePopupMenu();

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_ABOUT, GetLocalizedString(L"关于", L"About"));

    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_SUPPORT, GetLocalizedString(L"支持", L"Support"));
    
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_FEEDBACK, GetLocalizedString(L"反馈", L"Feedback"));
    
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_HELP, GetLocalizedString(L"使用指南", L"User Guide"));

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, 
               GetLocalizedString(L"检查更新", L"Check for Updates"));

    HMENU hLangMenu = CreatePopupMenu();
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_CHINESE, L"简体中文");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_CHINESE_TRAD, L"繁體中文");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_ENGLISH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_ENGLISH, L"English");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_SPANISH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_SPANISH, L"Español");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_FRENCH ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_FRENCH, L"Français");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_GERMAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_GERMAN, L"Deutsch");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_RUSSIAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_RUSSIAN, L"Русский");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_PORTUGUESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_PORTUGUESE, L"Português");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_JAPANESE ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_JAPANESE, L"日本語");
    AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == APP_LANG_KOREAN ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_LANG_KOREAN, L"한국어");

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hLangMenu, GetLocalizedString(L"语言", L"Language"));

    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAboutMenu, MF_STRING, 200,
                GetLocalizedString(L"重置", L"Reset"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(L"帮助", L"Help"));

    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(L"退出", L"Exit"));
    
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

/**
 * @brief Build and display timer control context menu (left-click menu)
 * @param hwnd Main window handle for menu operations
 * Creates focused menu for timer operations, time display, and Pomodoro functions
 */
void ShowContextMenu(HWND hwnd) {
    ReadTimeoutActionFromConfig();
    
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    HMENU hMenu = CreatePopupMenu();
    
    /** Timer management submenu with dynamic state-based controls */
    HMENU hTimerManageMenu = CreatePopupMenu();
    
    /** Check if timer is actively running (not system clock, and either counting up or countdown in progress) */
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    /** Dynamic pause/resume text based on current timer state */
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(L"继续", L"Resume") : 
                                    GetLocalizedString(L"暂停", L"Pause");
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    /** Check if restart operation is valid for current timer mode */
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || 
                      (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0)));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(L"重新开始", L"Start Over"));
    
    /** Dynamic text based on window visibility */
    const wchar_t* visibilityText = IsWindowVisible(hwnd) ?
        GetLocalizedString(L"隐藏窗口", L"Hide Window") :
        GetLocalizedString(L"显示窗口", L"Show Window");
    
    AppendMenuW(hTimerManageMenu, MF_STRING, CLOCK_IDC_TOGGLE_VISIBILITY, visibilityText);
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerManageMenu,
               GetLocalizedString(L"计时管理", L"Timer Control"));
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    HMENU hTimeMenu = CreatePopupMenu();
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_CURRENT_TIME,
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT,
               GetLocalizedString(L"24小时制", L"24-Hour Format"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS,
               GetLocalizedString(L"显示秒数", L"Show Seconds"));
    
    AppendMenuW(hMenu, MF_POPUP,
               (UINT_PTR)hTimeMenu,
               GetLocalizedString(L"时间显示", L"Time Display"));

    /** Load Pomodoro configuration from file for dynamic menu generation */
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    FILE *configFile = _wfopen(wconfigPath, L"r");
    POMODORO_TIMES_COUNT = 0;
    
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            /** Parse comma-separated Pomodoro time options */
            if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
                char* options = line + 22;
                char* token;
                int index = 0;
                
                token = strtok(options, ",");
                while (token && index < MAX_POMODORO_TIMES) {
                    POMODORO_TIMES[index++] = atoi(token);
                    token = strtok(NULL, ",");
                }
                
                POMODORO_TIMES_COUNT = index;
                
                /** Update default Pomodoro intervals from parsed options */
                if (index > 0) {
                    POMODORO_WORK_TIME = POMODORO_TIMES[0];
                    if (index > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
                    if (index > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
                }
            }
            else if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
                sscanf(line, "POMODORO_LOOP_COUNT=%d", &POMODORO_LOOP_COUNT);
                if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
            }
        }
        fclose(configFile);
    }

    HMENU hPomodoroMenu = CreatePopupMenu();
    
    wchar_t timeBuffer[64];
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(L"开始", L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    /** Generate dynamic Pomodoro time options with current phase indication */
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        FormatPomodoroTime(POMODORO_TIMES[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        /** Map indices to specific menu IDs for standard Pomodoro phases */
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
        /** Check if this time option represents the currently active Pomodoro phase */
        BOOL isCurrentPhase = (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                              current_pomodoro_time_index == i &&
                              !CLOCK_SHOW_CURRENT_TIME &&
                              !CLOCK_COUNT_UP &&
                              CLOCK_TOTAL_TIME == POMODORO_TIMES[i]);
        
        AppendMenuW(hPomodoroMenu, MF_STRING | (isCurrentPhase ? MF_CHECKED : MF_UNCHECKED), 
                    menuId, timeBuffer);
    }

    wchar_t menuText[64];
    _snwprintf(menuText, sizeof(menuText)/sizeof(wchar_t),
              GetLocalizedString(L"循环次数: %d", L"Loop Count: %d"),
              POMODORO_LOOP_COUNT);
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_LOOP_COUNT, menuText);

    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_COMBINATION,
              GetLocalizedString(L"组合", L"Combination"));
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPomodoroMenu,
                GetLocalizedString(L"番茄时钟", L"Pomodoro"));

    AppendMenuW(hMenu, MF_STRING | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_COUNT_UP_START,
               GetLocalizedString(L"正计时", L"Count Up"));

    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(L"倒计时", L"Countdown"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < time_options_count; i++) {
        wchar_t menu_item[20];
        FormatPomodoroTime(time_options[i], menu_item, sizeof(menu_item)/sizeof(wchar_t));
        AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_QUICK_TIME_BASE + i, menu_item);
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}