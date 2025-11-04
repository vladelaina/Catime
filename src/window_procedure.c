/**
 * @file window_procedure.c
 * @brief Table-driven window procedure with dispatch tables
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include "../resource/resource.h"
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/tray.h"
#include "../include/tray_menu.h"
#include "../include/timer.h"
#include "../include/window.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/window_events.h"
#include "../include/drag_scale.h"
#include "../include/drawing.h"
#include "../include/timer_events.h"
#include "../include/tray_events.h"
#include "../include/dialog_procedure.h"
#include "../include/pomodoro.h"
#include "../include/update_checker.h"
#include "../include/async_update_checker.h"
#include "../include/hotkey.h"
#include "../include/notification.h"
#include "../include/cli.h"
#include "../include/tray_animation_core.h"
#include "../include/tray_animation_loader.h"
#include "../include/tray_animation_menu.h"
#include "../include/menu_preview.h"
#include "../include/utils/natural_sort.h"

/* ============================================================================
 * String Constant Pool
 * ============================================================================ */

/** Shared INI value constants */
static const char* const STR_TRUE = "TRUE";
static const char* const STR_FALSE = "FALSE";
static const char* const STR_NONE = "None";
static const char* const STR_DEFAULT = "DEFAULT";
static const char* const STR_MESSAGE = "MESSAGE";
static const char* const STR_OK = "OK";

/* ============================================================================
 * String Conversion System
 * ============================================================================ */

/** UTF-8 → UTF-16 wrapper with validity flag */
typedef struct {
    wchar_t buf[MAX_PATH];
    BOOL valid;
} WideString;

/** UTF-16 → UTF-8 wrapper with validity flag */
typedef struct {
    char buf[MAX_PATH];
    BOOL valid;
} Utf8String;

/**
 * @brief Convert UTF-8 to UTF-16
 * @param utf8 Source string
 * @return Wrapped result - check `.valid` before use
 */
static inline WideString ToWide(const char* utf8) {
    WideString ws = {{0}, FALSE};
    if (utf8) {
        ws.valid = (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, ws.buf, MAX_PATH) > 0);
    }
    return ws;
}

/**
 * @brief Convert UTF-16 to UTF-8
 * @param wide Source string
 * @return Wrapped result - check `.valid` before use
 */
static inline Utf8String ToUtf8(const wchar_t* wide) {
    Utf8String us = {{0}, FALSE};
    if (wide) {
        us.valid = (WideCharToMultiByte(CP_UTF8, 0, wide, -1, us.buf, MAX_PATH, NULL, NULL) > 0);
    }
    return us;
}

/* ============================================================================
 * Configuration Key Constants
 * ============================================================================ */
#define CFG_KEY_TEXT_COLOR           "CLOCK_TEXT_COLOR"
#define CFG_KEY_BASE_FONT_SIZE       "CLOCK_BASE_FONT_SIZE"
#define CFG_KEY_WINDOW_POS_X         "CLOCK_WINDOW_POS_X"
#define CFG_KEY_WINDOW_POS_Y         "CLOCK_WINDOW_POS_Y"
#define CFG_KEY_WINDOW_SCALE         "WINDOW_SCALE"
#define CFG_KEY_WINDOW_TOPMOST       "WINDOW_TOPMOST"
#define CFG_KEY_USE_24HOUR           "CLOCK_USE_24HOUR"
#define CFG_KEY_SHOW_SECONDS         "CLOCK_SHOW_SECONDS"
#define CFG_KEY_SHOW_MILLISECONDS    "CLOCK_SHOW_MILLISECONDS"
#define CFG_KEY_TIME_FORMAT          "CLOCK_TIME_FORMAT"
#define CFG_KEY_TIMEOUT_ACTION       "CLOCK_TIMEOUT_ACTION"
#define CFG_KEY_TIMEOUT_FILE         "CLOCK_TIMEOUT_FILE"
#define CFG_KEY_TIMEOUT_TEXT         "CLOCK_TIMEOUT_TEXT"
#define CFG_KEY_TIMEOUT_WEBSITE      "CLOCK_TIMEOUT_WEBSITE"
#define CFG_KEY_DEFAULT_START_TIME   "CLOCK_DEFAULT_START_TIME"
#define CFG_KEY_TIME_OPTIONS         "CLOCK_TIME_OPTIONS"
#define CFG_KEY_STARTUP_MODE         "STARTUP_MODE"
#define CFG_KEY_POMODORO_OPTIONS     "POMODORO_TIME_OPTIONS"
#define CFG_KEY_POMODORO_LOOP_COUNT  "POMODORO_LOOP_COUNT"
#define CFG_KEY_HOTKEY_CUSTOM_CD     "HOTKEY_CUSTOM_COUNTDOWN"

#define CFG_SECTION_DISPLAY     INI_SECTION_DISPLAY
#define CFG_SECTION_TIMER       INI_SECTION_TIMER
#define CFG_SECTION_POMODORO    INI_SECTION_POMODORO
#define CFG_SECTION_COLORS      INI_SECTION_COLORS

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

#define UNUSED(...) (void)(__VA_ARGS__)

#define RESTART_TIMER_INTERVAL(hwnd) \
    do { KillTimer((hwnd), 1); ResetTimerWithInterval(hwnd); } while(0)

static inline void ClearInputBuffer(wchar_t* buffer, size_t size) {
    memset(buffer, 0, size);
}

#define IS_IN_RANGE(val, base, count) ((val) >= (base) && (val) < ((base) + (count)))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ============================================================================
 * Error Handling System
 * ============================================================================ */

#define VALIDATE_HWND(hwnd) \
    do { if (!(hwnd) || !IsWindow(hwnd)) return FALSE; } while(0)

#define VALIDATE_PTR(ptr) \
    do { if (!(ptr)) return FALSE; } while(0)

#define VALIDATE_FILE_EXISTS(path, hwnd) \
    do { \
        WideString _ws = ToWide(path); \
        if (!_ws.valid || GetFileAttributesW(_ws.buf) == INVALID_FILE_ATTRIBUTES) { \
            ShowError(hwnd, ERR_FILE_NOT_FOUND); \
            return FALSE; \
        } \
    } while(0)

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/** Batch config writes + defer redraw until ConfigApply() */
typedef struct ConfigWriter {
    HWND hwnd;
    BOOL needsRefresh;
} ConfigWriter;

static inline ConfigWriter ConfigUpdate(HWND hwnd) {
    ConfigWriter cw = {hwnd, FALSE};
    return cw;
}

static inline ConfigWriter* ConfigSet(ConfigWriter* cw, const char* key, const char* val) {
    if (cw) {
        WriteConfigKeyValue(key, val);
        cw->needsRefresh = TRUE;
    }
    return cw;
}

static inline void ConfigApply(ConfigWriter* cw) {
    if (cw && cw->needsRefresh && cw->hwnd) {
        InvalidateRect(cw->hwnd, NULL, TRUE);
    }
}

static inline void UpdateConfigWithRefresh(HWND hwnd, const char* key, const char* value) {
    ConfigWriter cw = ConfigUpdate(hwnd);
    ConfigSet(&cw, key, value);
    ConfigApply(&cw);
}

/* ============================================================================
 * Command Generation System
 * ============================================================================ */

/** Generate WM_COMMAND handler: static LRESULT Cmd##name(...) */
#define CMD(name, cleanup, action) \
    static LRESULT Cmd##name(HWND hwnd, WPARAM wp, LPARAM lp) { \
        UNUSED(wp, lp); \
        if (cleanup) CleanupBeforeTimerAction(); \
        action; \
        return 0; \
    }

#define WRITE_CFG_REFRESH(fn, ...) \
    do { fn(__VA_ARGS__); InvalidateRect(hwnd, NULL, TRUE); } while(0)

#define TIMER_PARAMS_COUNTDOWN(dur) {dur, TRUE, FALSE, TRUE}
#define TIMER_PARAMS_SHOW_TIME() {0, TRUE, TRUE, TRUE}
#define TIMER_PARAMS_COUNTUP() {0, TRUE, TRUE, TRUE}

#define CONFIG_RELOAD_HANDLER(name) \
    static LRESULT HandleApp##name##Changed(HWND hwnd)

/* ============================================================================
 * Range Handler Generator
 * ============================================================================ */

/** Generate handler for menu ID ranges: static BOOL Handle##name(...) */
#define RANGE_HANDLER(name, validation, action) \
    static BOOL Handle##name(HWND hwnd, UINT cmd, int index) { \
        (void)cmd; \
        if (!(validation)) return FALSE; \
        action; \
        return TRUE; \
    }

/* ============================================================================
 * Path Operation Utilities
 * ============================================================================ */

/** Append path component with automatic backslash */
static inline BOOL PathJoinW(wchar_t* base, size_t baseSize, const wchar_t* component) {
    if (!base || !component || baseSize == 0) return FALSE;
    size_t len = wcslen(base);
    if (len > 0 && base[len - 1] != L'\\') {
        if (len + 1 >= baseSize) return FALSE;
        wcscat_s(base, baseSize, L"\\");
    }
    return wcscat_s(base, baseSize, component) == 0;
}

/** Extract relative path by stripping root prefix */
static BOOL GetRelativePathW(const wchar_t* root, const wchar_t* target, 
                            wchar_t* relative, size_t relativeSize) {
    if (!root || !target || !relative || relativeSize == 0) return FALSE;
    size_t rootLen = wcslen(root);
    if (_wcsnicmp(target, root, rootLen) != 0) return FALSE;
    const wchar_t* rel = target + rootLen;
    while (*rel == L'\\') rel++;
    return wcsncpy_s(relative, relativeSize, rel, _TRUNCATE) == 0;
}

/* ============================================================================
 * Error Handling Utilities
 * ============================================================================ */

typedef enum {
    ERR_NONE = 0,
    ERR_FILE_NOT_FOUND,
    ERR_INVALID_INPUT,
    ERR_BUFFER_TOO_SMALL,
    ERR_OPERATION_FAILED
} ErrorCode;

/** Show localized error dialog */
static void ShowError(HWND hwnd, ErrorCode errorCode) {
    const wchar_t* title = GetLocalizedString(L"错误", L"Error");
    const wchar_t* message;
    
    switch (errorCode) {
        case ERR_FILE_NOT_FOUND:
            message = GetLocalizedString(L"所选文件不存在", L"Selected file does not exist");
            break;
        case ERR_INVALID_INPUT:
            message = GetLocalizedString(L"输入格式不正确", L"Invalid input format");
            break;
        case ERR_BUFFER_TOO_SMALL:
            message = GetLocalizedString(L"缓冲区太小", L"Buffer too small");
            break;
        case ERR_OPERATION_FAILED:
            message = GetLocalizedString(L"操作失败", L"Operation failed");
            break;
        default:
            message = GetLocalizedString(L"未知错误", L"Unknown error");
    }
    
    MessageBoxW(hwnd, message, title, MB_ICONERROR);
}

#define REQUIRE_NON_NULL(ptr, retval) \
    do { if (!(ptr)) { ShowError(NULL, ERR_OPERATION_FAILED); return (retval); } } while(0)

#define REQUIRE_RANGE(cond, retval) \
    do { if (!(cond)) { ShowError(NULL, ERR_INVALID_INPUT); return (retval); } } while(0)

/* ============================================================================
 * Configuration Access Helpers
 * ============================================================================ */

static __declspec(thread) char g_configPathCache[MAX_PATH] = {0};
static __declspec(thread) BOOL g_configPathCached = FALSE;

/** Cached config path getter */
static inline const char* GetCachedConfigPath(void) {
    if (!g_configPathCached) {
        GetConfigPath(g_configPathCache, MAX_PATH);
        g_configPathCached = TRUE;
    }
    return g_configPathCache;
}

static inline void ReadConfigStr(const char* section, const char* key, 
                                 const char* defaultVal, char* out, size_t size) {
    ReadIniString(section, key, defaultVal, out, (int)size, GetCachedConfigPath());
}

static inline int ReadConfigInt(const char* section, const char* key, int defaultVal) {
    return ReadIniInt(section, key, defaultVal, GetCachedConfigPath());
}

static inline BOOL ReadConfigBool(const char* section, const char* key, BOOL defaultVal) {
    return ReadIniBool(section, key, defaultVal, GetCachedConfigPath());
}

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef struct {
    const wchar_t* title;
    const wchar_t* prompt;
    const wchar_t* defaultText;
    wchar_t* result;
    size_t maxLen;
} InputBoxParams;

typedef LRESULT (*CommandHandler)(HWND hwnd, WPARAM wp, LPARAM lp);
typedef void (*SimpleAction)(HWND hwnd);
typedef LRESULT (*AppMessageHandler)(HWND hwnd);

