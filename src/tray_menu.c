/**
 * @file tray_menu.c
 * @brief Implementation of system tray menu functionality
 * 
 * This file implements the system tray menu functionality for the application, including:
 * - Right-click menu and its submenus
 * - Color selection menu
 * - Font settings menu
 * - Timeout action settings
 * - Pomodoro functionality
 * - Preset time management
 * - Multi-language interface support
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "../include/language.h"
#include "../include/tray_menu.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/drag_scale.h"
#include "../include/pomodoro.h"
#include "../resource/resource.h"

/// @name External variable declarations
/// @{
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
extern BOOL CLOCK_WINDOW_TOPMOST;       ///< Whether the window is always on top

// Add Pomodoro related variable declarations
extern int POMODORO_WORK_TIME;      ///< Work time (seconds)
extern int POMODORO_SHORT_BREAK;    ///< Short break time (seconds)
extern int POMODORO_LONG_BREAK;     ///< Long break time (seconds)
extern int POMODORO_LOOP_COUNT;     ///< Loop count

// Pomodoro time array and count variables
#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // Store all Pomodoro times
extern int POMODORO_TIMES_COUNT;              // Actual number of Pomodoro times

// Add to external variable declaration section
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];   ///< URL for timeout open website
extern int current_pomodoro_time_index; // Current Pomodoro time index
extern POMODORO_PHASE current_pomodoro_phase; // Pomodoro phase
/// @}

/// @name External function declarations
/// @{
extern void GetConfigPath(char* path, size_t size);
extern BOOL IsAutoStartEnabled(void);
extern void WriteConfigStartupMode(const char* mode);
extern void ClearColorOptions(void);
extern void AddColorOption(const char* color);
/// @}

/**
 * @brief Timeout action type enumeration
 * 
 * Defines different types of operations that can be executed after timer completion
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,   ///< Display message reminder
    TIMEOUT_ACTION_LOCK = 1,      ///< Lock screen
    TIMEOUT_ACTION_SHUTDOWN = 2,  ///< Shutdown
    TIMEOUT_ACTION_RESTART = 3,   ///< Restart system
    TIMEOUT_ACTION_OPEN_FILE = 4, ///< Open specified file
    TIMEOUT_ACTION_SHOW_TIME = 5, ///< Show current time
    TIMEOUT_ACTION_COUNT_UP = 6,   ///< Switch to count-up mode
    TIMEOUT_ACTION_OPEN_WEBSITE = 7, ///< Open website
    TIMEOUT_ACTION_SLEEP = 8,        ///< Sleep
    TIMEOUT_ACTION_RUN_COMMAND = 9,  ///< Run command
    TIMEOUT_ACTION_HTTP_REQUEST = 10 ///< Send HTTP request
} TimeoutActionType;

extern TimeoutActionType CLOCK_TIMEOUT_ACTION;

/**
 * @brief Read timeout action settings from configuration file
 * 
 * Read the timeout action settings saved in the configuration file and update the global variable CLOCK_TIMEOUT_ACTION
 */
void ReadTimeoutActionFromConfig() {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    FILE *configFile = fopen(configPath, "r");
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

/**
 * @brief Recent file structure
 * 
 * Store information about recently used files, including full path and display name
 */
typedef struct {
    char path[MAX_PATH];  ///< Full file path
    char name[MAX_PATH];  ///< File display name (may be truncated)
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[];
extern int CLOCK_RECENT_FILES_COUNT;

/**
 * @brief Format Pomodoro time to wide string
 * @param seconds Number of seconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 */
static void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    int hours = minutes / 60;
    minutes %= 60;
    
    if (hours > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, secs);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", minutes, secs);
    }
}

