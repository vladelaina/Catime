/**
 * @file tray_menu.c
 * @brief System tray context menus (left/right-click)
 * 
 * Key features:
 * - Dynamic menu generation from filesystem (fonts, animations)
 * - Natural sorting for numeric filenames
 * - Recursive directory scanning with checkmarks
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
#include "../include/startup.h"
#include "../include/utils/string_convert.h"

/** @brief Animation menu entry for directory scanning */
typedef struct {
    wchar_t name[MAX_PATH];
    char rel_path_utf8[MAX_PATH];
    BOOL is_dir;
} AnimationEntry;

/** @brief Font menu entry with submenu tracking */
typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t fullPath[MAX_PATH];
    wchar_t displayName[MAX_PATH];
    BOOL is_dir;
    BOOL isCurrentFont;
    int subFolderStatus;
    HMENU hSubMenu;
} FontEntry;

/**
 * @brief Convert UTF-8 path to wide character (wrapper for consistency)
 * @return TRUE on success
 */
static inline BOOL PathUtf8ToWide(const char* utf8, wchar_t* wide, size_t wideSize) {
    return Utf8ToWide(utf8, wide, wideSize);
}

/**
 * @brief Convert wide character path to UTF-8 (wrapper for consistency)
 * @return TRUE on success
 */
static inline BOOL PathWideToUtf8(const wchar_t* wide, char* utf8, size_t utf8Size) {
    return WideToUtf8(wide, utf8, utf8Size);
}

/**
 * @brief Get wide-character path to config.ini
 * @return TRUE on success
 */
static BOOL GetConfigPathWide(wchar_t* wPath, size_t size) {
    if (!wPath || size == 0) return FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return PathUtf8ToWide(configPath, wPath, size);
}

/**
 * @brief Read single key=value from config.ini
 * @param key Configuration key
 * @param outBuffer Output buffer for value
 * @param bufferSize Buffer size
 * @return TRUE if key found
 */
static BOOL ReadConfigValue(const char* key, char* outBuffer, size_t bufferSize) {
    if (!key || !outBuffer || bufferSize == 0) return FALSE;
    
    wchar_t wConfigPath[MAX_PATH];
    if (!GetConfigPathWide(wConfigPath, MAX_PATH)) return FALSE;
    
    FILE* file = _wfopen(wConfigPath, L"r");
    if (!file) return FALSE;
    
    size_t keyLen = strlen(key);
    char line[256];
    BOOL found = FALSE;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, key, keyLen) == 0 && line[keyLen] == '=') {
            char* value = line + keyLen + 1;
            size_t len = strlen(value);
            while (len > 0 && (value[len-1] == '\n' || value[len-1] == '\r')) {
                value[--len] = '\0';
            }
            strncpy(outBuffer, value, bufferSize - 1);
            outBuffer[bufferSize - 1] = '\0';
            found = TRUE;
            break;
        }
    }
    
    fclose(file);
    return found;
}

/**
 * @brief Load Pomodoro time options from config
 * @note Updates global POMODORO_TIMES array
 */
static void LoadPomodoroConfig(void) {
    
    wchar_t wConfigPath[MAX_PATH];
    if (!GetConfigPathWide(wConfigPath, MAX_PATH)) return;
    
    FILE* file = _wfopen(wConfigPath, L"r");
    if (!file) return;
    
    POMODORO_TIMES_COUNT = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "POMODORO_TIME_OPTIONS=", 22) == 0) {
            char* options = line + 22;
            char* token = strtok(options, ",");
            int index = 0;
            
            while (token && index < MAX_POMODORO_TIMES) {
                POMODORO_TIMES[index++] = atoi(token);
                token = strtok(NULL, ",");
            }
            
            POMODORO_TIMES_COUNT = index;
            
            if (index > 0) POMODORO_WORK_TIME = POMODORO_TIMES[0];
            if (index > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
            if (index > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[2];
        }
        else if (strncmp(line, "POMODORO_LOOP_COUNT=", 20) == 0) {
            sscanf(line, "POMODORO_LOOP_COUNT=%d", &POMODORO_LOOP_COUNT);
            if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
        }
    }
    
    fclose(file);
}

/**
 * @brief Natural string comparison with numeric ordering
 * @note Handles multi-digit numbers correctly (e.g., "file2" < "file10")
 */