typedef struct {
    UINT cmdId;
    CommandHandler handler;
    const char* description;
} CommandDispatchEntry;

typedef struct {
    UINT msgId;
    AppMessageHandler handler;
    const char* description;
} AppMessageDispatchEntry;

/* ============================================================================
 * Configuration Reload System
 * ============================================================================ */

typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_CUSTOM
} ConfigItemType;

/** Declarative config descriptor for hot-reload */
typedef struct {
    ConfigItemType type;
    const char* section;
    const char* key;
    void* target;
    size_t targetSize;
    const void* defaultValue;
    BOOL (*customLoader)(const char* section, const char* key, void* target, const void* def);
    BOOL triggerRedraw;
} ConfigItem;

static BOOL ReloadConfigItems(HWND hwnd, const ConfigItem* items, size_t count);

/** @brief Load string config - returns TRUE if value changed */
static BOOL LoadConfigString(const char* section, const char* key, void* target, size_t size, const char* def) {
    char temp[512];
    ReadConfigStr(section, key, def, temp, sizeof(temp));
    if (strcmp(temp, (char*)target) != 0) {
        strncpy_s(target, size, temp, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

/** @brief Load integer config - returns TRUE if value changed */
static BOOL LoadConfigInt(const char* section, const char* key, void* target, int def) {
    int temp = ReadConfigInt(section, key, def);
    if (temp != *(int*)target) {
        *(int*)target = temp;
        return TRUE;
    }
    return FALSE;
}

/** @brief Load boolean config - returns TRUE if value changed */
static BOOL LoadConfigBool(const char* section, const char* key, void* target, BOOL def) {
    BOOL temp = ReadConfigBool(section, key, def);
    if (temp != *(BOOL*)target) {
        *(BOOL*)target = temp;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Load float config with positive value validation
 * @return TRUE if value changed and valid
 * 
 * Why epsilon comparison: Prevents float rounding errors from triggering spurious redraws.
 */
static BOOL LoadConfigFloat(const char* section, const char* key, void* target, float def) {
    char buffer[32];
    ReadConfigStr(section, key, "", buffer, sizeof(buffer));
    if (buffer[0]) {
        float temp = (float)atof(buffer);
        if (temp > 0.0f && fabsf(temp - *(float*)target) > 0.0001f) {
            *(float*)target = temp;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Reload config items from INI file
 * @param hwnd Window for UI refresh (may be NULL)
 * @param items Descriptor array
 * @param count Array length
 * @return TRUE if any value changed
 * 
 * Why: Centralizes reload logic. File watcher posts WM_APP on change detection.
 */
static BOOL ReloadConfigItems(HWND hwnd, const ConfigItem* items, size_t count) {
    BOOL anyChanged = FALSE;
    BOOL needsRedraw = FALSE;
    
    for (size_t i = 0; i < count; i++) {
        const ConfigItem* item = &items[i];
        BOOL changed = FALSE;
        
        if (item->customLoader) {
            changed = item->customLoader(item->section, item->key, item->target, item->defaultValue);
        } else {
            switch (item->type) {
                case CONFIG_TYPE_STRING:
                    changed = LoadConfigString(item->section, item->key, item->target, 
                                              item->targetSize, (const char*)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_INT:
                    changed = LoadConfigInt(item->section, item->key, item->target, 
                                           (int)(intptr_t)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_BOOL:
                    changed = LoadConfigBool(item->section, item->key, item->target, 
                                            (BOOL)(intptr_t)item->defaultValue);
                    break;
                    
                case CONFIG_TYPE_FLOAT:
                    changed = LoadConfigFloat(item->section, item->key, item->target, 
                                             *(float*)&item->defaultValue);
                    break;
                    
                default:
                    break;
            }
        }
        
        if (changed) {
            anyChanged = TRUE;
            if (item->triggerRedraw) {
                needsRedraw = TRUE;
            }
        }
    }
    
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    return anyChanged;
}

/** Config descriptor macros - reduce boilerplate in reload handlers */
#define CFG_STR(sec, key, var, def) \
    {CONFIG_TYPE_STRING, sec, key, (void*)var, sizeof(var), (void*)def, NULL, TRUE}

#define CFG_STR_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_STRING, sec, key, (void*)var, sizeof(var), (void*)def, NULL, FALSE}

#define CFG_INT(sec, key, var, def) \
    {CONFIG_TYPE_INT, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, TRUE}

#define CFG_INT_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_INT, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, FALSE}

#define CFG_BOOL(sec, key, var, def) \
    {CONFIG_TYPE_BOOL, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, TRUE}

#define CFG_BOOL_NOREDRAW(sec, key, var, def) \
    {CONFIG_TYPE_BOOL, sec, key, (void*)&var, sizeof(var), (void*)(intptr_t)def, NULL, FALSE}

#define CFG_CUSTOM(sec, key, var, def, loader, redraw) \
    {CONFIG_TYPE_CUSTOM, sec, key, (void*)var, 0, (void*)def, loader, redraw}

/* ============================================================================
 * Preview System - Now provided by menu_preview.h/menu_preview.c
 * ============================================================================ */

/* All preview functions are now in preview.c */

/* ============================================================================
 * External Function Declarations
 * ============================================================================ */

extern BOOL ShowInputDialog(HWND hwnd, wchar_t* text);
extern BOOL ParseTimeInput(const char* input, int* outSeconds);
extern void InitializePomodoro(void);
extern void StopNotificationSound(void);
extern void ReadNotificationTypeConfig(void);
extern INT_PTR ShowFontLicenseDialog(HWND hwnd);
extern BOOL LoadFontByNameAndGetRealName(HINSTANCE hInst, const char* name, char* out, size_t size);
extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInst);
extern void ReadPercentIconColorsConfig(void);
extern void ReloadAnimationSpeedFromConfig(void);
extern void UpdateTrayIcon(HWND hwnd);

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BUFFER_SIZE_FONT_NAME 256
#define BUFFER_SIZE_TIME_TEXT 50
#define BUFFER_SIZE_CLI_INPUT 256
#define BUFFER_SIZE_MENU_ITEM 100
#define MAX_ANIMATION_MENU_ITEMS 1000
#define OPACITY_FULL 255

#define IDT_MENU_DEBOUNCE 500
#define MENU_DEBOUNCE_DELAY_MS 50

#define CMD_QUICK_COUNTDOWN_BASE 102
#define CMD_QUICK_COUNTDOWN_END 108
#define CMD_COLOR_OPTIONS_BASE 201
#define CMD_POMODORO_TIME_BASE 600
#define CMD_POMODORO_TIME_END 609
#define CMD_FONT_SELECTION_BASE 2000
#define CMD_FONT_SELECTION_END 3000

/* ============================================================================
 * Recursive File Finder System - Builds font/animation menus from folders
 * ============================================================================ */

typedef struct {
    wchar_t name[MAX_PATH];
    char relPathUtf8[MAX_PATH];
    BOOL isDir;
} FileEntry;

typedef BOOL (*FileFilterFunc)(const wchar_t* filename);
typedef BOOL (*FileActionFunc)(const char* relPath, void* userData);

/**
 * @brief qsort comparator: directories first, then natural sort
 * @note Uses NaturalCompareW from utils/natural_sort.h
 */
static int CompareFileEntries(const void* a, const void* b) {
    const FileEntry* ea = (const FileEntry*)a;
    const FileEntry* eb = (const FileEntry*)b;
    if (ea->isDir != eb->isDir) return eb->isDir - ea->isDir;
    return NaturalCompareW(ea->name, eb->name);
}

/**
 * @brief Recursively map menu ID to file path
 * @param rootPathW Root directory (wide-char)
 * @param relPathUtf8 Current relative path (for nested dirs)
 * @param filter File extension filter (NULL = accept all)
 * @param targetId Menu ID to find
 * @param currentId ID counter (increments per file, find by index)
 * @param action Callback executed when targetId matches currentId
 * @param userData Context for callback
 * @return TRUE if found and callback succeeded
 * 
 * Why: Dynamic menus assign sequential IDs (2000, 2001...) to files.
 * This reverses the mapping when user clicks menu item.
 */
static BOOL RecursiveFindFile(const wchar_t* rootPathW, const char* relPathUtf8,
                              FileFilterFunc filter, UINT targetId, UINT* currentId,
                              FileActionFunc action, void* userData) {
    FileEntry* entries = (FileEntry*)malloc(sizeof(FileEntry) * MAX_ANIMATION_FRAMES);
    if (!entries) return FALSE;
    
    int count = 0;
    wchar_t searchPath[MAX_PATH];
    wcscpy_s(searchPath, MAX_PATH, rootPathW);
    PathJoinW(searchPath, MAX_PATH, L"*");
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(entries);
        return FALSE;
    }
    
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        if (count >= MAX_ANIMATION_FRAMES) break;
        
        FileEntry* e = &entries[count];
        e->isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        wcsncpy_s(e->name, MAX_PATH, ffd.cFileName, _TRUNCATE);
        
        char nameUtf8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, nameUtf8, MAX_PATH, NULL, NULL);
        
        if (relPathUtf8 && relPathUtf8[0]) {
            snprintf(e->relPathUtf8, MAX_PATH, "%s\\%s", relPathUtf8, nameUtf8);
        } else {
            strncpy_s(e->relPathUtf8, MAX_PATH, nameUtf8, _TRUNCATE);
        }
        
        if (!e->isDir && filter && !filter(ffd.cFileName)) continue;
        count++;
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    if (count == 0) {
        free(entries);
        return FALSE;
    }
    
    qsort(entries, count, sizeof(FileEntry), CompareFileEntries);
    
    for (int i = 0; i < count; i++) {
        FileEntry* e = &entries[i];
        
        if (e->isDir) {
            wchar_t subPath[MAX_PATH];
            wcscpy_s(subPath, MAX_PATH, rootPathW);
            PathJoinW(subPath, MAX_PATH, e->name);
            
            if (RecursiveFindFile(subPath, e->relPathUtf8, filter, targetId, currentId, action, userData)) {
                free(entries);
                return TRUE;
            }
        } else {
            if (*currentId == targetId) {
                BOOL result = action(e->relPathUtf8, userData);
                free(entries);
                return result;
            }
            (*currentId)++;
        }
    }
    
    free(entries);
    return FALSE;
}

/**
 * Check if filename matches any extension in array (case-insensitive).
 */
static BOOL MatchExtension(const wchar_t* filename, const wchar_t** exts, size_t count) {
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (!ext) return FALSE;
    for (size_t i = 0; i < count; i++) {
        if (_wcsicmp(ext, exts[i]) == 0) return TRUE;
    }
    return FALSE;
}

static const wchar_t* ANIMATION_EXTS[] = {
    L".gif", L".webp", L".ico", L".png", L".bmp", 
    L".jpg", L".jpeg", L".tif", L".tiff"
};
static const wchar_t* FONT_EXTS[] = {L".ttf", L".otf"};

static BOOL IsAnimationFile(const wchar_t* filename) {
    return MatchExtension(filename, ANIMATION_EXTS, ARRAY_SIZE(ANIMATION_EXTS));
}

static BOOL IsFontFile(const wchar_t* filename) {
    return MatchExtension(filename, FONT_EXTS, ARRAY_SIZE(FONT_EXTS));
}

static BOOL AnimationPreviewAction(const char* relPath, void* userData) {
    (void)userData;
    StartAnimationPreview(relPath);
    return TRUE;
}

static BOOL FontLoadAction(const char* relPath, void* userData) {
    HWND hwnd = (HWND)userData;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    char fontPath[MAX_PATH];
    strncpy_s(fontPath, MAX_PATH, relPath, _TRUNCATE);
    if (SwitchFont(hInstance, fontPath)) {
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return TRUE;
    }
    return FALSE;
}

/**
 * Map animation menu ID to file and start preview.
 */
static BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* relPathUtf8, 
                                      UINT* nextIdPtr, UINT targetId) {
    return RecursiveFindFile(folderPathW, relPathUtf8, IsAnimationFile, 
                           targetId, nextIdPtr, AnimationPreviewAction, NULL);
}

/* ============================================================================
 * Font Menu Builder
 * ============================================================================ */

typedef struct {
    wchar_t relPath[MAX_PATH];
    HWND hwnd;
} FontFindData;

static BOOL FontPreviewAction(const char* relPath, void* userData) {
    if (!userData) return FALSE;
    wchar_t relPathW[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, relPath, -1, relPathW, MAX_PATH);
    wcsncpy_s(((FontFindData*)userData)->relPath, MAX_PATH, relPathW, _TRUNCATE);
    return TRUE;
}

/**
 * Map font menu ID back to file path using recursive search.
 */
static BOOL FindFontByIdRecursiveW(const wchar_t* folderPathW, int targetId, int* currentId,
                                   wchar_t* foundRelativePathW, const wchar_t* fontsFolderRootW) {
    (void)fontsFolderRootW;
    
    FontFindData data = {0};
    UINT id = (UINT)*currentId;
    
    if (RecursiveFindFile(folderPathW, "", IsFontFile, (UINT)targetId, &id, FontPreviewAction, &data)) {
        wcsncpy_s(foundRelativePathW, MAX_PATH, data.relPath, _TRUNCATE);
        *currentId = (int)id;
        return TRUE;
    }
    
    *currentId = (int)id;
    return FALSE;
}

/* ============================================================================
 * Timer Mode Switching - Unified state machine for 4 timer modes
 * ============================================================================ */

typedef enum {
    TIMER_MODE_COUNTDOWN,
    TIMER_MODE_COUNTUP,
    TIMER_MODE_SHOW_TIME,
    TIMER_MODE_POMODORO
} TimerMode;

typedef struct {
    int totalSeconds;
    BOOL resetElapsed;
    BOOL showWindow;
    BOOL resetInterval;
} TimerModeParams;

/**
 * @brief Switch timer mode with unified state management
 * @param hwnd Window handle
 * @param mode Target mode
 * @param params Mode-specific parameters (NULL for defaults)
 * @return TRUE if switched successfully
 * 
 * Why centralized: Previous code had duplicate state management in 6+ places.
 * This ensures consistent transitions and timer interval updates.
 */
static BOOL SwitchTimerMode(HWND hwnd, TimerMode mode, const TimerModeParams* params) {
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    TimerModeParams defaultParams = {0, TRUE, FALSE, TRUE};
    if (!params) params = &defaultParams;
    
    CLOCK_SHOW_CURRENT_TIME = (mode == TIMER_MODE_SHOW_TIME);
    CLOCK_COUNT_UP = (mode == TIMER_MODE_COUNTUP);
    CLOCK_IS_PAUSED = FALSE;
    
    if (params->resetElapsed) {
        elapsed_time = 0;
        countdown_elapsed_time = 0;
        countup_elapsed_time = 0;
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        ResetMillisecondAccumulator();
    }
    
    if (mode == TIMER_MODE_COUNTDOWN || mode == TIMER_MODE_POMODORO) {
        CLOCK_TOTAL_TIME = params->totalSeconds;
    }
    
    if (params->showWindow) {
        ShowWindow(hwnd, SW_SHOW);
    }
    
    if (params->resetInterval && (wasShowingTime || mode == TIMER_MODE_SHOW_TIME)) {
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

/* ============================================================================
 * Configuration Update System
 * ============================================================================ */

static inline void UpdateConfigWithRedraw(HWND hwnd, const char* key, const char* value, BOOL needsRedraw) {
    WriteConfigKeyValue(key, value);
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * Toggle boolean config value and optionally redraw.
 */
static inline void ToggleConfigBool(HWND hwnd, const char* key, BOOL* currentValue, BOOL needsRedraw) {
    *currentValue = !(*currentValue);
    WriteConfigKeyValue(key, *currentValue ? "TRUE" : "FALSE");
    if (needsRedraw && hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

static inline void WriteConfigAndRedraw(HWND hwnd, const char* key, const char* value) {
    WriteConfigKeyValue(key, value);
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
}

/* ============================================================================
 * Dialog Systems
 * ============================================================================ */

/**
 * @brief Show Windows file open dialog
 * @param hwnd Parent window
 * @param selectedPath Output buffer for UTF-8 path
 * @param bufferSize Buffer capacity
 * @return TRUE if user selected file, FALSE if cancelled
 */
static BOOL ShowFilePicker(HWND hwnd, char* selectedPath, size_t bufferSize) {
    wchar_t szFile[MAX_PATH] = {0};
    
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, selectedPath, (int)bufferSize, NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Timeout Action Setters
 * ============================================================================ */

/**
 * @brief Set timeout action configuration
 * @param action Action string ("MESSAGE", "LOCK", "SHOW_TIME", etc.)
 * 
 * Eliminates 7 duplicate implementations.
 */
static inline void SetTimeoutAction(const char* action) {
    WriteConfigTimeoutAction(action);
}

/* ============================================================================
 * Utility Helpers - Static Functions
 * ============================================================================ */

/**
 * @brief Get fonts folder path in wide-char format
 * @param out Output buffer for wide-char path
 * @param size Buffer size in wchar_t units
 * @return TRUE if path retrieved successfully
 */
static BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size) {
    if (!out || size == 0) return FALSE;
    
    WideString ws = ToWide(GetCachedConfigPath());
    if (!ws.valid) return FALSE;
    wchar_t wConfigPath[MAX_PATH];
    wcscpy_s(wConfigPath, MAX_PATH, ws.buf);
    
    wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
    if (!lastSep) return FALSE;
    *lastSep = L'\0';
    
    _snwprintf_s(out, size, _TRUNCATE, L"%s\\resources\\fonts", wConfigPath);
    return TRUE;
}

/**
 * @brief Check if wide-char string is NULL, empty or whitespace-only
 * @param str String to check (can be NULL)
 * @return TRUE if string is NULL/empty/whitespace-only, FALSE otherwise
 */
static inline BOOL isAllSpacesOnly(const wchar_t* str) {
    if (!str || str[0] == L'\0') return TRUE;
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * Preview Management - Unified System Implementation (v5.0)
 * ============================================================================ */

/* Preview functions now implemented in preview.c */

/* ============================================================================
 * File Validation - Static Helpers
 * ============================================================================ */

/**
 * @brief Validate file + update config + save to recent files
 * @param hwnd Parent for error dialog
 * @param filePathUtf8 Path to validate
 * @return TRUE if file exists, FALSE otherwise
 * 
 * Why combined: File selection requires all 3 operations together.
 */
static BOOL ValidateAndSetTimeoutFile(HWND hwnd, const char* filePathUtf8) {
    if (!filePathUtf8 || filePathUtf8[0] == '\0') {
        return FALSE;
    }
    
    WideString ws = ToWide(filePathUtf8);
    if (!ws.valid) return FALSE;
    
    if (GetFileAttributesW(ws.buf) != INVALID_FILE_ATTRIBUTES) {
        WriteConfigTimeoutFile(filePathUtf8);
        SaveRecentFile(filePathUtf8);
        return TRUE;
    } else {
        if (hwnd) {
            MessageBoxW(hwnd, 
                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                GetLocalizedString(L"错误", L"Error"),
                MB_ICONERROR);
        }
        return FALSE;
    }
}

/* ============================================================================
 * Input Dialog - Static Functions
 * ============================================================================ */

/* ============================================================================
 * Unified Input Validation Framework - Retry loop until valid or cancelled
 * ============================================================================ */

/** Validator returns TRUE if input valid, writes to output buffer */
typedef BOOL (*InputValidator)(const char* input, void* output);

/**
 * @brief Show input dialog with validation retry loop
 * @param hwnd Parent window
 * @param dialogId Dialog resource ID
 * @param validator Validation callback
 * @param output Buffer for validated result
 * @return TRUE if user provided valid input, FALSE if cancelled
 * 
 * Why: Eliminates 5+ duplicate "show dialog -> validate -> retry" patterns.
 * Works with any validator by accepting callback.
 */
static BOOL ValidatedInputLoop(HWND hwnd, UINT dialogId, 
                              InputValidator validator, void* output) {
    while (1) {
        memset(inputText, 0, sizeof(inputText));
        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(dialogId), 
                       hwnd, DlgProc, (LPARAM)dialogId);
        
        if (inputText[0] == L'\0' || isAllSpacesOnly(inputText)) {
            return FALSE;
        }
        
        Utf8String us = ToUtf8(inputText);
        if (!us.valid) {
            ShowErrorDialog(hwnd);
            continue;
        }
        char inputTextA[256];
        strcpy_s(inputTextA, sizeof(inputTextA), us.buf);
        
        if (validator(inputTextA, output)) {
            return TRUE;
        } else {
            ShowErrorDialog(hwnd);
        }
    }
}

/** Time input validator: accepts "5m", "1h30m", "90s", etc. */
static BOOL ValidateTimeInput(const char* input, void* output) {
    return ParseInput(input, (int*)output);
}

/** Legacy wrapper for time input dialogs */
static BOOL ValidatedTimeInputLoop(HWND hwnd, UINT dialogId, int* outSeconds) {
    int result = 0;
    if (ValidatedInputLoop(hwnd, dialogId, ValidateTimeInput, &result)) {
        if (outSeconds) *outSeconds = result;
        return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Input Dialog - Static Functions
 * ============================================================================ */

/**
 * @brief Dialog procedure for custom input box
 * @param hwndDlg Dialog window handle
 * @param uMsg Message identifier
 * @param wParam Message parameter
 * @param lParam InputBoxParams pointer on WM_INITDIALOG
 * @return Dialog message processing result
 * 
 * Handles text input with default value and OK/Cancel response.
 */
static INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            /** Extract parameters and initialize dialog controls */
            InputBoxParams* params = (InputBoxParams*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            SetWindowTextW(hwndDlg, params->title);
            
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            
            /** Select all text for easy replacement */
            SendDlgItemMessageW(hwndDlg, IDC_EDIT_INPUT, EM_SETSEL, 0, -1);
            
            SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_INPUT));
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    /** Retrieve user input and close dialog */
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                }
                
                case IDCANCEL:
                    /** Cancel operation without saving input */
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Show modal input dialog with customizable parameters
 * @param hwndParent Parent window handle
 * @param title Dialog title text
 * @param prompt User prompt message
 * @param defaultText Pre-filled input value
 * @param result Output buffer for user input
 * @param maxLen Maximum result buffer length
 * @return TRUE if OK clicked, FALSE if cancelled
 * 
 * Displays modal dialog for text input with pre-filled default value.
 */
static BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    InputBoxParams params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}


/* ============================================================================
 * WM_APP Message Handlers - Configuration Reload Subsystem
 * ============================================================================ */

/**
 * @brief Custom loader for window position and scale (requires special handling)
 */
static BOOL LoadDisplayWindowSettings(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    if (CLOCK_EDIT_MODE) return FALSE;  /* Don't update in edit mode */
    
    int posX = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_X, CLOCK_WINDOW_POS_X);
    int posY = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_Y, CLOCK_WINDOW_POS_Y);
    char scaleStr[16];
    ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_SCALE, "1.62", scaleStr, sizeof(scaleStr));
    float newScale = (float)atof(scaleStr);
    BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
    
    BOOL changed = FALSE;
    BOOL posChanged = (posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y);
    BOOL scaleChanged = (newScale > 0.0f && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f);
    
    if (scaleChanged) {
        extern float CLOCK_FONT_SCALE_FACTOR;
        CLOCK_WINDOW_SCALE = newScale;
        CLOCK_FONT_SCALE_FACTOR = newScale;
        changed = TRUE;
    }
    
    if (posChanged || scaleChanged) {
        HWND hwnd = *(HWND*)target;
        SetWindowPos(hwnd, NULL, posX, posY,
                    (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
                    (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
                    SWP_NOZORDER | SWP_NOACTIVATE);
        CLOCK_WINDOW_POS_X = posX;
        CLOCK_WINDOW_POS_Y = posY;
        changed = TRUE;
    }
    
    if (newTopmost != CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(*(HWND*)target, newTopmost);
        changed = TRUE;
    }
    
    return changed;
}

/** WM_APP_DISPLAY_CHANGED handler */
CONFIG_RELOAD_HANDLER(Display) {
    ConfigItem items[] = {
        CFG_STR(CFG_SECTION_DISPLAY, CFG_KEY_TEXT_COLOR, CLOCK_TEXT_COLOR, CLOCK_TEXT_COLOR),
        CFG_INT(CFG_SECTION_DISPLAY, CFG_KEY_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_DISPLAY, NULL, (void*)&hwnd, 0, NULL, LoadDisplayWindowSettings, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

/* ============================================================================
 * Generic Enum System - X-Macro generates ToStr/FromStr/Loader
 * ============================================================================ */

/** X-Macro enum → string mappings */
#define TIME_FORMAT_MAP X(TIME_FORMAT_DEFAULT, "DEFAULT") X(TIME_FORMAT_ZERO_PADDED, "ZERO_PADDED") X(TIME_FORMAT_FULL_PADDED, "FULL_PADDED")

#define TIMEOUT_ACTION_MAP \
    X(TIMEOUT_ACTION_MESSAGE, STR_MESSAGE) X(TIMEOUT_ACTION_LOCK, "LOCK") \
    X(TIMEOUT_ACTION_OPEN_FILE, "OPEN_FILE") X(TIMEOUT_ACTION_SHOW_TIME, "SHOW_TIME") \
    X(TIMEOUT_ACTION_COUNT_UP, "COUNT_UP") X(TIMEOUT_ACTION_OPEN_WEBSITE, "OPEN_WEBSITE") \
    X(TIMEOUT_ACTION_SLEEP, "SLEEP") X(TIMEOUT_ACTION_SHUTDOWN, "SHUTDOWN") X(TIMEOUT_ACTION_RESTART, "RESTART")

/* Phase 1: enum → string */
#define X(val, name) case val: return name;
static const char* TimeFormatTypeToStr(TimeFormatType val) { 
    switch(val) { TIME_FORMAT_MAP default: return STR_DEFAULT; } 
}
static const char* TimeoutActionTypeToStr(TimeoutActionType val) { 
    switch(val) { TIMEOUT_ACTION_MAP default: return STR_MESSAGE; } 
}
#undef X

/* Phase 2: string → enum */
#define X(val, name) if (strcmp(str, name) == 0) return val;
static TimeFormatType TimeFormatTypeFromStr(const char* str) { 
    if (!str) return TIME_FORMAT_DEFAULT; 
    TIME_FORMAT_MAP 
    return TIME_FORMAT_DEFAULT; 
}
static TimeoutActionType TimeoutActionTypeFromStr(const char* str) { 
    if (!str) return TIMEOUT_ACTION_MESSAGE; 
    TIMEOUT_ACTION_MAP 
    return TIMEOUT_ACTION_MESSAGE; 
}
#undef X

/* Phase 3: config loaders */
#define ENUM_LOADER(EnumType, FromStrFunc, defaultVal) \
    static BOOL Load##EnumType(const char* sec, const char* key, void* target, const void* def) { \
        char buf[32]; \
        ReadConfigStr(sec, key, (const char*)def, buf, sizeof(buf)); \
        EnumType newVal = FromStrFunc(buf); \
        if (newVal != *(EnumType*)target) { *(EnumType*)target = newVal; return TRUE; } \
        return FALSE; \
    }

ENUM_LOADER(TimeFormatType, TimeFormatTypeFromStr, TIME_FORMAT_DEFAULT)
ENUM_LOADER(TimeoutActionType, TimeoutActionTypeFromStr, TIMEOUT_ACTION_MESSAGE)

/** Milliseconds loader - restarts timer when changed */
static BOOL LoadShowMilliseconds(const char* section, const char* key, void* target, const void* def) {
    BOOL temp = ReadConfigBool(section, key, (BOOL)(intptr_t)def);
    if (temp != *(BOOL*)target) {
        *(BOOL*)target = temp;
        HWND hwnd = GetActiveWindow();
        if (hwnd) RESTART_TIMER_INTERVAL(hwnd);
        return TRUE;
    }
    return FALSE;
}


static BOOL LoadTimeoutWebsite(const char* section, const char* key, void* target, const void* def) {
    char buffer[MAX_PATH];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    WideString ws = ToWide(buffer);
    if (!ws.valid && buffer[0]) ws.buf[0] = L'\0';
    if (wcscmp(ws.buf, (wchar_t*)target) != 0) {
        wcsncpy_s((wchar_t*)target, MAX_PATH, ws.buf, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * List Loader System
 * ============================================================================ */

/** Generate loader for comma-separated integer lists */
#define INT_LIST_LOADER(name, targetArr, targetCnt, maxCnt) \
    static BOOL Load##name(const char* sec, const char* key, void* target, const void* def) { \
        (void)target; \
        char buf[256]; \
        ReadConfigStr(sec, key, (const char*)def, buf, sizeof(buf)); \
        int newArr[maxCnt] = {0}, newCnt = 0; \
        char* tok = strtok(buf, ","); \
        while (tok && newCnt < maxCnt) { while (*tok == ' ') tok++; newArr[newCnt++] = atoi(tok); tok = strtok(NULL, ","); } \
        BOOL changed = (newCnt != targetCnt); \
        if (!changed) { for (int i = 0; i < newCnt; i++) { if (newArr[i] != targetArr[i]) { changed = TRUE; break; } } } \
        if (changed) { targetCnt = newCnt; memcpy(targetArr, newArr, newCnt * sizeof(int)); } \
        return changed; \
    }

INT_LIST_LOADER(TimeOptions, time_options, time_options_count, MAX_TIME_OPTIONS)

/** WM_APP_TIMER_CHANGED handler */
CONFIG_RELOAD_HANDLER(Timer) {
    ConfigItem items[] = {
        CFG_BOOL(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR, CLOCK_USE_24HOUR, CLOCK_USE_24HOUR),
        CFG_BOOL(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS, CLOCK_SHOW_SECONDS, CLOCK_SHOW_SECONDS),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIME_FORMAT, (void*)&CLOCK_TIME_FORMAT, 0, "DEFAULT", LoadTimeFormatType, TRUE},
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_SHOW_MILLISECONDS, (void*)&CLOCK_SHOW_MILLISECONDS, 0, (void*)FALSE, LoadShowMilliseconds, TRUE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_TEXT, CLOCK_TIMEOUT_TEXT, "0"),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_ACTION, (void*)&CLOCK_TIMEOUT_ACTION, 0, "MESSAGE", LoadTimeoutActionType, FALSE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_FILE, CLOCK_TIMEOUT_FILE_PATH, ""),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_WEBSITE, (void*)CLOCK_TIMEOUT_WEBSITE_URL, 0, "", LoadTimeoutWebsite, FALSE},
        CFG_INT_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_DEFAULT_START_TIME, CLOCK_DEFAULT_START_TIME, CLOCK_DEFAULT_START_TIME),
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_TIMER, CFG_KEY_TIME_OPTIONS, (void*)time_options, 0, "1500,600,300", LoadTimeOptions, FALSE},
        CFG_STR_NOREDRAW(CFG_SECTION_TIMER, CFG_KEY_STARTUP_MODE, CLOCK_STARTUP_MODE, CLOCK_STARTUP_MODE)
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadPomodoroOptions(const char* section, const char* key, void* target, const void* def) {
    (void)target;
    extern int POMODORO_WORK_TIME, POMODORO_SHORT_BREAK, POMODORO_LONG_BREAK;
    
    char buf[128];
    ReadConfigStr(section, key, (const char*)def, buf, sizeof(buf));
    int tmp[3] = {POMODORO_WORK_TIME, POMODORO_SHORT_BREAK, POMODORO_LONG_BREAK}, cnt = 0;
    char* tok = strtok(buf, ",");
    while (tok && cnt < 3) { while (*tok == ' ') tok++; tmp[cnt++] = atoi(tok); tok = strtok(NULL, ","); }
    
    BOOL changed = FALSE;
    if (cnt > 0 && tmp[0] != POMODORO_WORK_TIME) { POMODORO_WORK_TIME = tmp[0]; changed = TRUE; }
    if (cnt > 1 && tmp[1] != POMODORO_SHORT_BREAK) { POMODORO_SHORT_BREAK = tmp[1]; changed = TRUE; }
    if (cnt > 2 && tmp[2] != POMODORO_LONG_BREAK) { POMODORO_LONG_BREAK = tmp[2]; changed = TRUE;
    }
    
    return changed;
}

static BOOL LoadPomodoroLoopCount(const char* section, const char* key, void* target, const void* def) {
    int temp = ReadConfigInt(section, key, (int)(intptr_t)def);
    if (temp < 1) temp = 1;
    if (temp != *(int*)target) {
        *(int*)target = temp;
        return TRUE;
    }
    return FALSE;
}

/** WM_APP_POMODORO_CHANGED handler */
CONFIG_RELOAD_HANDLER(Pomodoro) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_POMODORO, CFG_KEY_POMODORO_OPTIONS, (void*)POMODORO_TIMES, 0, "1500,300,1500,600", LoadPomodoroOptions, FALSE},
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_POMODORO, CFG_KEY_POMODORO_LOOP_COUNT, (void*)&POMODORO_LOOP_COUNT, 0, (void*)1, LoadPomodoroLoopCount, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadNotificationSettings(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    ReadNotificationMessagesConfig();
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
    ReadNotificationTypeConfig();
    ReadNotificationSoundConfig();
    ReadNotificationVolumeConfig();
    ReadNotificationDisabledConfig();
    
    return FALSE;
}

/** WM_APP_NOTIFICATION_CHANGED handler */
CONFIG_RELOAD_HANDLER(Notification) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadNotificationSettings, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadHotkeys(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)def;
    RegisterGlobalHotkeys(*(HWND*)target);
    return FALSE;
}

/** WM_APP_HOTKEYS_CHANGED handler */
CONFIG_RELOAD_HANDLER(Hotkeys) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, (void*)&hwnd, 0, NULL, LoadHotkeys, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

/** Reload recent files + validate timeout file still exists */
static BOOL LoadRecentFilesConfig(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    
    extern void LoadRecentFiles(void);
    LoadRecentFiles();
    
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    extern char CLOCK_TIMEOUT_FILE_PATH[];
    
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE) {
        BOOL match = FALSE;
        for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; ++i) {
            if (strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0) {
                match = TRUE;
                break;
            }
        }
        
        if (match) {
            WideString ws = ToWide(CLOCK_TIMEOUT_FILE_PATH);
            if (!ws.valid || GetFileAttributesW(ws.buf) == INVALID_FILE_ATTRIBUTES) {
                match = FALSE;
            }
        }
        
        if (!match && CLOCK_RECENT_FILES_COUNT > 0) {
            WriteConfigTimeoutFile(CLOCK_RECENT_FILES[0].path);
        }
    }
    
    return FALSE;
}

/** WM_APP_RECENTFILES_CHANGED handler */
CONFIG_RELOAD_HANDLER(RecentFiles) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadRecentFilesConfig, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadColorOptions(const char* section, const char* key, void* target, const void* def) {
    (void)target;
    char buffer[1024];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    
    ClearColorOptions();
    char* tok = strtok(buffer, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        AddColorOption(tok);
        tok = strtok(NULL, ",");
    }
    
    ReadPercentIconColorsConfig();
    TrayAnimation_UpdatePercentIconIfNeeded();
    
    return TRUE;
}

/** WM_APP_COLORS_CHANGED handler */
CONFIG_RELOAD_HANDLER(Colors) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, CFG_SECTION_COLORS, "COLOR_OPTIONS", NULL, 0,
         DEFAULT_COLOR_OPTIONS_INI,
         LoadColorOptions, TRUE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadAnimSpeed(const char* section, const char* key, void* target, const void* def) {
    (void)section; (void)key; (void)target; (void)def;
    ReloadAnimationSpeedFromConfig();
    TrayAnimation_RecomputeTimerDelay();
    return FALSE;
}

/** WM_APP_ANIM_SPEED_CHANGED handler */
CONFIG_RELOAD_HANDLER(AnimSpeed) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, NULL, NULL, NULL, 0, NULL, LoadAnimSpeed, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

static BOOL LoadAnimPath(const char* section, const char* key, void* target, const void* def) {
    char buffer[MAX_PATH];
    ReadConfigStr(section, key, (const char*)def, buffer, sizeof(buffer));
    ApplyAnimationPathValueNoPersist(buffer);
    return FALSE;
}

/** WM_APP_ANIM_PATH_CHANGED handler */
CONFIG_RELOAD_HANDLER(AnimPath) {
    ConfigItem items[] = {
        {CONFIG_TYPE_CUSTOM, "Animation", "ANIMATION_PATH", NULL, 0, "__logo__", LoadAnimPath, FALSE}
    };
    
    ReloadConfigItems(hwnd, items, ARRAY_SIZE(items));
    return 0;
}

/* ============================================================================
 * Command Handler Functions
 * ============================================================================ */

static LRESULT CmdCustomCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        CLOCK_LAST_TIME_UPDATE = 0;
        KillTimer(hwnd, 1);
    }
    
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_DIALOG1, &total_seconds)) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, total_seconds);
    }
    return 0;
}

static LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(hwnd, wp, lp);
    RemoveTrayIcon();
    PostQuitMessage(0);
    return 0;
}

CMD(PauseResume, 0, TogglePauseResumeTimer(hwnd))
CMD(RestartTimer, 1, CloseAllNotifications(); RestartCurrentTimer(hwnd))
CMD(About, 0, ShowAboutDialog(hwnd))

static LRESULT CmdToggleTopmost(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(hwnd, wp, lp);
    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
    WriteConfigTopmost(newTopmost ? STR_TRUE : STR_FALSE);
    return 0;
}

CMD(TimeFormatDefault, 0, WRITE_CFG_REFRESH(WriteConfigTimeFormat, TIME_FORMAT_DEFAULT))
CMD(TimeFormatZeroPadded, 0, WRITE_CFG_REFRESH(WriteConfigTimeFormat, TIME_FORMAT_ZERO_PADDED))
CMD(TimeFormatFullPadded, 0, WRITE_CFG_REFRESH(WriteConfigTimeFormat, TIME_FORMAT_FULL_PADDED))
CMD(ToggleMilliseconds, 0, WRITE_CFG_REFRESH(WriteConfigShowMilliseconds, !CLOCK_SHOW_MILLISECONDS))

static LRESULT CmdCountdownReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    if (CLOCK_COUNT_UP) CLOCK_COUNT_UP = FALSE;
    ResetTimer();
    RESTART_TIMER_INTERVAL(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

CMD(EditMode, 0, if (CLOCK_EDIT_MODE) EndEditMode(hwnd); else StartEditMode(hwnd); InvalidateRect(hwnd, NULL, TRUE))
CMD(ToggleVisibility, 0, PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0))