/**
 * @brief Truncate long file names
 * 
 * @param fileName Original file name
 * @param truncated Truncated file name buffer
 * @param maxLen Maximum display length (excluding terminator)
 * 
 * If the file name exceeds the specified length, it uses the format "first 12 characters...last 12 characters.extension" for intelligent truncation.
 * This function preserves the file extension to ensure users can identify the file type.
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!fileName || !truncated || maxLen <= 7) return; // At least need to display "x...y"
    
    size_t nameLen = wcslen(fileName);
    if (nameLen <= maxLen) {
        // File name does not exceed the length limit, copy directly
        wcscpy(truncated, fileName);
        return;
    }
    
    // Find the position of the last dot (extension separator)
    const wchar_t* lastDot = wcsrchr(fileName, L'.');
    const wchar_t* fileNameNoExt = fileName;
    const wchar_t* ext = L"";
    size_t nameNoExtLen = nameLen;
    size_t extLen = 0;
    
    if (lastDot && lastDot != fileName) {
        // Has valid extension
        ext = lastDot;  // Extension including dot
        extLen = wcslen(ext);
        nameNoExtLen = lastDot - fileName;  // Length of file name without extension
    }
    
    // If the pure file name length is less than or equal to 27 characters (12+3+12), use the old truncation method
    if (nameNoExtLen <= 27) {
        // Simple truncation of main file name, preserving extension
        wcsncpy(truncated, fileName, maxLen - extLen - 3);
        truncated[maxLen - extLen - 3] = L'\0';
        wcscat(truncated, L"...");
        wcscat(truncated, ext);
        return;
    }
    
    // Use new truncation method: first 12 characters + ... + last 12 characters + extension
    wchar_t buffer[MAX_PATH];
    
    // Copy first 12 characters
    wcsncpy(buffer, fileName, 12);
    buffer[12] = L'\0';
    
    // Add ellipsis
    wcscat(buffer, L"...");
    
    // Copy last 12 characters (excluding extension part)
    wcsncat(buffer, fileName + nameNoExtLen - 12, 12);
    
    // Add extension
    wcscat(buffer, ext);
    
    // Copy result to output buffer
    wcscpy(truncated, buffer);
}

/**
 * @brief Display color and settings menu
 * 
 * @param hwnd Window handle
 * 
 * Create and display the application's main settings menu, including:
 * - Edit mode toggle
 * - Timeout action settings
 * - Preset time management
 * - Startup mode settings
 * - Font selection
 * - Color settings
 * - Language selection
 * - Help and about information
 */