static int NaturalCompareW(const wchar_t* a, const wchar_t* b) {
    const wchar_t* pa = a;
    const wchar_t* pb = b;
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            const wchar_t* za = pa; while (*za == L'0') za++;
            const wchar_t* zb = pb; while (*zb == L'0') zb++;
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

/** @brief Compare entries: directories first, then natural order */
static int DirFirstThenNaturalName(const wchar_t* nameA, BOOL isDirA,
                                   const wchar_t* nameB, BOOL isDirB) {
    if (isDirA != isDirB) {
        return isDirB - isDirA;
    }
    return NaturalCompareW(nameA, nameB);
}

/** @brief qsort comparator for AnimationEntry */
static int CompareAnimationEntries(const void* a, const void* b) {
    const AnimationEntry* entryA = (const AnimationEntry*)a;
    const AnimationEntry* entryB = (const AnimationEntry*)b;
    return DirFirstThenNaturalName(entryA->name, entryA->is_dir,
                                   entryB->name, entryB->is_dir);
}

/** @brief qsort comparator for FontEntry */
static int CompareFontEntries(const void* a, const void* b) {
    const FontEntry* entryA = (const FontEntry*)a;
    const FontEntry* entryB = (const FontEntry*)b;
    return DirFirstThenNaturalName(entryA->name, entryA->is_dir,
                                   entryB->name, entryB->is_dir);
}

/**
 * @brief Check if folder is a leaf (no subdirs or animated files)
 * @return TRUE if folder contains only static image frames
 */
static BOOL IsAnimationLeafFolderW(const wchar_t* folderPathW) {
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return TRUE;

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

/**
 * @brief Recursively build animation menu hierarchy
 * @param parentMenu Parent menu handle
 * @param folderPathW Wide-character folder path
 * @param folderPathUtf8 UTF-8 relative path
 * @param nextIdPtr Menu ID counter
 * @param currentAnim Current animation for checkmark
 * @return TRUE if subtree contains current animation
 * @note Leaf folders → menu items; branch folders → submenus
 */
static BOOL BuildAnimationFolderMenu(HMENU parentMenu, const wchar_t* folderPathW, 
                                     const char* folderPathUtf8, UINT* nextIdPtr, 
                                     const char* currentAnim) {
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
        PathWideToUtf8(ffd.cFileName, itemUtf8, MAX_PATH);
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
                UINT flags = MF_STRING | (currentAnim && _stricmp(e->rel_path_utf8, currentAnim) == 0 ? MF_CHECKED : 0);
                AppendMenuW(parentMenu, flags, (*nextIdPtr)++, e->name);
                if (flags & MF_CHECKED) subtreeHasCurrent = TRUE;
            } else {
                HMENU hSubMenu = CreatePopupMenu();
                BOOL childHas = BuildAnimationFolderMenu(hSubMenu, wSubFolderPath, e->rel_path_utf8, nextIdPtr, currentAnim);
                UINT folderFlags = MF_POPUP | (childHas ? MF_CHECKED : 0);
                if (childHas) subtreeHasCurrent = TRUE;
                AppendMenuW(parentMenu, folderFlags, (UINT_PTR)hSubMenu, e->name);
            }
        } else {
            UINT flags = MF_STRING | (currentAnim && _stricmp(e->rel_path_utf8, currentAnim) == 0 ? MF_CHECKED : 0);
            AppendMenuW(parentMenu, flags, (*nextIdPtr)++, e->name);
            if (flags & MF_CHECKED) subtreeHasCurrent = TRUE;
        }
    }
    free(entries);
    return subtreeHasCurrent;
}

extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern char CLOCK_TEXT_COLOR[10];
extern char FONT_FILE_NAME[];
extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;

extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];
extern int current_pomodoro_time_index;
extern POMODORO_PHASE current_pomodoro_phase;

extern void GetConfigPath(char* path, size_t size);
extern void ClearColorOptions(void);
extern void AddColorOption(const char* color);

/**
 * @brief Get fonts folder path (%LOCALAPPDATA%\Catime\resources\fonts)
 * @param out Wide-char buffer
 * @param size Buffer size
 * @return TRUE on success
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

/** @brief Load timeout action from config */
void ReadTimeoutActionFromConfig() {
    char value[32] = {0};
    if (ReadConfigValue("TIMEOUT_ACTION", value, sizeof(value))) {
        CLOCK_TIMEOUT_ACTION = (TimeoutActionType)atoi(value);
    }
}

/**
 * @brief Format time for Pomodoro menu display
 * @param seconds Duration in seconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @note Shows h:mm:ss if >1hr, mm:ss if has seconds, or just minutes
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
 * @brief Truncate long filenames for menu display
 * @param fileName Original filename
 * @param truncated Output buffer
 * @param maxLen Maximum display length
 * @note Uses middle truncation ("start...end.ext") for very long names
 */
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