static LRESULT CmdCustomizeColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    COLORREF color = ShowColorDialog(hwnd);
    if (color != (COLORREF)-1) {
        char hex_color[10];
        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                GetRValue(color), GetGValue(color), GetBValue(color));
        WriteConfigColor(hex_color);
    }
    return 0;
}

static LRESULT CmdFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    
    if (ShowFontLicenseDialog(hwnd) == IDOK) {
        SetFontLicenseAccepted(TRUE);
        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdFontAdvanced(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    
    char configPathUtf8[MAX_PATH];
    GetConfigPath(configPathUtf8, MAX_PATH);
    
    WideString wsConfig = ToWide(configPathUtf8);
    if (!wsConfig.valid) return 0;
    
    wchar_t* lastSep = wcsrchr(wsConfig.buf, L'\\');
    if (lastSep) {
        *lastSep = L'\0';
        wchar_t wFontsFolderPath[MAX_PATH];
        _snwprintf_s(wFontsFolderPath, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts", wsConfig.buf);
        SHCreateDirectoryExW(NULL, wFontsFolderPath, NULL);
        ShellExecuteW(hwnd, L"open", wFontsFolderPath, NULL, NULL, SW_SHOWNORMAL);
    }
    return 0;
}

static LRESULT CmdShowCurrentTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        TimerModeParams params = TIMER_PARAMS_SHOW_TIME();
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
    } else {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    }
    return 0;
}