void ShowColorMenu(HWND hwnd) {
    // Read timeout action settings from the configuration file before creating the menu
    ReadTimeoutActionFromConfig();
    
    // Set mouse cursor to default arrow to prevent wait cursor display
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    
    // Add edit mode option
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(L"编辑模式", L"Edit Mode"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Timeout action menu
    HMENU hTimeoutMenu = CreatePopupMenu();
    
    // 1. Show message
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(L"显示消息", L"Show Message"));

    // 2. Show current time
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHOW_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_SHOW_TIME, 
               GetLocalizedString(L"显示当前时间", L"Show Current Time"));

    // 3. Count up
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_COUNT_UP ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_COUNT_UP, 
               GetLocalizedString(L"正计时", L"Count Up"));

    // 4. Lock screen
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_LOCK_SCREEN,
               GetLocalizedString(L"锁定屏幕", L"Lock Screen"));

    // First separator
    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    // 5. Open file (submenu)
    HMENU hFileMenu = CreatePopupMenu();

    // First add recent files list
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        wchar_t wFileName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[i].name, -1, wFileName, MAX_PATH);
        
        // Truncate long file names
        wchar_t truncatedName[MAX_PATH];
        TruncateFileName(wFileName, truncatedName, 25); // Limit to 25 characters
        
        // Check if this is the currently selected file and the current timeout action is "open file"
        BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                             strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
                             strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
        
        // Use menu item check state to indicate selection
        AppendMenuW(hFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : 0), 
                   CLOCK_IDM_RECENT_FILE_1 + i, truncatedName);
    }
               
    // Add separator if there are recent files
    if (CLOCK_RECENT_FILES_COUNT > 0) {
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    }

    // Finally add "Browse..." option
    AppendMenuW(hFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE,
               GetLocalizedString(L"浏览...", L"Browse..."));

    // Add "Open File" as a submenu to the timeout action menu
    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hFileMenu, 
               GetLocalizedString(L"打开文件/软件", L"Open File/Software"));

    // 6. Open website
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_OPEN_WEBSITE,
               GetLocalizedString(L"打开网站", L"Open Website"));

    // Second separator
    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    // Add a non-selectable hint option
    AppendMenuW(hTimeoutMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 
               0,  // Use ID 0 to indicate non-selectable menu item
               GetLocalizedString(L"以下超时动作为一次性", L"Following actions are one-time only"));

    // 7. Shutdown
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHUTDOWN,
               GetLocalizedString(L"关机", L"Shutdown"));

    // 8. Restart
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RESTART,
               GetLocalizedString(L"重启", L"Restart"));

    // 9. Sleep
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SLEEP,
               GetLocalizedString(L"睡眠", L"Sleep"));

    // Third separator and Advanced menu
    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    // Create Advanced submenu
    HMENU hAdvancedMenu = CreatePopupMenu();

    // Add "Run Command" option
    AppendMenuW(hAdvancedMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RUN_COMMAND ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RUN_COMMAND,
               GetLocalizedString(L"运行命令", L"Run Command"));

    // Add "HTTP Request" option
    AppendMenuW(hAdvancedMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_HTTP_REQUEST ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_HTTP_REQUEST,
               GetLocalizedString(L"HTTP 请求", L"HTTP Request"));

    // Add Advanced submenu to timeout menu
    AppendMenuW(hTimeoutMenu, MF_POPUP, (UINT_PTR)hAdvancedMenu,
               GetLocalizedString(L"高级", L"Advanced"));

    // Add timeout action menu to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));

    // Preset management menu
    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"倒计时预设", L"Modify Quick Countdown Options"));
    
    // Startup settings submenu
    HMENU hStartupSettingsMenu = CreatePopupMenu();

    // Read current startup mode
    char currentStartupMode[20] = "COUNTDOWN";
    char configPath[MAX_PATH];  
    GetConfigPath(configPath, MAX_PATH);
    FILE *configFile = fopen(configPath, "r");  
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
    
    // Add startup mode options
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

    // Add auto-start option
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
            (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
            CLOCK_IDC_AUTO_START,
            GetLocalizedString(L"开机自启动", L"Start with Windows"));

    // Add startup settings menu to preset management menu
    AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                GetLocalizedString(L"启动设置", L"Startup Settings"));

    // Add notification settings menu - changed to direct menu item, no longer using submenu
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDM_NOTIFICATION_SETTINGS,
                GetLocalizedString(L"通知设置", L"Notification Settings"));

    // Add preset management menu to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(L"预设管理", L"Preset Management"));
    
    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(L"置顶", L"Always on Top"));

    // Add "Hotkey Settings" option after preset management menu
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(L"热键设置", L"Hotkey Settings"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Font menu
    HMENU hMoreFontsMenu = CreatePopupMenu();
    HMENU hFontSubMenu = CreatePopupMenu();
    
    // First add commonly used fonts to the main menu
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        // These fonts are kept in the main menu
        if (strcmp(fontResources[i].fontName, "Terminess Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "DaddyTimeMono Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Foldit SemiBold Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Jacquarda Bastarda 9 Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Moirai One Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Silkscreen Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pixelify Sans Medium Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pinyon Script Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            
            BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
            wchar_t wDisplayName[100];
            MultiByteToWideChar(CP_UTF8, 0, fontResources[i].fontName, -1, wDisplayName, 100);
            wchar_t* dot = wcsstr(wDisplayName, L".ttf");
            if (dot) *dot = L'\0';
            
            AppendMenuW(hFontSubMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                      fontResources[i].menuId, wDisplayName);
        }
    }

    AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);

    // Add other fonts to the "More" submenu
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        // Exclude fonts already added to the main menu
        if (strcmp(fontResources[i].fontName, "Terminess Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "DaddyTimeMono Nerd Font Propo Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Foldit SemiBold Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Jacquarda Bastarda 9 Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Moirai One Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Silkscreen Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pixelify Sans Medium Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Burned Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Rubik Glitch Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ProFont IIx Nerd Font Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Yesteryear Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "Pinyon Script Essence.ttf") == 0 ||
            strcmp(fontResources[i].fontName, "ZCOOL KuaiLe Essence.ttf") == 0) {
            continue;
        }

        BOOL isCurrentFont = strcmp(FONT_FILE_NAME, fontResources[i].fontName) == 0;
        wchar_t wDisplayNameMore[100];
        MultiByteToWideChar(CP_UTF8, 0, fontResources[i].fontName, -1, wDisplayNameMore, 100);
        wchar_t* dot = wcsstr(wDisplayNameMore, L".ttf");
        if (dot) *dot = L'\0';
        
        AppendMenuW(hMoreFontsMenu, MF_STRING | (isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                  fontResources[i].menuId, wDisplayNameMore);
    }

    // Add "More" submenu to main font menu
    AppendMenuW(hFontSubMenu, MF_POPUP, (UINT_PTR)hMoreFontsMenu, GetLocalizedString(L"更多", L"More"));

    // Color menu
    HMENU hColorSubMenu = CreatePopupMenu();
    // Preset color option menu IDs start from 201 to 201+COLOR_OPTIONS_COUNT-1
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;
        
        MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.fState = strcmp(CLOCK_TEXT_COLOR, hexColor) == 0 ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = 201 + i;  // Preset color menu item IDs start from 201
        mii.dwTypeData = (LPSTR)hexColor;
        
        InsertMenuItem(hColorSubMenu, i, TRUE, &mii);
    }
    AppendMenuW(hColorSubMenu, MF_SEPARATOR, 0, NULL);

    // Custom color options
    HMENU hCustomizeMenu = CreatePopupMenu();
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_VALUE, 
                GetLocalizedString(L"颜色值", L"Color Value"));
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL, 
                GetLocalizedString(L"颜色面板", L"Color Panel"));

    AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu, 
                GetLocalizedString(L"自定义", L"Customize"));

    // Add font and color menus to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(L"字体", L"Font"));
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(L"颜色", L"Color"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // About menu
    HMENU hAboutMenu = CreatePopupMenu();

    // Add "About" menu item here
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_ABOUT, GetLocalizedString(L"关于", L"About"));

    // Add separator
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);

    // Add "Support" option - open sponsorship page
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_SUPPORT, GetLocalizedString(L"支持", L"Support"));
    
    // Add "Feedback" option - open different feedback links based on language
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_FEEDBACK, GetLocalizedString(L"反馈", L"Feedback"));
    
    // Add separator
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    
    // Add "Help" option - open user guide webpage
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_HELP, GetLocalizedString(L"使用指南", L"User Guide"));

    // Add "Check for Updates" option
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, 
               GetLocalizedString(L"检查更新", L"Check for Updates"));

    // Language selection menu
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

    // Add reset option to the end of the help menu
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAboutMenu, MF_STRING, 200,
                GetLocalizedString(L"重置", L"Reset"));

    // Add about menu to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(L"帮助", L"Help"));

    // Only keep exit option
    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(L"退出", L"Exit"));
    
    // Display menu
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // This will allow the menu to close automatically when clicking outside
    DestroyMenu(hMenu);
}