/**
 * @brief Build and display right-click configuration menu
 * @param hwnd Main window handle
 * @note Creates nested menus for timeout actions, fonts, colors, animations, etc.
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
    ReadConfigValue("STARTUP_MODE", currentStartupMode, sizeof(currentStartupMode));
    
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
    
    AppendMenuW(hFormatMenu, MF_SEPARATOR, 0, NULL);
    
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
    
    int g_advancedFontId = 2000;
    
    /**
     * @brief Recursively scan font folder and build submenu
     * @return 0=no content, 1=has content, 2=contains current font
     * @note Uses heap allocation to prevent stack overflow
     */
    int ScanFontFolder(const char* folderPath, HMENU parentMenu, int* fontId) {
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
        _snwprintf_s(wSearchPath, MAX_PATH, _TRUNCATE, L"%s\\*", wFolderPath);
        
        FontEntry* entries = NULL;
        int entryCount = 0;
        int entryCapacity = 0;
        
        HANDLE hFind = FindFirstFileW(wSearchPath, findData);
        int folderStatus = 0;
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(findData->cFileName, L".") == 0 || wcscmp(findData->cFileName, L"..") == 0) {
                    continue;
                }
                
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
                _snwprintf_s(entry->fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolderPath, findData->cFileName);
                entry->is_dir = (findData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry->isCurrentFont = FALSE;
                entry->subFolderStatus = 0;
                entry->hSubMenu = NULL;
                
                if (!entry->is_dir) {
                    wchar_t* ext = wcsrchr(findData->cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                        wcsncpy(entry->displayName, findData->cFileName, MAX_PATH - 1);
                        entry->displayName[MAX_PATH - 1] = L'\0';
                        wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
                        if (dotPos) *dotPos = L'\0';
                        
                        wchar_t wFontsFolderPath[MAX_PATH] = {0};
                        if (GetFontsFolderWideFromConfig(wFontsFolderPath, MAX_PATH)) {
                            const char* localPrefix = FONTS_PATH_PREFIX;
                            if (_strnicmp(FONT_FILE_NAME, localPrefix, (int)strlen(localPrefix)) == 0) {
                                const char* relUtf8 = FONT_FILE_NAME + strlen(localPrefix);
                                wchar_t wRel[MAX_PATH] = {0};
                                MultiByteToWideChar(CP_UTF8, 0, relUtf8, -1, wRel, MAX_PATH);
                                wchar_t wCurrentFull[MAX_PATH] = {0};
                                _snwprintf_s(wCurrentFull, MAX_PATH, _TRUNCATE, L"%s\\%s", wFontsFolderPath, wRel);
                                
                                entry->isCurrentFont = (_wcsicmp(entry->fullPath, wCurrentFull) == 0);
                            }
                        }
                        
                        entryCount++;
                        
                        if (entry->isCurrentFont) {
                            folderStatus = 2;
                        } else if (folderStatus == 0) {
                            folderStatus = 1;
                        }
                    } else {
                        continue;
                    }
                } else {
                    entry->hSubMenu = CreatePopupMenu();
                    
                    char fullItemPathUtf8[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0, entry->fullPath, -1, fullItemPathUtf8, MAX_PATH, NULL, NULL);
                    
                    entry->subFolderStatus = ScanFontFolder(fullItemPathUtf8, entry->hSubMenu, fontId);
                    
                    entryCount++;
                    
                    if (entry->subFolderStatus == 2) {
                        folderStatus = 2;
                    } else if (entry->subFolderStatus == 1 && folderStatus == 0) {
                        folderStatus = 1;
                    }
                }
            } while (FindNextFileW(hFind, findData));
            FindClose(hFind);
        }
        
        if (entryCount > 0) {
            qsort(entries, entryCount, sizeof(FontEntry), CompareFontEntries);
            
            for (int i = 0; i < entryCount; i++) {
                FontEntry* entry = &entries[i];
                
                if (!entry->is_dir) {
                    AppendMenuW(parentMenu, MF_STRING | (entry->isCurrentFont ? MF_CHECKED : MF_UNCHECKED),
                              (*fontId)++, entry->displayName);
                } else {
                    if (entry->subFolderStatus == 0) {
                        AppendMenuW(entry->hSubMenu, MF_STRING | MF_GRAYED, 0, L"(Empty folder)");
                        AppendMenuW(parentMenu, MF_POPUP, (UINT_PTR)entry->hSubMenu, entry->name);
                    } else {
                        UINT folderFlags = MF_POPUP;
                        if (entry->subFolderStatus == 2) {
                            folderFlags |= MF_CHECKED;
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
    
    extern BOOL NeedsFontLicenseVersionAcceptance(void);
    
    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE, 
                   GetLocalizedString(L"点击同意许可协议后继续", L"Click to agree to license agreement"));
    } else {
        wchar_t wFontsFolder[MAX_PATH] = {0};
        if (GetFontsFolderWideFromConfig(wFontsFolder, MAX_PATH)) {
            char fontsFolderPathUtf8[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, wFontsFolder, -1, fontsFolderPathUtf8, MAX_PATH, NULL, NULL);
            
            g_advancedFontId = 2000;
            
            int fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId);
            if (fontFolderStatus == 0) {
                WriteLog(LOG_LEVEL_INFO, "Font scan failed, checking known fonts...");
                wchar_t wTestFontPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, fontsFolderPathUtf8, -1, wTestFontPath, MAX_PATH - 32);
                wcscat(wTestFontPath, L"\\Wallpoet Essence.ttf");
                DWORD attribs = GetFileAttributesW(wTestFontPath);
                if (attribs != INVALID_FILE_ATTRIBUTES) {
                    WriteLog(LOG_LEVEL_WARNING, "Wallpoet Essence.ttf exists but scan failed!");
                } else {
                    WriteLog(LOG_LEVEL_INFO, "Wallpoet Essence.ttf does not exist");
                }
            }
            WriteLog(LOG_LEVEL_INFO, "Font folder scan result: %d", fontFolderStatus);
            
            if (fontFolderStatus == 0) {
                extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);
                HINSTANCE hInst = GetModuleHandle(NULL);
                if (ExtractEmbeddedFontsToFolder(hInst)) {
                    fontFolderStatus = ScanFontFolder(fontsFolderPathUtf8, hFontSubMenu, &g_advancedFontId);
                }
            }

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

    HMENU hAnimMenu = CreatePopupMenu();
    {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);

        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        const char* currentAnim = GetCurrentAnimationName();

        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__logo__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_LOGO, GetLocalizedString(L"使用Logo", L"Use Logo"));
        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__cpu__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_CPU, GetLocalizedString(L"CPU 百分比", L"CPU Percent"));
        AppendMenuW(hAnimMenu, MF_STRING | (currentAnim && _stricmp(currentAnim, "__mem__") == 0 ? MF_CHECKED : 0),
                    CLOCK_IDM_ANIMATIONS_USE_MEM, GetLocalizedString(L"内存百分比", L"Memory Percent"));
        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

        (void)BuildAnimationFolderMenu(hAnimMenu, wRoot, "", &nextId, currentAnim);
        
        if (GetMenuItemCount(hAnimMenu) <= 4) {
            AppendMenuW(hAnimMenu, MF_STRING | MF_GRAYED, 0, GetLocalizedString(L"(支持 GIF、WebP、PNG 等)", L"(Supports GIF, WebP, PNG, etc.)"));
        }

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

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

    AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hLangMenu, L"Language");

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
 * @brief Build and display left-click timer control menu
 * @param hwnd Main window handle
 * @note Includes timer management, Pomodoro, and quick countdown options
 */