CMD(24HourFormat, 0, ToggleConfigBool(hwnd, CFG_KEY_USE_24HOUR, &CLOCK_USE_24HOUR, TRUE))
CMD(ShowSeconds, 0, ToggleConfigBool(hwnd, CFG_KEY_SHOW_SECONDS, &CLOCK_SHOW_SECONDS, TRUE))

static LRESULT CmdCountUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    
    if (!CLOCK_COUNT_UP) {
        TimerModeParams params = TIMER_PARAMS_COUNTUP();
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
    } else {
        CLOCK_COUNT_UP = FALSE;
        RESTART_TIMER_INTERVAL(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdCountUpStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    
    if (!CLOCK_COUNT_UP) {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
    } else {
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

CMD(CountUpReset, 1, ResetTimer(); InvalidateRect(hwnd, NULL, TRUE))

static LRESULT CmdAutoStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    BOOL isEnabled = IsAutoStartEnabled();
    if (isEnabled) {
        if (RemoveShortcut()) {
            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
        }
    } else {
        if (CreateShortcut()) {
            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
        }
    }
    return 0;
}

CMD(ColorDialog, 0, DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG), hwnd, (DLGPROC)ColorDlgProc))
CMD(ColorPanel, 0, if (ShowColorDialog(hwnd) != (COLORREF)-1) InvalidateRect(hwnd, NULL, TRUE))

static LRESULT CmdPomodoroStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    
    if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOW);
    
    TimerModeParams params = TIMER_PARAMS_COUNTDOWN(POMODORO_WORK_TIME);
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    
    InitializePomodoro();
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdPomodoroReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    CleanupBeforeTimerAction();
    
    ResetTimer();
    
    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME || 
        CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK || 
        CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK) {
        RESTART_TIMER_INTERVAL(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

CMD(PomodoroLoopCount, 0, ShowPomodoroLoopDialog(hwnd))
CMD(PomodoroCombo, 0, ShowPomodoroComboDialog(hwnd))
CMD(OpenWebsite, 0, ShowWebsiteDialog(hwnd))
CMD(NotificationContent, 0, ShowNotificationMessagesDialog(hwnd))
CMD(NotificationDisplay, 0, ShowNotificationDisplayDialog(hwnd))
CMD(NotificationSettings, 0, ShowNotificationSettingsDialog(hwnd))
CMD(CheckUpdate, 0, CheckForUpdateAsync(hwnd, FALSE))
CMD(HotkeySettings, 0, ShowHotkeySettingsDialog(hwnd); RegisterGlobalHotkeys(hwnd))

CMD(Help, 0, OpenUserGuide())
CMD(Support, 0, OpenSupportPage())
CMD(Feedback, 0, OpenFeedbackPage())

static inline LRESULT HandleStartupMode(HWND hwnd, const char* mode) {
    SetStartupMode(hwnd, mode);
    return 0;
}

static LRESULT CmdModifyTimeOptions(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    
    while (1) {
        ClearInputBuffer(inputText, sizeof(inputText));
        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG), 
                       NULL, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);
        
        if (isAllSpacesOnly(inputText)) break;
        
        Utf8String us = ToUtf8(inputText);
        char inputTextA[MAX_PATH];
        strcpy_s(inputTextA, sizeof(inputTextA), us.buf);
        
        char* token = strtok(inputTextA, " ");
        char options[256] = {0};
        int valid = 1, count = 0;
        
        while (token && count < MAX_TIME_OPTIONS) {
            int seconds = 0;
            if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                valid = 0;
                break;
            }
            
            if (count > 0) strcat_s(options, sizeof(options), ",");
            
            char secondsStr[32];
            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
            strcat_s(options, sizeof(options), secondsStr);
            count++;
            token = strtok(NULL, " ");
        }
        
        if (valid && count > 0) {
            CleanupBeforeTimerAction();
            WriteConfigTimeOptions(options);
            break;
        } else {
            ShowErrorDialog(hwnd);
        }
    }
    return 0;
}

static LRESULT CmdModifyDefaultTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        CleanupBeforeTimerAction();
        WriteConfigDefaultStartTime(total_seconds);
        WriteConfigStartupMode("COUNTDOWN");
    }
    return 0;
}

static LRESULT CmdSetCountdownTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        WriteConfigDefaultStartTime(total_seconds);
    }
    return HandleStartupMode(hwnd, "COUNTDOWN");
}

CMD(StartupShowTime, 0, return HandleStartupMode(hwnd, "SHOW_TIME"))
CMD(StartupCountUp, 0, return HandleStartupMode(hwnd, "COUNT_UP"))
CMD(StartupNoDisplay, 0, return HandleStartupMode(hwnd, "NO_DISPLAY"))

CMD(TimeoutShowTime, 0, WriteConfigTimeoutAction("SHOW_TIME"))
CMD(TimeoutCountUp, 0, WriteConfigTimeoutAction("COUNT_UP"))
CMD(TimeoutShowMessage, 0, WriteConfigTimeoutAction("MESSAGE"))
CMD(TimeoutLockScreen, 0, WriteConfigTimeoutAction("LOCK"))
CMD(TimeoutShutdown, 0, WriteConfigTimeoutAction("SHUTDOWN"))
CMD(TimeoutRestart, 0, WriteConfigTimeoutAction("RESTART"))
CMD(TimeoutSleep, 0, WriteConfigTimeoutAction("SLEEP"))

CMD(BrowseFile, 0, { \
    char utf8Path[MAX_PATH]; \
    if (ShowFilePicker(hwnd, utf8Path, sizeof(utf8Path))) \
        ValidateAndSetTimeoutFile(hwnd, utf8Path); \
})

/* ============================================================================
 * Reset Defaults
 * ============================================================================ */

/** Reset all timer state to default values */
static void ResetTimerStateToDefaults(void) {
    CLOCK_TOTAL_TIME = 25 * 60;
    elapsed_time = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    message_shown = FALSE;
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_IS_PAUSED = FALSE;
    
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    ResetTimer();
}

/** Detect system language via Windows API */
static AppLanguage DetectSystemLanguage(void) {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLangId = PRIMARYLANGID(langId);
    WORD subLangId = SUBLANGID(langId);
    
    switch (primaryLangId) {
        case LANG_CHINESE:
            return (subLangId == SUBLANG_CHINESE_SIMPLIFIED) ? 
                   APP_LANG_CHINESE_SIMP : APP_LANG_CHINESE_TRAD;
        case LANG_SPANISH: return APP_LANG_SPANISH;
        case LANG_FRENCH: return APP_LANG_FRENCH;
        case LANG_GERMAN: return APP_LANG_GERMAN;
        case LANG_RUSSIAN: return APP_LANG_RUSSIAN;
        case LANG_PORTUGUESE: return APP_LANG_PORTUGUESE;
        case LANG_JAPANESE: return APP_LANG_JAPANESE;
        case LANG_KOREAN: return APP_LANG_KOREAN;
        default: return APP_LANG_ENGLISH;
    }
}

/** Delete config.ini + create fresh one with defaults */
static void ResetConfigurationFile(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    /** Remove existing config if it exists */
    FILE* test = _wfopen(wconfig_path, L"r");
    if (test) {
        fclose(test);
        remove(config_path);
    }
    
    /** Create fresh default configuration */
    CreateDefaultConfig(config_path);
    
    /** Reload notification messages from new config */
    extern void ReadNotificationMessagesConfig(void);
    ReadNotificationMessagesConfig();
    
    /** Extract embedded fonts to resources folder */
    extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE);
    ExtractEmbeddedFontsToFolder(GetModuleHandle(NULL));
}

/** Reload font from config - handles %LOCALAPPDATA% prefix */
static void ReloadDefaultFont(void) {
    extern BOOL LoadFontByNameAndGetRealName(HINSTANCE, const char*, char*, size_t);
    
    char actualFontFileName[MAX_PATH];
    const char* localappdata_prefix = FONTS_PATH_PREFIX;
    
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
        LoadFontByNameAndGetRealName(GetModuleHandle(NULL), actualFontFileName, 
                                     FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
    }
}

