

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
#include "../include/timer.h"
#include "../resource/resource.h"


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


typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[];
extern int CLOCK_RECENT_FILES_COUNT;


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


void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen) {
    if (!fileName || !truncated || maxLen <= 7) return;
    
    size_t nameLen = wcslen(fileName);
    if (nameLen <= maxLen) {

        wcscpy(truncated, fileName);
        return;
    }
    

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
    

    if (nameNoExtLen <= 27) {

        wcsncpy(truncated, fileName, maxLen - extLen - 3);
        truncated[maxLen - extLen - 3] = L'\0';
        wcscat(truncated, L"...");
        wcscat(truncated, ext);
        return;
    }
    

    wchar_t buffer[MAX_PATH];
    

    wcsncpy(buffer, fileName, 12);
    buffer[12] = L'\0';
    

    wcscat(buffer, L"...");
    

    wcsncat(buffer, fileName + nameNoExtLen - 12, 12);
    

    wcscat(buffer, ext);
    

    wcscpy(truncated, buffer);
}


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


    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);


    HMENU hAdvancedMenu = CreatePopupMenu();


    AppendMenuW(hAdvancedMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RUN_COMMAND ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RUN_COMMAND,
               GetLocalizedString(L"运行命令", L"Run Command"));


    AppendMenuW(hAdvancedMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_HTTP_REQUEST ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_HTTP_REQUEST,
               GetLocalizedString(L"HTTP 请求", L"HTTP Request"));


    BOOL isAdvancedOptionSelected = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RUN_COMMAND ||
                                    CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_HTTP_REQUEST);


    AppendMenuW(hTimeoutMenu, MF_POPUP | (isAdvancedOptionSelected ? MF_CHECKED : MF_UNCHECKED),
               (UINT_PTR)hAdvancedMenu,
               GetLocalizedString(L"高级", L"Advanced"));


    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(L"超时动作", L"Timeout Action"));


    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(L"倒计时预设", L"Modify Quick Countdown Options"));
    

    HMENU hStartupSettingsMenu = CreatePopupMenu();


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
    
    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(L"置顶", L"Always on Top"));


    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(L"热键设置", L"Hotkey Settings"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);


    HMENU hMoreFontsMenu = CreatePopupMenu();
    HMENU hFontSubMenu = CreatePopupMenu();
    

    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {

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


    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {

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


    AppendMenuW(hFontSubMenu, MF_POPUP, (UINT_PTR)hMoreFontsMenu, GetLocalizedString(L"更多", L"More"));


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
    
    // Display menu
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // This will allow the menu to close automatically when clicking outside
    DestroyMenu(hMenu);
}


void ShowContextMenu(HWND hwnd) {

    ReadTimeoutActionFromConfig();
    

    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
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


    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(L"倒计时", L"Countdown"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Add quick time options (use high, conflict-free ID base)
    for (int i = 0; i < time_options_count; i++) {
        wchar_t menu_item[20];
        FormatPomodoroTime(time_options[i], menu_item, sizeof(menu_item)/sizeof(wchar_t));
        AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_QUICK_TIME_BASE + i, menu_item);
    }

    // Display menu
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0); // This will allow the menu to close automatically when clicking outside
    DestroyMenu(hMenu);
}