void ShowContextMenu(HWND hwnd) {
    ReadTimeoutActionFromConfig();
    
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    HMENU hMenu = CreatePopupMenu();
    
    HMENU hTimerManageMenu = CreatePopupMenu();
    
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(L"继续", L"Resume") : 
                                    GetLocalizedString(L"暂停", L"Pause");
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || 
                      (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0)));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(L"重新开始", L"Start Over"));
    
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

    LoadPomodoroConfig();

    HMENU hPomodoroMenu = CreatePopupMenu();
    
    wchar_t timeBuffer[64];
    
    AppendMenuW(hPomodoroMenu, MF_STRING, CLOCK_IDM_POMODORO_START,
                GetLocalizedString(L"开始", L"Start"));
    AppendMenuW(hPomodoroMenu, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        FormatPomodoroTime(POMODORO_TIMES[i], timeBuffer, sizeof(timeBuffer)/sizeof(wchar_t));
        
        UINT menuId;
        if (i == 0) menuId = CLOCK_IDM_POMODORO_WORK;
        else if (i == 1) menuId = CLOCK_IDM_POMODORO_BREAK;
        else if (i == 2) menuId = CLOCK_IDM_POMODORO_LBREAK;
        else menuId = CLOCK_IDM_POMODORO_TIME_BASE + i;
        
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