/** Calculate window size (3% of screen height) */
static void RecalculateWindowSize(HWND hwnd) {
    CLOCK_WINDOW_SCALE = 1.0f;
    CLOCK_FONT_SCALE_FACTOR = 1.0f;
    
    HDC hdc = GetDC(hwnd);
    
    wchar_t fontNameW[BUFFER_SIZE_FONT_NAME];
    MultiByteToWideChar(CP_UTF8, 0, FONT_INTERNAL_NAME, -1, fontNameW, BUFFER_SIZE_FONT_NAME);
    
    HFONT hFont = CreateFontW(
        -CLOCK_BASE_FONT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, fontNameW
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    char time_text[BUFFER_SIZE_TIME_TEXT];
    FormatTime(CLOCK_TOTAL_TIME, time_text);
    
    wchar_t time_textW[BUFFER_SIZE_TIME_TEXT];
    MultiByteToWideChar(CP_UTF8, 0, time_text, -1, time_textW, BUFFER_SIZE_TIME_TEXT);
    
    SIZE textSize;
    GetTextExtentPoint32(hdc, time_textW, (int)wcslen(time_textW), &textSize);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hwnd, hdc);
    
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    float defaultScale = (screenHeight * 0.03f) / 20.0f;
    CLOCK_WINDOW_SCALE = defaultScale;
    CLOCK_FONT_SCALE_FACTOR = defaultScale;
    
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        (int)(textSize.cx * defaultScale), (int)(textSize.cy * defaultScale),
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

static LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    CleanupBeforeTimerAction();
    KillTimer(hwnd, 1);
    UnregisterGlobalHotkeys(hwnd);
    
    ResetTimerStateToDefaults();
    
    CLOCK_EDIT_MODE = FALSE;
    SetClickThrough(hwnd, TRUE);
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
    
    AppLanguage defaultLanguage = DetectSystemLanguage();
    if (CURRENT_LANGUAGE != defaultLanguage) {
        CURRENT_LANGUAGE = defaultLanguage;
    }
    ResetConfigurationFile();
    ReloadDefaultFont();
    
    InvalidateRect(hwnd, NULL, TRUE);
    RecalculateWindowSize(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    ResetTimerWithInterval(hwnd);
    
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    RegisterGlobalHotkeys(hwnd);
    
    return 0;
}

/* ============================================================================
 * Command Dispatch Table
 * ============================================================================ */

static const CommandDispatchEntry COMMAND_DISPATCH_TABLE[] = {
    {101, CmdCustomCountdown, "Custom countdown"},
    {109, CmdExit, "Exit application"},
    {200, CmdResetDefaults, "Reset to defaults"},
    
    {CLOCK_IDC_MODIFY_TIME_OPTIONS, CmdModifyTimeOptions, "Modify time options"},
    {CLOCK_IDC_MODIFY_DEFAULT_TIME, CmdModifyDefaultTime, "Modify default time"},
    {CLOCK_IDC_SET_COUNTDOWN_TIME, CmdSetCountdownTime, "Set countdown time"},
    {CLOCK_IDC_START_SHOW_TIME, CmdStartupShowTime, "Start show time"},
    {CLOCK_IDC_START_COUNT_UP, CmdStartupCountUp, "Start count up"},
    {CLOCK_IDC_START_NO_DISPLAY, CmdStartupNoDisplay, "Start no display"},
    {CLOCK_IDC_AUTO_START, CmdAutoStart, "Auto-start toggle"},
    {CLOCK_IDC_EDIT_MODE, CmdEditMode, "Edit mode toggle"},
    {CLOCK_IDC_TOGGLE_VISIBILITY, CmdToggleVisibility, "Toggle visibility"},
    {CLOCK_IDC_CUSTOMIZE_LEFT, CmdCustomizeColor, "Customize color"},
    {CLOCK_IDC_FONT_LICENSE_AGREE, CmdFontLicense, "Font license agree"},
    {CLOCK_IDC_FONT_ADVANCED, CmdFontAdvanced, "Advanced font selection"},
    {CLOCK_IDC_COLOR_VALUE, CmdColorDialog, "Color value dialog"},
    {CLOCK_IDC_COLOR_PANEL, CmdColorPanel, "Color panel"},
    {CLOCK_IDC_TIMEOUT_BROWSE, CmdBrowseFile, "Browse timeout file"},
    
    {CLOCK_IDM_TIMER_PAUSE_RESUME, CmdPauseResume, "Pause/Resume"},
    {CLOCK_IDM_TIMER_RESTART, CmdRestartTimer, "Restart timer"},
    {CLOCK_IDM_ABOUT, CmdAbout, "About dialog"},
    {CLOCK_IDM_TOPMOST, CmdToggleTopmost, "Toggle topmost"},
    
    {CLOCK_IDM_TIME_FORMAT_DEFAULT, CmdTimeFormatDefault, "Time format default"},
    {CLOCK_IDM_TIME_FORMAT_ZERO_PADDED, CmdTimeFormatZeroPadded, "Time format zero-padded"},
    {CLOCK_IDM_TIME_FORMAT_FULL_PADDED, CmdTimeFormatFullPadded, "Time format full-padded"},
    {CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS, CmdToggleMilliseconds, "Toggle milliseconds"},
    
    {CLOCK_IDM_COUNTDOWN_RESET, CmdCountdownReset, "Countdown reset"},
    {CLOCK_IDM_SHOW_CURRENT_TIME, CmdShowCurrentTime, "Show current time"},
    {CLOCK_IDM_24HOUR_FORMAT, Cmd24HourFormat, "24-hour format"},
    {CLOCK_IDM_SHOW_SECONDS, CmdShowSeconds, "Show seconds"},
    
    {CLOCK_IDM_BROWSE_FILE, CmdBrowseFile, "Browse file"},
    {CLOCK_IDM_COUNT_UP, CmdCountUp, "Count up"},
    {CLOCK_IDM_COUNT_UP_START, CmdCountUpStart, "Count up start"},
    {CLOCK_IDM_COUNT_UP_RESET, CmdCountUpReset, "Count up reset"},
    
    {CLOCK_IDM_POMODORO_START, CmdPomodoroStart, "Pomodoro start"},
    {CLOCK_IDM_POMODORO_RESET, CmdPomodoroReset, "Pomodoro reset"},
    {CLOCK_IDM_POMODORO_LOOP_COUNT, CmdPomodoroLoopCount, "Pomodoro loop count"},
    {CLOCK_IDM_POMODORO_COMBINATION, CmdPomodoroCombo, "Pomodoro combination"},
    
    {CLOCK_IDM_TIMEOUT_SHOW_TIME, CmdTimeoutShowTime, "Timeout show time"},
    {CLOCK_IDM_TIMEOUT_COUNT_UP, CmdTimeoutCountUp, "Timeout count up"},
    {CLOCK_IDM_SHOW_MESSAGE, CmdTimeoutShowMessage, "Show message"},
    {CLOCK_IDM_LOCK_SCREEN, CmdTimeoutLockScreen, "Lock screen"},
    {CLOCK_IDM_SHUTDOWN, CmdTimeoutShutdown, "Shutdown"},
    {CLOCK_IDM_RESTART, CmdTimeoutRestart, "Restart"},
    {CLOCK_IDM_SLEEP, CmdTimeoutSleep, "Sleep"},
    
    {CLOCK_IDM_CHECK_UPDATE, CmdCheckUpdate, "Check update"},
    {CLOCK_IDM_OPEN_WEBSITE, CmdOpenWebsite, "Open website"},
    {CLOCK_IDM_CURRENT_WEBSITE, CmdOpenWebsite, "Current website"},
    
    {CLOCK_IDM_NOTIFICATION_CONTENT, CmdNotificationContent, "Notification content"},
    {CLOCK_IDM_NOTIFICATION_DISPLAY, CmdNotificationDisplay, "Notification display"},
    {CLOCK_IDM_NOTIFICATION_SETTINGS, CmdNotificationSettings, "Notification settings"},
    
    {CLOCK_IDM_HOTKEY_SETTINGS, CmdHotkeySettings, "Hotkey settings"},
    {CLOCK_IDM_HELP, CmdHelp, "Help"},
    {CLOCK_IDM_SUPPORT, CmdSupport, "Support"},
    {CLOCK_IDM_FEEDBACK, CmdFeedback, "Feedback"},
    
    {0, NULL, NULL}
};

/* ============================================================================
 * Range Command System
 * ============================================================================ */

typedef BOOL (*RangeCommandHandler)(HWND hwnd, UINT cmd, int index);
typedef void (*RangeAction)(HWND hwnd, int index);

typedef struct {
    UINT rangeStart;
    UINT rangeEnd;
    RangeCommandHandler handler;
} RangeCommandDescriptor;

#define DEFINE_SIMPLE_RANGE_HANDLER(name, action) \
    static BOOL Handle##name(HWND hwnd, UINT cmd, int index) { \
        (void)cmd; \
        action(hwnd, index); \
        return TRUE; \
    }

static BOOL HandleQuickCountdown(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= 0 && index < time_options_count && time_options[index] > 0) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, time_options[index]);
    }
    return TRUE;
}

static BOOL HandleColorSelection(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= 0 && index < COLOR_OPTIONS_COUNT) {
        strncpy_s(CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), 
                 COLOR_OPTIONS[index].hexColor, _TRUNCATE);
        char config_path[MAX_PATH];
        GetConfigPath(config_path, MAX_PATH);
        WriteConfig(config_path);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return TRUE;
}

static BOOL HandleRecentFile(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= CLOCK_RECENT_FILES_COUNT) return TRUE;
    
    if (!ValidateAndSetTimeoutFile(hwnd, CLOCK_RECENT_FILES[index].path)) {
        WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", "");
        WriteConfigTimeoutAction("MESSAGE");
        for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
            CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
        }
        CLOCK_RECENT_FILES_COUNT--;
        char config_path[MAX_PATH];
        GetConfigPath(config_path, MAX_PATH);
        WriteConfig(config_path);
    }
    return TRUE;
}

static BOOL HandlePomodoroTime(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    HandlePomodoroTimeConfig(hwnd, index);
    return TRUE;
}

static BOOL HandleFontSelection(HWND hwnd, UINT cmd, int index) {
    (void)index;
    wchar_t fontsFolderRootW[MAX_PATH];
    if (!GetFontsFolderWideFromConfig(fontsFolderRootW, MAX_PATH)) return TRUE;
    
    int currentIndex = CMD_FONT_SELECTION_BASE;
    wchar_t foundRelativePathW[MAX_PATH];
    
    if (FindFontByIdRecursiveW(fontsFolderRootW, cmd, &currentIndex, 
                              foundRelativePathW, fontsFolderRootW)) {
        char foundFontNameUTF8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, foundRelativePathW, -1, 
                          foundFontNameUTF8, MAX_PATH, NULL, NULL);
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        if (SwitchFont(hInstance, foundFontNameUTF8)) {
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
    }
    return TRUE;
}

static BOOL DispatchRangeCommand(HWND hwnd, UINT cmd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    RangeCommandDescriptor rangeTable[] = {
        {CMD_QUICK_COUNTDOWN_BASE, CMD_QUICK_COUNTDOWN_END, HandleQuickCountdown},
        {CLOCK_IDM_QUICK_TIME_BASE, CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS - 1, HandleQuickCountdown},
        {CMD_COLOR_OPTIONS_BASE, CMD_COLOR_OPTIONS_BASE + COLOR_OPTIONS_COUNT - 1, HandleColorSelection},
        {CLOCK_IDM_RECENT_FILE_1, CLOCK_IDM_RECENT_FILE_5, HandleRecentFile},
        {CMD_POMODORO_TIME_BASE, CMD_POMODORO_TIME_END, HandlePomodoroTime},
        {CMD_FONT_SELECTION_BASE, CMD_FONT_SELECTION_END - 1, HandleFontSelection},
        {0, 0, NULL}
    };
    
    for (const RangeCommandDescriptor* r = rangeTable; r->handler; r++) {
        if (cmd >= r->rangeStart && cmd <= r->rangeEnd) {
            int index = cmd - r->rangeStart;
            return r->handler(hwnd, cmd, index);
        }
    }
    
    if (cmd >= CLOCK_IDM_LANG_CHINESE && cmd <= CLOCK_IDM_LANG_KOREAN) {
        return HandleLanguageSelection(hwnd, cmd);
    }
    if (cmd == CLOCK_IDM_POMODORO_WORK || cmd == CLOCK_IDM_POMODORO_BREAK || 
        cmd == CLOCK_IDM_POMODORO_LBREAK) {
        int idx = (cmd == CLOCK_IDM_POMODORO_WORK) ? 0 : 
                 (cmd == CLOCK_IDM_POMODORO_BREAK) ? 1 : 2;
        return HandlePomodoroTime(hwnd, cmd, idx);
    }
    if (HandleAnimationMenuCommand(hwnd, cmd)) return TRUE;
    
    return FALSE;
}

/* ============================================================================
 * Application Control
 * ============================================================================ */

/** Exit and remove tray icon */
static void ExitProgram(HWND hwnd) {
    (void)hwnd;
    RemoveTrayIcon();
    PostQuitMessage(0);
}

/** Maps config reload messages to handlers */
static const AppMessageDispatchEntry APP_MESSAGE_DISPATCH_TABLE[] = {
    {WM_APP_DISPLAY_CHANGED,       HandleAppDisplayChanged,       "Display settings reload"},
    {WM_APP_TIMER_CHANGED,         HandleAppTimerChanged,         "Timer settings reload"},
    {WM_APP_POMODORO_CHANGED,      HandleAppPomodoroChanged,      "Pomodoro settings reload"},
    {WM_APP_NOTIFICATION_CHANGED,  HandleAppNotificationChanged,  "Notification settings reload"},
    {WM_APP_HOTKEYS_CHANGED,       HandleAppHotkeysChanged,       "Hotkey assignments reload"},
    {WM_APP_RECENTFILES_CHANGED,   HandleAppRecentFilesChanged,   "Recent files list reload"},
    {WM_APP_COLORS_CHANGED,        HandleAppColorsChanged,        "Color options reload"},
    {WM_APP_ANIM_SPEED_CHANGED,    HandleAppAnimSpeedChanged,     "Animation speed reload"},
    {WM_APP_ANIM_PATH_CHANGED,     HandleAppAnimPathChanged,      "Animation path reload"},
    {0,                             NULL,                          NULL}
};

/** Route config reload message to handler */
static inline BOOL DispatchAppMessage(HWND hwnd, UINT msg) {
    for (const AppMessageDispatchEntry* entry = APP_MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msgId == msg) {
            entry->handler(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}

/* ============================================================================
 * Hotkey Handler Dispatch Table
 * ============================================================================ */

typedef void (*HotkeyAction)(HWND);

typedef struct {
    int id;
    HotkeyAction action;
} HotkeyDescriptor;

/** Toggle window visibility */
static void HotkeyToggleVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

/** Restart timer and close notifications */
static void HotkeyRestartTimer(HWND hwnd) {
    CloseAllNotifications();
    RestartCurrentTimer(hwnd);
}

/** Show custom countdown dialog (closes existing first) */
static void HotkeyCustomCountdown(HWND hwnd) {
    if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
        SendMessage(g_hwndInputDialog, WM_CLOSE, 0, 0);
        return;
    }
    
    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();
    ClearInputBuffer(inputText, sizeof(inputText));
    
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), 
                   hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);
    
    if (inputText[0] != L'\0') {
        int total_seconds = 0;
        Utf8String us = ToUtf8(inputText);
        if (ParseInput(us.buf, &total_seconds)) {
            CleanupBeforeTimerAction();
            StartCountdownWithTime(hwnd, total_seconds);
        }
    }
}

