/**
 * @file tray_menu.c
 * @brief Complex popup menu system for system tray right-click and left-click menus
 * Handles extensive configuration options, font/color selection, and multilingual support
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
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
#include "../include/tray_animation.h"

/** @brief Represents a file or folder entry for sorting animation menus. */
typedef struct {
    wchar_t name[MAX_PATH];
    char rel_path_utf8[MAX_PATH]; /** Relative path from animations root */
    BOOL is_dir;
} AnimationEntry;

/** @brief Font entry structure for natural sorting */
typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t fullPath[MAX_PATH];
    wchar_t displayName[MAX_PATH];
    BOOL is_dir;
    BOOL isCurrentFont;
    int subFolderStatus; /** For directories: 0=no content, 1=has content, 2=contains current font */
    HMENU hSubMenu; /** For directories: submenu handle */
} FontEntry;

/** @brief Natural string compare that sorts numeric substrings by value (e.g., 2 < 10). */
static int NaturalCompareW(const wchar_t* a, const wchar_t* b) {
    const wchar_t* pa = a;
    const wchar_t* pb = b;
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            const wchar_t* za = pa; while (*za == L'0') za++;
            const wchar_t* zb = pb; while (*zb == L'0') zb++;
            /** Primary rule: numbers with more leading zeros come first */
            size_t leadA = (size_t)(za - pa);
            size_t leadB = (size_t)(zb - pb);
            if (leadA != leadB) return (leadA > leadB) ? -1 : 1;
            const wchar_t* ea = za; while (iswdigit(*ea)) ea++;
            const wchar_t* eb = zb; while (iswdigit(*eb)) eb++;
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) return (lena < lenb) ? -1 : 1;
            int dcmp = wcsncmp(za, zb, lena);
            if (dcmp != 0) return (dcmp < 0) ? -1 : 1;
            pa = ea;
            pb = eb;
            continue;
        }
        wchar_t ca = towlower(*pa);
        wchar_t cb = towlower(*pb);
        if (ca != cb) return (ca < cb) ? -1 : 1;
        pa++; pb++;
    }
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

/** @brief Shared comparator core: directories first, then natural string compare by name. */
static int DirFirstThenNaturalName(const wchar_t* nameA, BOOL isDirA,
                                   const wchar_t* nameB, BOOL isDirB) {
    if (isDirA != isDirB) {
        return isDirB - isDirA; /** Directories first */
    }
    return NaturalCompareW(nameA, nameB);
}

/** @brief qsort comparator for AnimationEntry, sorting directories first, then by natural order. */
static int CompareAnimationEntries(const void* a, const void* b) {
    const AnimationEntry* entryA = (const AnimationEntry*)a;
    const AnimationEntry* entryB = (const AnimationEntry*)b;
    return DirFirstThenNaturalName(entryA->name, entryA->is_dir,
                                   entryB->name, entryB->is_dir);
}

/** @brief qsort comparator for FontEntry, using natural sorting with directories first */
static int CompareFontEntries(const void* a, const void* b) {
    const FontEntry* entryA = (const FontEntry*)a;
    const FontEntry* entryB = (const FontEntry*)b;
    return DirFirstThenNaturalName(entryA->name, entryA->is_dir,
                                   entryB->name, entryB->is_dir);
}

/** @brief Checks if a folder contains no sub-folders or animated images, making it a leaf. */
static BOOL IsAnimationLeafFolderW(const wchar_t* folderPathW) {
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return TRUE; // Empty is a leaf

    BOOL hasSubItems = FALSE;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            hasSubItems = TRUE;
            break;
        }
        wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
        if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
            hasSubItems = TRUE;
            break;
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    return !hasSubItems;
}

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
 * @brief Get %LOCALAPPDATA%\Catime\resources\fonts in wide-char using config path
 * @param out Wide-char buffer
 * @param size Buffer size (wchar_t count)
 */
static BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size) {
    if (!out || size == 0) return FALSE;
    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);
    if (configPathUtf8[0] == '\0') return FALSE;
    wchar_t wconfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wconfigPath, MAX_PATH);
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 1 >= size) return FALSE;
    wcsncpy(out, wconfigPath, dirLen);
    out[dirLen] = L'\0';
    const wchar_t* tail = L"\\resources\\fonts";
    if (wcslen(out) + wcslen(tail) + 1 >= size) return FALSE;
    wcscat(out, tail);
    return TRUE;
}

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
    
    /** ANSI fallback scanner removed to ensure consistent wide-char API usage */
    

    /** Recursive function to scan folder and create submenus */
    /** Returns: 0 = no content, 1 = has content but no current font, 2 = contains current font */
    int ScanFontFolder(const char* folderPath, HMENU parentMenu, int* fontId) {
        /** Use heap allocation for buffers to prevent stack overflow in deep recursion */
        wchar_t* wFolderPath = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
        wchar_t* wSearchPath = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
        WIN32_FIND_DATAW* findData = (WIN32_FIND_DATAW*)malloc(sizeof(WIN32_FIND_DATAW));

        if (!wFolderPath || !wSearchPath || !findData) {
            if (wFolderPath) free(wFolderPath);
            if (wSearchPath) free(wSearchPath);
            if (findData) free(findData);
            return 0;
        }

        MultiByteToWideChar(CP_UTF8, 0, folderPath, -1, wFolderPath, MAX_PATH);
        swprintf(wSearchPath, MAX_PATH, L"%s\\*", wFolderPath);
        
        /** Collect all entries first */
        FontEntry* entries = NULL;
        int entryCount = 0;
        int entryCapacity = 0;
        
        HANDLE hFind = FindFirstFileW(wSearchPath, findData);
        int folderStatus = 0; /** 0 = no content, 1 = has content, 2 = contains current font */
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                /** Skip . and .. entries */
                if (wcscmp(findData->cFileName, L".") == 0 || wcscmp(findData->cFileName, L"..") == 0) {
                    continue;
                }
                
                /** Expand entries array if needed */
                if (entryCount >= entryCapacity) {
                    entryCapacity = entryCapacity == 0 ? 16 : entryCapacity * 2;
                    FontEntry* newEntries = (FontEntry*)realloc(entries, entryCapacity * sizeof(FontEntry));
                    if (!newEntries) {
                        free(entries);
                        free(wFolderPath);
                        free(wSearchPath);
                        free(findData);
                        FindClose(hFind);
                        return 0;
                    }
                    entries = newEntries;
                }
                
                FontEntry* entry = &entries[entryCount];
                wcsncpy(entry->name, findData->cFileName, MAX_PATH - 1);
                entry->name[MAX_PATH - 1] = L'\0';
                swprintf(entry->fullPath, MAX_PATH, L"%s\\%s", wFolderPath, findData->cFileName);
                entry->is_dir = (findData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry->isCurrentFont = FALSE;
                entry->subFolderStatus = 0;
                entry->hSubMenu = NULL;
                
                if (!entry->is_dir) {
                    /** Handle regular font files */
                    wchar_t* ext = wcsrchr(findData->cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                        /** Remove extension for display */
                        wcsncpy(entry->displayName, findData->cFileName, MAX_PATH - 1);
                        entry->displayName[MAX_PATH - 1] = L'\0';
                        wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
                        if (dotPos) *dotPos = L'\0';
                        
                        /** Check if this is the current font */
                        wchar_t wFontsFolderPath[MAX_PATH] = {0};
                        if (GetFontsFolderWideFromConfig(wFontsFolderPath, MAX_PATH)) {
                            const char* localPrefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
                            if (_strnicmp(FONT_FILE_NAME, localPrefix, (int)strlen(localPrefix)) == 0) {
                                const char* relUtf8 = FONT_FILE_NAME + strlen(localPrefix);
                                wchar_t wRel[MAX_PATH] = {0};
                                MultiByteToWideChar(CP_UTF8, 0, relUtf8, -1, wRel, MAX_PATH);
                                wchar_t wCurrentFull[MAX_PATH] = {0};
                                _snwprintf_s(wCurrentFull, MAX_PATH, _TRUNCATE, L"%s\\%s", wFontsFolderPath, wRel);
                                
                                /** Compare with candidate file path */
                                entry->isCurrentFont = (_wcsicmp(entry->fullPath, wCurrentFull) == 0);
                            }
                        }
                        
                        entryCount++;
                        
                        /** Update folder status */
                        if (entry->isCurrentFont) {
                            folderStatus = 2; /** This folder contains the current font */
                        } else if (folderStatus == 0) {
                            folderStatus = 1; /** This folder has content but not the current font */
                        }
                    } else {
                        /** Skip non-font files */
                        continue;
                    }
                } else {
                    /** Handle subdirectories */
                    entry->hSubMenu = CreatePopupMenu();
                    
                    /** Convert wide path back to UTF-8 for recursive call */
                    char fullItemPathUtf8[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0, entry->fullPath, -1, fullItemPathUtf8, MAX_PATH, NULL, NULL);
                    
                    /** Recursively scan this subdirectory */
                    entry->subFolderStatus = ScanFontFolder(fullItemPathUtf8, entry->hSubMenu, fontId);
                    
                    entryCount++;
                    
                    /** Update current folder status based on subfolder status */
                    if (entry->subFolderStatus == 2) {
                        folderStatus = 2; /** This folder contains the current font (in subfolder) */
                    } else if (entry->subFolderStatus == 1 && folderStatus == 0) {
                        folderStatus = 1; /** This folder has content but not the current font */
                    }
                }
            } while (FindNextFileW(hFind, findData));
            FindClose(hFind);
        }
        
        /** Sort entries using natural sorting */
        if (entryCount > 0) {
            qsort(entries, entryCount, sizeof(FontEntry), CompareFontEntries);
            
            /** Add sorted entries to menu */
            for (int i = 0; i < entryCount; i++) {
                FontEntry* entry = &entries[i];
                
                if (!entry->is_dir) {
                    /** Add font file */
                    AppendMenuW(parentMenu, MF_STRING | (entry->isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                              (*fontId)++, entry->displayName);
                } else {
                    /** Add directory submenu */
                    if (entry->subFolderStatus == 0) {
                        /** Add "Empty folder" indicator */
                        AppendMenuW(entry->hSubMenu, MF_STRING | MF_GRAYED, 0, L"(Empty folder)");
                        AppendMenuW(parentMenu, MF_POPUP, (UINT_PTR)entry->hSubMenu, entry->name);
                    } else {
                        /** Add folder with check mark if it contains current font */
                        UINT folderFlags = MF_POPUP;
                        if (entry->subFolderStatus == 2) {
                            folderFlags |= MF_CHECKED; /** Folder contains current font */
                        }
                        AppendMenuW(parentMenu, folderFlags, (UINT_PTR)entry->hSubMenu, entry->name);
                    }
                }
            }
        }
        
        free(entries);
        free(wFolderPath);
        free(wSearchPath);
        free(findData);

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
        wchar_t wFontsFolder[MAX_PATH] = {0};
        if (GetFontsFolderWideFromConfig(wFontsFolder, MAX_PATH)) {
            char fontsFolderPathUtf8[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, wFontsFolder, -1, fontsFolderPathUtf8, MAX_PATH, NULL, NULL);
            
            g_advancedFontId = 2000; /** Reset global font ID counter */
            
            /** Use recursive function to scan all folders and subfolders directly in main font menu */
            /** Try Unicode scan first, fallback to ANSI if needed */
            int fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId);

            /** No ANSI fallback: rely solely on wide-char scanning */

            /** Additional debug: manually check some known font files */
            if (fontFolderStatus == 0) {
                WriteLog(LOG_LEVEL_INFO, "Both scans failed, manually checking known font files...");
                char testFontPath[MAX_PATH];
                snprintf(testFontPath, MAX_PATH, "%s\\Wallpoet Essence.ttf", fontsFolderPathUtf8);
                DWORD attribs = GetFileAttributesA(testFontPath);
                if (attribs != INVALID_FILE_ATTRIBUTES) {
                    WriteLog(LOG_LEVEL_WARNING, "Manual check: Wallpoet Essence.ttf EXISTS but scan failed to find it!");
                } else {
                    WriteLog(LOG_LEVEL_INFO, "Manual check: Wallpoet Essence.ttf does not exist");
                }
            }
            WriteLog(LOG_LEVEL_INFO, "Font folder scan result: %d (0=no content, 1=has content, 2=contains current font)", fontFolderStatus);
            
            /** If no fonts found, try extracting embedded fonts once and rescan */
            if (fontFolderStatus == 0) {
                extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);
                HINSTANCE hInst = GetModuleHandle(NULL);
                if (ExtractEmbeddedFontsToFolder(hInst)) {
                    fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId);
                }
            }

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

    /** Animations submenu */
    HMENU hAnimMenu = CreatePopupMenu();
    {
        /**
         * NOTE: The menu is built by scanning the filesystem. To ensure the command handler
         * maps the menu ID back to the correct file, both this logic and the handler
         * MUST use an identical, deterministic sorting order.
         */
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);

        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        const char* currentAnim = GetCurrentAnimationName();

        /** Add fixed entries: logo, CPU %, Memory % */
        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__logo__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_LOGO, GetLocalizedString(L"使用Logo", L"Use Logo"));
        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__cpu__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_CPU, GetLocalizedString(L"CPU 百分比", L"CPU Percent"));
        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__mem__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_MEM, GetLocalizedString(L"内存百分比", L"Memory Percent"));
        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

        /** Recursive helper function to build menu for a folder; returns TRUE if subtree contains currentAnim */
        BOOL BuildFolderMenuRecursive(HMENU parentMenu, const wchar_t* folderPathW, const char* folderPathUtf8, UINT* nextIdPtr, const char* currentAnim) {
            AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_TRAY_FRAMES);
            if (!entries) return FALSE;
            int entryCount = 0;
 
             wchar_t wSearch[MAX_PATH] = {0};
             _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
             
             WIN32_FIND_DATAW ffd;
             HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind == INVALID_HANDLE_VALUE) {
                free(entries);
                return FALSE;
            }
 
             do {
                 if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                 if (entryCount >= MAX_TRAY_FRAMES) break;
 
                 AnimationEntry* e = &entries[entryCount];
                 e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                 wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
                 e->name[MAX_PATH - 1] = L'\0';
 
                 char itemUtf8[MAX_PATH] = {0};
                 WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
                 if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
                     _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
                 } else {
                     _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
                 }
                 
                if (e->is_dir) {
                     entryCount++;
                 } else {
                    wchar_t* ext = wcsrchr(e->name, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                                _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                                _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                                _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                                _wcsicmp(ext, L".tiff") == 0)) {
                         entryCount++;
                     }
                 }
             } while (FindNextFileW(hFind, &ffd));
             FindClose(hFind);
 
             if (entryCount == 0) {
                 free(entries);
                 return FALSE;
             }
             qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);
 
             BOOL subtreeHasCurrent = FALSE;
             for (int i = 0; i < entryCount; ++i) {
                 AnimationEntry* e = &entries[i];
                 if (e->is_dir) {
                     wchar_t wSubFolderPath[MAX_PATH] = {0};
                     _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);
                     
                     if (IsAnimationLeafFolderW(wSubFolderPath)) {
                         // Leaf folder, add as a clickable item.
                         UINT flags = MF_STRING | (currentAnim && _stricmp(e->rel_path_utf8, currentAnim) == 0 ? MF_CHECKED : 0);
                         AppendMenuW(parentMenu, flags, (*nextIdPtr)++, e->name);
                         if (flags & MF_CHECKED) subtreeHasCurrent = TRUE;
                     } else {
                         // Branch folder, create a submenu.
                         HMENU hSubMenu = CreatePopupMenu();
                         BOOL childHas = BuildFolderMenuRecursive(hSubMenu, wSubFolderPath, e->rel_path_utf8, nextIdPtr, currentAnim);
                         UINT folderFlags = MF_POPUP | (childHas ? MF_CHECKED : 0);
                         if (childHas) subtreeHasCurrent = TRUE;
                         AppendMenuW(parentMenu, folderFlags, (UINT_PTR)hSubMenu, e->name);
                     }
                } else {
                     // File item.
                     UINT flags = MF_STRING | (currentAnim && _stricmp(e->rel_path_utf8, currentAnim) == 0 ? MF_CHECKED : 0);
                     AppendMenuW(parentMenu, flags, (*nextIdPtr)++, e->name);
                     if (flags & MF_CHECKED) subtreeHasCurrent = TRUE;
                 }
             }
             free(entries);
             return subtreeHasCurrent;
         }

        (void)BuildFolderMenuRecursive(hAnimMenu, wRoot, "", &nextId, currentAnim);
        
        // Fallback message if no items were added at all.
        if (GetMenuItemCount(hAnimMenu) <= 2) { // Logo and separator are always there
            AppendMenuW(hAnimMenu, MF_STRING | MF_GRAYED, 0, GetLocalizedString(L"(无动画文件夹)", L"(No animation folders)"));
        }

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

        /** Animation speed metric sub options */
        HMENU hAnimSpeedMenu = CreatePopupMenu();
        AnimationSpeedMetric currentMetric = GetAnimationSpeedMetric();
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_MEMORY ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_MEMORY, GetLocalizedString(L"按内存占用", L"By Memory Usage"));
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_CPU ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_CPU, GetLocalizedString(L"按CPU占用", L"By CPU Usage"));
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_TIMER ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_TIMER, GetLocalizedString(L"按倒计时进度", L"By Countdown Progress"));
        AppendMenuW(hAnimMenu, MF_POPUP, (UINT_PTR)hAnimSpeedMenu,
                    GetLocalizedString(L"动画速度依据", L"Animation Speed Metric"));

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hAnimMenu, MF_STRING, CLOCK_IDM_ANIMATIONS_OPEN_DIR, GetLocalizedString(L"打开动画文件夹", L"Open animations folder"));
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAnimMenu, GetLocalizedString(L"托盘图标", L"Tray Icon"));

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
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
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
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}