/**
 * @brief Display tray right-click menu
 * 
 * @param hwnd Window handle
 * 
 * Create and display the system tray right-click menu, dynamically adjusting menu items based on current application state. Includes:
 * - Timer control (pause/resume, restart)
 * - Time display settings (24-hour format, show seconds)
 * - Pomodoro clock settings
 * - Count-up and countdown mode switching
 * - Quick time preset options
 */
void ShowContextMenu(HWND hwnd) {
    // Read timeout action settings from configuration file before creating the menu
    ReadTimeoutActionFromConfig();
    
    // Set mouse cursor to default arrow to prevent wait cursor display
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    
    // Timer management menu - added at the top
    HMENU hTimerManageMenu = CreatePopupMenu();
    
    // Set conditions for whether submenu items should be enabled
    // Timer options should be available when:
    // 1. Not in show current time mode
    // 2. And either countdown or count-up is in progress
    // 3. If in countdown mode, the timer hasn't ended yet (countdown elapsed time is less than total time)
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    // Pause/Resume text changes based on current state
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(L"继续", L"Resume") : 
                                    GetLocalizedString(L"暂停", L"Pause");
    
    // Submenu items are disabled based on conditions, but parent menu item remains selectable
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    // Restart option should be available when:
    // 1. Not in show current time mode
    // 2. And either countdown or count-up is in progress (regardless of whether countdown has ended)
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || 
                      (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0)));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(L"重新开始", L"Start Over"));
    
    // Add timer management menu to main menu - parent menu item is always enabled
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerManageMenu,
               GetLocalizedString(L"计时管理", L"Timer Control"));
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Time display menu
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

    // Before Pomodoro menu, first read the latest configuration values
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    FILE *configFile = fopen(configPath, "r");
    POMODORO_TIMES_COUNT = 0;  // Initialize to 0
    
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
                char* options = line + 22;
                char* token;
                int index = 0;
                
                token = strtok(options, ",");
                while (token && index < MAX_POMODORO_TIMES) {
                    POMODORO_TIMES[index++] = atoi(token);
                    token = strtok(NULL, ",");
                }
                
                // Set the actual number of time options
                POMODORO_TIMES_COUNT = index;
                
                // Ensure at least one valid value
                if (index > 0) {
                    POMODORO_WORK_TIME = POMODORO_TIMES[0];
                    if (index > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
                    if (index > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
                }
            }
            else if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
                sscanf(line, "POMODORO_LOOP_COUNT=%d", &POMODORO_LOOP_COUNT);
                // Ensure loop count is at least 1
                if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
            }
        }
        fclose(configFile);
    }

    // Pomodoro menu
    HMENU hPomodoroMenu = CreatePopupMenu();
    
    // Add timeBuffer declaration
    wchar_t timeBuffer[64]; // For storing formatted time string
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(L"开始", L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    // Create menu items for each Pomodoro time
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        FormatPomodoroTime(POMODORO_TIMES[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        // Support both old and new ID systems
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
        // Check if this is the active Pomodoro phase
        BOOL isCurrentPhase = (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                              current_pomodoro_time_index == i &&
                              !CLOCK_SHOW_CURRENT_TIME &&
                              !CLOCK_COUNT_UP &&  // Add check for not being in count-up mode
                              CLOCK_TOTAL_TIME == POMODORO_TIMES[i]);
        
        // Add check mark if it's the current phase
        AppendMenuW(hPomodoroMenu, MF_STRING | (isCurrentPhase ? MF_CHECKED : MF_UNCHECKED), 
                    menuId, timeBuffer);
    }

    // Add loop count option
    wchar_t menuText[64];
    _snwprintf(menuText, sizeof(menuText)/sizeof(wchar_t),
              GetLocalizedString(L"循环次数: %d", L"Loop Count: %d"),
              POMODORO_LOOP_COUNT);
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_LOOP_COUNT, menuText);


    // Add separator
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    // Add combination option
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_COMBINATION,
              GetLocalizedString(L"组合", L"Combination"));
    
    // Add Pomodoro menu to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPomodoroMenu,
                GetLocalizedString(L"番茄时钟", L"Pomodoro"));

    // Count-up menu - changed to direct click to start
    AppendMenuW(hMenu, MF_STRING | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_COUNT_UP_START,
               GetLocalizedString(L"正计时", L"Count Up"));

    // Add "Set Countdown" option below Count-up
    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(L"倒计时", L"Countdown"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Add quick time options
    for (int i = 0; i < time_options_count; i++) {
        wchar_t menu_item[20];
        _snwprintf(menu_item, sizeof(menu_item)/sizeof(wchar_t), L"%d", time_options[i]);
        AppendMenuW(hMenu, MF_STRING, 102 + i, menu_item);
    }

    // Display menu
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // This will allow the menu to close automatically when clicking outside
    DestroyMenu(hMenu);
}