/** Trigger quick countdown slot */
static void HotkeyQuickCountdown(HWND hwnd, int index) {
    StartQuickCountdownByIndex(hwnd, index);
}

/** Encode index as function pointer */
#define QUICK_CD(n) (void(*)(HWND))(void*)(uintptr_t)(n)

static const HotkeyDescriptor HOTKEY_DISPATCH_TABLE[] = {
    {HOTKEY_ID_SHOW_TIME, ToggleShowTimeMode},
    {HOTKEY_ID_COUNT_UP, StartCountUp},
    {HOTKEY_ID_COUNTDOWN, StartDefaultCountDown},
    {HOTKEY_ID_QUICK_COUNTDOWN1, QUICK_CD(1)},
    {HOTKEY_ID_QUICK_COUNTDOWN2, QUICK_CD(2)},
    {HOTKEY_ID_QUICK_COUNTDOWN3, QUICK_CD(3)},
    {HOTKEY_ID_POMODORO, StartPomodoroTimer},
    {HOTKEY_ID_TOGGLE_VISIBILITY, HotkeyToggleVisibility},
    {HOTKEY_ID_EDIT_MODE, ToggleEditMode},
    {HOTKEY_ID_PAUSE_RESUME, TogglePauseResume},
    {HOTKEY_ID_RESTART_TIMER, HotkeyRestartTimer},
    {HOTKEY_ID_CUSTOM_COUNTDOWN, HotkeyCustomCountdown}
};

/** Route hotkey to handler (quick countdown uses encoded index) */
static BOOL DispatchHotkey(HWND hwnd, int hotkeyId) {
    for (size_t i = 0; i < ARRAY_SIZE(HOTKEY_DISPATCH_TABLE); i++) {
        if (HOTKEY_DISPATCH_TABLE[i].id == hotkeyId) {
            HotkeyAction action = HOTKEY_DISPATCH_TABLE[i].action;
            
            if (hotkeyId >= HOTKEY_ID_QUICK_COUNTDOWN1 && hotkeyId <= HOTKEY_ID_QUICK_COUNTDOWN3) {
                int index = (int)(uintptr_t)(void*)action;
                HotkeyQuickCountdown(hwnd, index);
            } else {
                action(hwnd);
            }
            return TRUE;
        }
    }
    return FALSE;
}

/* ============================================================================
 * Hotkey Registration System (v12.0 - X-Macro Generated)
 * ============================================================================ */

/** X-macro hotkey registry: X(ID_SUFFIX, CONFIG_KEY) */
#define HOTKEY_REGISTRY \
    X(SHOW_TIME, "HOTKEY_SHOW_TIME") \
    X(COUNT_UP, "HOTKEY_COUNT_UP") \
    X(COUNTDOWN, "HOTKEY_COUNTDOWN") \
    X(QUICK_COUNTDOWN1, "HOTKEY_QUICK_COUNTDOWN1") \
    X(QUICK_COUNTDOWN2, "HOTKEY_QUICK_COUNTDOWN2") \
    X(QUICK_COUNTDOWN3, "HOTKEY_QUICK_COUNTDOWN3") \
    X(POMODORO, "HOTKEY_POMODORO") \
    X(TOGGLE_VISIBILITY, "HOTKEY_TOGGLE_VISIBILITY") \
    X(EDIT_MODE, "HOTKEY_EDIT_MODE") \
    X(PAUSE_RESUME, "HOTKEY_PAUSE_RESUME") \
    X(RESTART_TIMER, "HOTKEY_RESTART_TIMER") \
    X(CUSTOM_COUNTDOWN, "HOTKEY_CUSTOM_COUNTDOWN")

typedef struct {
    int id;
    WORD value;
    const char* configKey;
} HotkeyConfig;
static HotkeyConfig g_hotkeyConfigs[] = {
    #define X(name, key) {HOTKEY_ID_##name, 0, key},
    HOTKEY_REGISTRY
    #undef X
};

/** Register hotkey (clears value on conflict) */
static BOOL RegisterSingleHotkey(HWND hwnd, HotkeyConfig* config) {
    if (config->value == 0) return FALSE;
    
    BYTE vk = LOBYTE(config->value);
    BYTE mod = HIBYTE(config->value);
    
    UINT fsModifiers = 0;
    if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
    if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
    if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
    
    if (!RegisterHotKey(hwnd, config->id, fsModifiers, vk)) {
        config->value = 0;
        return FALSE;
    }
    return TRUE;
}

/** Load and register hotkeys (auto-clears conflicts in INI) */
BOOL RegisterGlobalHotkeys(HWND hwnd) {
    extern WORD StringToHotkey(const char* str);
    extern void HotkeyToString(WORD hotkey, char* out, size_t size);
    
    UnregisterGlobalHotkeys(hwnd);
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        char hotkeyStr[64];
        ReadIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey, 
                     "None", hotkeyStr, sizeof(hotkeyStr), config_path);
        g_hotkeyConfigs[i].value = StringToHotkey(hotkeyStr);
    }
    
    BOOL anyRegistered = FALSE;
    BOOL configChanged = FALSE;
    
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        WORD oldValue = g_hotkeyConfigs[i].value;
        if (RegisterSingleHotkey(hwnd, &g_hotkeyConfigs[i])) {
            anyRegistered = TRUE;
        } else if (oldValue != 0) {
            configChanged = TRUE;
        }
    }
    
    if (configChanged) {
        for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
            char hotkeyStr[64];
            HotkeyToString(g_hotkeyConfigs[i].value, hotkeyStr, sizeof(hotkeyStr));
            WriteIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey, 
                          hotkeyStr, config_path);
        }
    }
    
    return anyRegistered;
}

/** Unregister all hotkeys */
void UnregisterGlobalHotkeys(HWND hwnd) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        UnregisterHotKey(hwnd, g_hotkeyConfigs[i].id);
    }
}

/* ============================================================================
 * Unified Preview System (v8.0 - Meta-programming)
 * ============================================================================ */

typedef BOOL (*PreviewMatcher)(HWND hwnd, UINT menuId);

typedef struct {
    UINT rangeStart;
    UINT rangeEnd;
    PreviewMatcher matcher;
} PreviewRange;

/** Start color preview */
static BOOL MatchColorPreview(HWND hwnd, UINT menuId) {
    int colorIndex = menuId - CMD_COLOR_OPTIONS_BASE;
    if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
        StartPreview(PREVIEW_TYPE_COLOR, COLOR_OPTIONS[colorIndex].hexColor, hwnd);
        return TRUE;
    }
    return FALSE;
}

/** Start font preview */
static BOOL MatchFontPreview(HWND hwnd, UINT menuId) {
    wchar_t fontsFolderRootW[MAX_PATH];
    if (!GetFontsFolderWideFromConfig(fontsFolderRootW, MAX_PATH)) return FALSE;
    
    int currentIndex = CMD_FONT_SELECTION_BASE;
    wchar_t foundRelativePathW[MAX_PATH];
    
    if (FindFontByIdRecursiveW(fontsFolderRootW, menuId, &currentIndex, 
                               foundRelativePathW, fontsFolderRootW)) {
        char foundFontNameUTF8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, foundRelativePathW, -1, 
                          foundFontNameUTF8, MAX_PATH, NULL, NULL);
        StartPreview(PREVIEW_TYPE_FONT, foundFontNameUTF8, hwnd);
        return TRUE;
    }
    return FALSE;
}

/** Start animation preview */
static BOOL MatchAnimationPreview(HWND hwnd, UINT menuId) {
    char animPath[MAX_PATH] = {0};
    
    if (GetAnimationNameFromMenuId(menuId, animPath, sizeof(animPath))) {
        StartPreview(PREVIEW_TYPE_ANIMATION, animPath, hwnd);
        return TRUE;
    }
    
    return FALSE;
}

/** Start time format preview */
static BOOL MatchTimeFormatPreview(HWND hwnd, UINT menuId) {
    if (menuId == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
        BOOL previewMs = !CLOCK_SHOW_MILLISECONDS;
        StartPreview(PREVIEW_TYPE_MILLISECONDS, &previewMs, hwnd);
        return TRUE;
    }
    
    TimeFormatType formats[] = {
        [CLOCK_IDM_TIME_FORMAT_DEFAULT] = TIME_FORMAT_DEFAULT,
        [CLOCK_IDM_TIME_FORMAT_ZERO_PADDED] = TIME_FORMAT_ZERO_PADDED,
        [CLOCK_IDM_TIME_FORMAT_FULL_PADDED] = TIME_FORMAT_FULL_PADDED
    };
    
    if (menuId >= CLOCK_IDM_TIME_FORMAT_DEFAULT && menuId <= CLOCK_IDM_TIME_FORMAT_FULL_PADDED) {
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &formats[menuId], hwnd);
        return TRUE;
    }
    return FALSE;
}

/** Maps menu ID ranges to preview matchers */
static const PreviewRange PREVIEW_RANGES[] = {
    {CMD_COLOR_OPTIONS_BASE, CMD_COLOR_OPTIONS_BASE + 100, MatchColorPreview},
    {CMD_FONT_SELECTION_BASE, CMD_FONT_SELECTION_END, MatchFontPreview},
    {CLOCK_IDM_TIME_FORMAT_DEFAULT, CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS, MatchTimeFormatPreview},
    {CLOCK_IDM_ANIMATIONS_USE_LOGO, CLOCK_IDM_ANIMATIONS_BASE + MAX_ANIMATION_MENU_ITEMS, MatchAnimationPreview}
};

/** Find and trigger preview handler */
static BOOL DispatchPreview(HWND hwnd, UINT menuId) {
    for (size_t i = 0; i < ARRAY_SIZE(PREVIEW_RANGES); i++) {
        if (menuId >= PREVIEW_RANGES[i].rangeStart && menuId <= PREVIEW_RANGES[i].rangeEnd) {
            return PREVIEW_RANGES[i].matcher(hwnd, menuId);
        }
    }
    return FALSE;
}

/** Trigger preview on menu hover (debounced) */
static LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    UINT menuItem = LOWORD(wp);
    UINT flags = HIWORD(wp);
    HMENU hMenu = (HMENU)lp;
    
    if (menuItem == 0xFFFF) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL);
        return 0;
    }
    
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    if (hMenu == NULL) return 0;
    
    if (!(flags & MF_POPUP) || (menuItem >= CLOCK_IDM_ANIMATIONS_USE_LOGO)) {
        if (DispatchPreview(hwnd, menuItem)) {
            return 0;
        }
    }
    
    CancelPreview(hwnd);
    return 0;
}

/* ============================================================================
 * Message Dispatch Table System (v10.0)
 * ============================================================================ */

typedef LRESULT (*MessageHandler)(HWND hwnd, WPARAM wp, LPARAM lp);

typedef struct {
    UINT msg;
    MessageHandler handler;
    const char* description;
} MessageDispatchEntry;

static LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleMouseMove(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandlePaint(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleQuickCountdownIndex(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleShowCliHelp(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleTrayUpdateIcon(HWND hwnd, WPARAM wp, LPARAM lp);
static LRESULT HandleAppReregisterHotkeys(HWND hwnd, WPARAM wp, LPARAM lp);

/* ============================================================================
 * Message Handler Implementations
 * ============================================================================ */

static LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    RegisterGlobalHotkeys(hwnd);
    HandleWindowCreate(hwnd);
    extern void ConfigWatcher_Start(HWND hwnd);
    ConfigWatcher_Start(hwnd);
    return 0;
}

static LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp);
    if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    if (LOWORD(lp) == HTCLIENT || lp == CLOCK_WM_TRAYICON) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    return DefWindowProc(hwnd, WM_SETCURSOR, wp, lp);
}

static LRESULT HandleLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    StartDragWindow(hwnd);
    return 0;
}

static LRESULT HandleLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    EndDragWindow(hwnd);
    return 0;
}

static LRESULT HandleMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(lp);
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    HandleScaleWindow(hwnd, delta);
    return 0;
}

static LRESULT HandleMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (HandleDragWindow(hwnd)) return 0;
    return DefWindowProc(hwnd, WM_MOUSEMOVE, wp, lp);
}

static LRESULT HandlePaint(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    HandleWindowPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}

/** Cancel debounced preview */
static LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(lp);
    if (wp == IDT_MENU_DEBOUNCE) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        CancelPreview(hwnd);
        return 0;
    }
    HandleTimerEvent(hwnd, wp);
    return 0;
}

static LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    UnregisterGlobalHotkeys(hwnd);
    HandleWindowDestroy(hwnd);
    extern void ConfigWatcher_Stop(void);
    ConfigWatcher_Stop();
    return 0;
}

static LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
    return 0;
}

/** Route menu command */
static LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    WORD cmd = LOWORD(wp);
    
    BOOL isAnimationSelectionCommand = 
        (cmd >= CLOCK_IDM_ANIMATIONS_BASE && cmd < CLOCK_IDM_ANIMATIONS_BASE + MAX_ANIMATION_MENU_ITEMS) ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_MEM;
    
    if (isAnimationSelectionCommand) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    } else {
        CancelAnimationPreview();
    }
    
    if (DispatchRangeCommand(hwnd, cmd, wp, lp)) return 0;
    
    for (const CommandDispatchEntry* entry = COMMAND_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->cmdId == cmd) return entry->handler(hwnd, wp, lp);
    }
    
    return 0;
}

/** Save position in edit mode */
static LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (CLOCK_EDIT_MODE) SaveWindowSettings(hwnd);
    return 0;
}

/** Adjust position on monitor change */
static LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    AdjustWindowPosition(hwnd, TRUE);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    return 0;
}

/** Exit edit mode on release */
static LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONUP, wp, lp);
}

/** Ctrl+Right toggles edit mode */
static LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
        if (CLOCK_EDIT_MODE) {
            SetClickThrough(hwnd, FALSE);
        } else {
            SetClickThrough(hwnd, TRUE);
            SaveWindowSettings(hwnd);
            WriteConfigColor(CLOCK_TEXT_COLOR);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONDOWN, wp, lp);
}

/** Debounce preview cancel */
static LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL);
    return 0;
}

static LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    SaveWindowSettings(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

/** Enter edit mode on double-click */
static LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    if (!CLOCK_EDIT_MODE) {
        StartEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_LBUTTONDBLCLK, wp, lp);
}

static LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(lp);
    if (DispatchHotkey(hwnd, (int)wp)) return 0;
    return DefWindowProc(hwnd, WM_HOTKEY, wp, lp);
}

/** IPC from second instance */
static LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp);
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
    if (pcds && pcds->dwData == COPYDATA_ID_CLI_TEXT && pcds->lpData && pcds->cbData > 0) {
        const size_t maxLen = BUFFER_SIZE_CLI_INPUT - 1;
        char buf[BUFFER_SIZE_CLI_INPUT];
        size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
        memcpy(buf, pcds->lpData, n);
        buf[maxLen] = '\0';
        buf[n] = '\0';
        HandleCliArguments(hwnd, buf);
        return 0;
    }
    return DefWindowProc(hwnd, WM_COPYDATA, wp, lp);
}

static LRESULT HandleQuickCountdownIndex(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp);
    int idx = (int)lp;
    if (idx >= 1) {
        StartQuickCountdownByIndex(hwnd, idx);
    } else {
        StartDefaultCountDown(hwnd);
    }
    return 0;
}

static LRESULT HandleShowCliHelp(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    ShowCliHelpDialog(hwnd);
    return 0;
}

/** Tray animation frame */
static LRESULT HandleTrayUpdateIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(hwnd, wp, lp);
    if (TrayAnimation_HandleUpdateMessage()) return 0;
    return DefWindowProc(hwnd, WM_USER + 100, wp, lp);
}

static LRESULT HandleAppReregisterHotkeys(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(wp, lp);
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

/* ============================================================================
 * Owner-Drawn Menu Handlers
 * ============================================================================ */

/** Set color menu item dimensions */
static LRESULT HandleMeasureItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(hwnd, wp);
    LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
    if (lpmis->CtlType == ODT_MENU) {
        lpmis->itemHeight = 25;
        lpmis->itemWidth = BUFFER_SIZE_MENU_ITEM;
        return TRUE;
    }
    return FALSE;
}

/** Draw color swatch */
static LRESULT HandleDrawItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    UNUSED(hwnd, wp);
    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
    if (lpdis->CtlType != ODT_MENU) return FALSE;
    
    int colorIndex = lpdis->itemID - CMD_COLOR_OPTIONS_BASE;
    if (colorIndex < 0 || colorIndex >= COLOR_OPTIONS_COUNT) return FALSE;
    
    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
    int r, g, b;
    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
    
    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    
    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
    
    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
             lpdis->rcItem.right, lpdis->rcItem.bottom);
    
    SelectObject(lpdis->hDC, oldPen);
    SelectObject(lpdis->hDC, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
    
    if (lpdis->itemState & ODS_SELECTED) {
        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
    }
    
    return TRUE;
}

/* ============================================================================
 * Message Dispatch Table Definition
 * ============================================================================ */

static const MessageDispatchEntry MESSAGE_DISPATCH_TABLE[] = {
    {WM_CREATE, HandleCreate, "Window creation"},
    {WM_SETCURSOR, HandleSetCursor, "Cursor management"},
    {WM_LBUTTONDOWN, HandleLButtonDown, "Mouse left button down"},
    {WM_LBUTTONUP, HandleLButtonUp, "Mouse left button up"},
    {WM_LBUTTONDBLCLK, HandleLButtonDblClk, "Mouse double-click"},
    {WM_RBUTTONDOWN, HandleRButtonDown, "Mouse right button down"},
    {WM_RBUTTONUP, HandleRButtonUp, "Mouse right button up"},
    {WM_MOUSEWHEEL, HandleMouseWheel, "Mouse wheel scroll"},
    {WM_MOUSEMOVE, HandleMouseMove, "Mouse movement"},
    {WM_PAINT, HandlePaint, "Window painting"},
    {WM_TIMER, HandleTimer, "Timer tick"},
    {WM_DESTROY, HandleDestroy, "Window destruction"},
    {CLOCK_WM_TRAYICON, HandleTrayIcon, "Tray icon message"},
    {WM_COMMAND, HandleCommand, "Menu command"},
    {WM_WINDOWPOSCHANGED, HandleWindowPosChanged, "Window position changed"},
    {WM_DISPLAYCHANGE, HandleDisplayChange, "Display configuration changed"},
    {WM_MENUSELECT, HandleMenuSelect, "Menu item selection"},
    {WM_MEASUREITEM, HandleMeasureItem, "Owner-drawn menu measurement"},
    {WM_DRAWITEM, HandleDrawItem, "Owner-drawn menu rendering"},
    {WM_EXITMENULOOP, HandleExitMenuLoop, "Menu loop exit"},
    {WM_CLOSE, HandleClose, "Window close"},
    {WM_HOTKEY, HandleHotkey, "Global hotkey"},
    {WM_COPYDATA, HandleCopyData, "Inter-process communication"},
    {WM_APP_QUICK_COUNTDOWN_INDEX, HandleQuickCountdownIndex, "Quick countdown by index"},
    {WM_APP_SHOW_CLI_HELP, HandleShowCliHelp, "Show CLI help"},
    {WM_USER + 100, HandleTrayUpdateIcon, "Tray icon update"},
    {WM_APP + 1, HandleAppReregisterHotkeys, "Hotkey re-registration"},
    {0, NULL, NULL}
};

/* ============================================================================
 * Main Window Procedure
 * ============================================================================ */

/** Main window procedure (table-driven) */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TASKBARCREATED) {
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        return 0;
    }
    
    if (DispatchAppMessage(hwnd, msg)) {
        return 0;
    }
    
    for (const MessageDispatchEntry* entry = MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msg == msg) {
            return entry->handler(hwnd, wp, lp);
        }
    }
    
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ============================================================================
 * Public API - Timer Action Functions
 * ============================================================================ */

void ToggleShowTimeMode(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
    }
}

void StartCountUp(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    TimerModeParams params = {0, TRUE, FALSE, TRUE};
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
}

/** Start default countdown (prompts if not configured) */
void StartDefaultCountDown(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();
    
    if (CLOCK_DEFAULT_START_TIME > 0) {
        TimerModeParams params = {CLOCK_DEFAULT_START_TIME, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    } else {
        PostMessage(hwnd, WM_COMMAND, 101, 0);
    }
}

void StartPomodoroTimer(HWND hwnd) {
    CleanupBeforeTimerAction();
    PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
}

/** Toggle edit mode (enables dragging, restores click-through on exit) */
void ToggleEditMode(HWND hwnd) {
    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
    
    if (CLOCK_EDIT_MODE) {
        PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
        
        if (!CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
        }
        
        SetBlurBehind(hwnd, TRUE);
        SetClickThrough(hwnd, FALSE);
        
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else {
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), OPACITY_FULL, LWA_COLORKEY);
        
        SetClickThrough(hwnd, TRUE);
        
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
            SetTimer(hwnd, TIMER_ID_FORCE_REDRAW, 150, NULL);
            return;
        }
        
        SaveWindowSettings(hwnd);
        WriteConfigColor(CLOCK_TEXT_COLOR);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

void TogglePauseResume(HWND hwnd) {
    CleanupBeforeTimerAction();
    TogglePauseResumeTimer(hwnd);
}

void RestartCurrentTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        if (CLOCK_COUNT_UP) {
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            countdown_elapsed_time = 0;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    HandleWindowReset(hwnd);
}

static void StartQuickCountdownByZeroBasedIndex(HWND hwnd, int index) {
    CleanupBeforeTimerAction();
    
    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();
    
    if (index >= 0 && index < time_options_count) {
        StartCountdownWithTime(hwnd, time_options[index]);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    CleanupBeforeTimerAction();

    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();

    int zeroBased = index - 1;
    if (zeroBased >= 0 && zeroBased < time_options_count) {
        StartCountdownWithTime(hwnd, time_options[zeroBased]);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

/** Stop audio and close notifications (prevents stale popups) */
void CleanupBeforeTimerAction(void) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    CloseAllNotifications();
}

/** Start countdown (exits Pomodoro if active) */
BOOL StartCountdownWithTime(HWND hwnd, int seconds) {
    if (seconds <= 0) return FALSE;
    
    countdown_message_shown = FALSE;
    
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles = 0;
    }
    
    TimerModeParams params = {seconds, TRUE, TRUE, TRUE};
    return SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
}
static const struct {
    UINT menuId;
    AppLanguage language;
} LANGUAGE_MAP[] = {
    {CLOCK_IDM_LANG_CHINESE, APP_LANG_CHINESE_SIMP},
    {CLOCK_IDM_LANG_CHINESE_TRAD, APP_LANG_CHINESE_TRAD},
    {CLOCK_IDM_LANG_ENGLISH, APP_LANG_ENGLISH},
    {CLOCK_IDM_LANG_SPANISH, APP_LANG_SPANISH},
    {CLOCK_IDM_LANG_FRENCH, APP_LANG_FRENCH},
    {CLOCK_IDM_LANG_GERMAN, APP_LANG_GERMAN},
    {CLOCK_IDM_LANG_RUSSIAN, APP_LANG_RUSSIAN},
    {CLOCK_IDM_LANG_PORTUGUESE, APP_LANG_PORTUGUESE},
    {CLOCK_IDM_LANG_JAPANESE, APP_LANG_JAPANESE},
    {CLOCK_IDM_LANG_KOREAN, APP_LANG_KOREAN}
};

BOOL HandleLanguageSelection(HWND hwnd, UINT menuId) {
    for (size_t i = 0; i < sizeof(LANGUAGE_MAP) / sizeof(LANGUAGE_MAP[0]); i++) {
        if (menuId == LANGUAGE_MAP[i].menuId) {
            SetLanguage(LANGUAGE_MAP[i].language);
            WriteConfigLanguage(LANGUAGE_MAP[i].language);
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateTrayIcon(hwnd);
            return TRUE;
        }
    }
    
    return FALSE;
}

/** Configure Pomodoro phase duration via dialog */
BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex) {
    if (selectedIndex < 0 || selectedIndex >= POMODORO_TIMES_COUNT) {
        return FALSE;
    }
    
    memset(inputText, 0, sizeof(inputText));
    DialogBoxParamW(GetModuleHandle(NULL), 
             MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
             hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
    
    if (inputText[0] && !isAllSpacesOnly(inputText)) {
        int total_seconds = 0;

        char inputTextA[256];
        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
        if (ParseInput(inputTextA, &total_seconds)) {
            POMODORO_TIMES[selectedIndex] = total_seconds;
            
            WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
            
            if (selectedIndex == 0) POMODORO_WORK_TIME = total_seconds;
            else if (selectedIndex == 1) POMODORO_SHORT_BREAK = total_seconds;
            else if (selectedIndex == 2) POMODORO_LONG_BREAK = total_seconds;
            
            return TRUE;
        }
    }
    
    return FALSE;
}
