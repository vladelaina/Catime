/**
 * @file tray_menu.c
 * @brief System tray context menus (left/right-click)
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
#include "log.h"
#include "language.h"
#include "tray/tray_menu.h"
#include "font.h"
#include "color/color.h"
#include "window.h"
#include "drag_scale.h"
#include "pomodoro.h"
#include "timer/timer.h"
#include "config.h"
#include "../resource/resource.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_menu.h"
#include "startup.h"
#include "utils/string_convert.h"
#include "utils/natural_sort.h"
#include "utils/string_format.h"
#include "tray/tray_menu_pomodoro.h"
#include "cache/resource_cache.h"

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

/** @brief qsort comparator for FontEntry - directories first, then natural sort */
static int CompareFontEntries(const void* a, const void* b) {
    const FontEntry* entryA = (const FontEntry*)a;
    const FontEntry* entryB = (const FontEntry*)b;
    
    /* Directories first */
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir;
    }
    
    /* Then natural sort by name */
    return NaturalCompareW(entryA->name, entryB->name);
}

extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern char CLOCK_TEXT_COLOR[10];
extern char FONT_FILE_NAME[MAX_PATH];
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];
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

/** 
 * @brief Load timeout action from config using standard API 
 * 
 * @note Preserves one-time actions (SHUTDOWN/RESTART/SLEEP) in memory.
 * These actions are intentionally not persisted to config, so we should
 * not override them when reading from config file.
 */
void ReadTimeoutActionFromConfig() {
    /* Preserve one-time actions: don't override them from config */
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ||
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ||
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
        return;
    }
    
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    char value[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", 
                  value, sizeof(value), configPath);
    
    if (strcmp(value, "MESSAGE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(value, "LOCK") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
    } else if (strcmp(value, "OPEN_FILE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    } else if (strcmp(value, "SHOW_TIME") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
    } else if (strcmp(value, "COUNT_UP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
    } else if (strcmp(value, "OPEN_WEBSITE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    } else {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    }
}

/**
 * @brief Build timeout action submenu
 * @param hMenu Parent menu handle
 */
static void BuildTimeoutActionSubmenu(HMENU hMenu) {
    HMENU hTimeoutMenu = CreatePopupMenu();
    
    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_SHOW_MESSAGE, 
               GetLocalizedString(NULL, L"Show Message"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHOW_TIME ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_SHOW_TIME, 
               GetLocalizedString(NULL, L"Show Current Time"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_COUNT_UP ? MF_CHECKED : MF_UNCHECKED), 
               CLOCK_IDM_TIMEOUT_COUNT_UP, 
               GetLocalizedString(NULL, L"Count Up"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_LOCK ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_LOCK_SCREEN,
               GetLocalizedString(NULL, L"Lock Screen"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    HMENU hFileMenu = CreatePopupMenu();

    for (int i = 0; i < g_AppConfig.recent_files.count; i++) {
        wchar_t wFileName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.recent_files.files[i].name, -1, wFileName, MAX_PATH);
        
        wchar_t truncatedName[MAX_PATH];
        TruncateFileName(wFileName, truncatedName, 25);
        
        BOOL isCurrentFile = (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && 
                             strlen(CLOCK_TIMEOUT_FILE_PATH) > 0 && 
                             strcmp(g_AppConfig.recent_files.files[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0);
        
        AppendMenuW(hFileMenu, MF_STRING | (isCurrentFile ? MF_CHECKED : 0), 
                   CLOCK_IDM_RECENT_FILE_1 + i, truncatedName);
    }
               
    if (g_AppConfig.recent_files.count > 0) {
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    }

    AppendMenuW(hFileMenu, MF_STRING, CLOCK_IDM_BROWSE_FILE,
               GetLocalizedString(NULL, L"Browse..."));

    AppendMenuW(hTimeoutMenu, MF_POPUP | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE ? MF_CHECKED : MF_UNCHECKED), 
               (UINT_PTR)hFileMenu, 
               GetLocalizedString(NULL, L"Open File/Software"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_OPEN_WEBSITE,
               GetLocalizedString(NULL, L"Open Website"));

    AppendMenuW(hTimeoutMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hTimeoutMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 
               0,
               GetLocalizedString(NULL, L"Following actions are one-time only"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHUTDOWN,
               GetLocalizedString(NULL, L"Shutdown"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_RESTART,
               GetLocalizedString(NULL, L"Restart"));

    AppendMenuW(hTimeoutMenu, MF_STRING | (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SLEEP,
               GetLocalizedString(NULL, L"Sleep"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeoutMenu, 
                GetLocalizedString(NULL, L"Timeout Action"));
}

/**
 * @brief Build preset management submenu (time options, startup settings, notifications)
 * @param hMenu Parent menu handle
 */
static void BuildPresetManagementSubmenu(HMENU hMenu) {
    HMENU hTimeOptionsMenu = CreatePopupMenu();
    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDC_MODIFY_TIME_OPTIONS,
                GetLocalizedString(NULL, L"Modify Quick Countdown Options"));
    
    HMENU hStartupSettingsMenu = CreatePopupMenu();

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    char currentStartupMode[20] = "COUNTDOWN";
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN",
                  currentStartupMode, sizeof(currentStartupMode), configPath);
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNTDOWN") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_SET_COUNTDOWN_TIME,
                GetLocalizedString(NULL, L"Countdown"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "COUNT_UP") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_COUNT_UP,
                GetLocalizedString(NULL, L"Stopwatch"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "SHOW_TIME") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_SHOW_TIME,
                GetLocalizedString(NULL, L"Show Current Time"));
    
    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
                (strcmp(currentStartupMode, "NO_DISPLAY") == 0 ? MF_CHECKED : 0),
                CLOCK_IDC_START_NO_DISPLAY,
                GetLocalizedString(NULL, L"No Display"));
    
    AppendMenuW(hStartupSettingsMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hStartupSettingsMenu, MF_STRING | 
            (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
            CLOCK_IDC_AUTO_START,
            GetLocalizedString(NULL, L"Start with Windows"));

    AppendMenuW(hTimeOptionsMenu, MF_POPUP, (UINT_PTR)hStartupSettingsMenu,
                GetLocalizedString(NULL, L"Startup Settings"));

    AppendMenuW(hTimeOptionsMenu, MF_STRING, CLOCK_IDM_NOTIFICATION_SETTINGS,
                GetLocalizedString(NULL, L"Notification Settings"));

    AppendMenuW(hTimeOptionsMenu, MF_SEPARATOR, 0, NULL);
    
    AppendMenuW(hTimeOptionsMenu, MF_STRING | (CLOCK_WINDOW_TOPMOST ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TOPMOST,
                GetLocalizedString(NULL, L"Always on Top"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimeOptionsMenu,
                GetLocalizedString(NULL, L"Preset Management"));
}

/**
 * @brief Build format submenu (time format options)
 * @param hMenu Parent menu handle
 */
static void BuildFormatSubmenu(HMENU hMenu) {
    HMENU hFormatMenu = CreatePopupMenu();
    
    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_DEFAULT ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_DEFAULT,
                GetLocalizedString(NULL, L"Default Format"));
    
    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_ZERO_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_ZERO_PADDED,
                GetLocalizedString(NULL, L"09:59 Format"));
    
    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_FULL_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_FULL_PADDED,
                GetLocalizedString(NULL, L"00:09:59 Format"));
    
    AppendMenuW(hFormatMenu, MF_SEPARATOR, 0, NULL);
    
    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.show_milliseconds ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS,
                GetLocalizedString(NULL, L"Show Milliseconds"));
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFormatMenu,
                GetLocalizedString(NULL, L"Format"));
}

/**
 * @brief Build font submenu with recursive folder scanning
 * @param hMenu Parent menu handle
 */
static void BuildFontSubmenu(HMENU hMenu) {
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
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        // Try to use cached font list (FAST PATH - 5-10ms)
        const FontCacheEntry* cachedFonts = NULL;
        int cachedCount = 0;
        FontCacheStatus cacheStatus = FontCache_GetEntries(&cachedFonts, &cachedCount);
        
        if (cacheStatus == FONT_CACHE_OK || cacheStatus == FONT_CACHE_EXPIRED) {
            // Use cached data to build menu quickly
            WriteLog(LOG_LEVEL_INFO, "Using cached font list (%d fonts, status=%d)", cachedCount, cacheStatus);
            
            g_advancedFontId = 2000;
            for (int i = 0; i < cachedCount; i++) {
                UINT flags = MF_STRING;
                if (cachedFonts[i].isCurrentFont) {
                    flags |= MF_CHECKED;
                }
                AppendMenuW(hFontSubMenu, flags, g_advancedFontId++, cachedFonts[i].displayName);
            }
            
            if (cachedCount > 0) {
                AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
            }
            
            // Trigger async refresh if cache expired
            if (cacheStatus == FONT_CACHE_EXPIRED) {
                ResourceCache_RequestRefresh();
            }
        } else {
            // Fallback: use original synchronous scan (SLOW PATH - 100-300ms)
            WriteLog(LOG_LEVEL_INFO, "Font cache not ready (status=%d), using fallback sync scan", cacheStatus);
            
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
                               GetLocalizedString(NULL, L"No font files found"));
                    AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
                } else {
                    AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
                }
            }
        }
        
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED, 
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu, 
                GetLocalizedString(NULL, L"Font"));
}

/**
 * @brief Build color submenu
 * @param hMenu Parent menu handle
 */
static void BuildColorSubmenu(HMENU hMenu) {
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
                GetLocalizedString(NULL, L"Color Value"));
    AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL, 
                GetLocalizedString(NULL, L"Color Panel"));

    AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu, 
                GetLocalizedString(NULL, L"Customize"));
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu, 
                GetLocalizedString(NULL, L"Color"));
}

/**
 * @brief Build animation/tray icon submenu
 * @param hMenu Parent menu handle
 */
static void BuildAnimationSubmenu(HMENU hMenu) {
    HMENU hAnimMenu = CreatePopupMenu();
    {
        const char* currentAnim = GetCurrentAnimationName();
        BuildAnimationMenu(hAnimMenu, currentAnim);
        
        if (GetMenuItemCount(hAnimMenu) <= 4) {
            AppendMenuW(hAnimMenu, MF_STRING | MF_GRAYED, 0, GetLocalizedString(NULL, L"(Supports GIF, WebP, PNG, etc.)"));
        }

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

        HMENU hAnimSpeedMenu = CreatePopupMenu();
        AnimationSpeedMetric currentMetric = GetAnimationSpeedMetric();
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_MEMORY ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_MEMORY, GetLocalizedString(NULL, L"By Memory Usage"));
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_CPU ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_CPU, GetLocalizedString(NULL, L"By CPU Usage"));
        AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_TIMER ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_ANIM_SPEED_TIMER, GetLocalizedString(NULL, L"By Countdown Progress"));
        AppendMenuW(hAnimMenu, MF_POPUP, (UINT_PTR)hAnimSpeedMenu,
                    GetLocalizedString(NULL, L"Animation Speed Metric"));

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hAnimMenu, MF_STRING, CLOCK_IDM_ANIMATIONS_OPEN_DIR, GetLocalizedString(NULL, L"Open animations folder"));
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAnimMenu, GetLocalizedString(NULL, L"Tray Icon"));
}

/**
 * @brief Build help/about submenu
 * @param hMenu Parent menu handle
 */
static void BuildHelpSubmenu(HMENU hMenu) {
    HMENU hAboutMenu = CreatePopupMenu();

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_ABOUT, GetLocalizedString(NULL, L"About"));

    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_SUPPORT, GetLocalizedString(NULL, L"Support"));
    
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_FEEDBACK, GetLocalizedString(NULL, L"Feedback"));
    
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_HELP, GetLocalizedString(NULL, L"User Guide"));

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, 
               GetLocalizedString(NULL, L"Check for Updates"));

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
                GetLocalizedString(NULL, L"Reset"));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu,
                GetLocalizedString(NULL, L"Help"));
}

/**
 * @brief Build and display right-click configuration menu (Coordinator)
 * @param hwnd Main window handle
 * @note Delegates to specialized submenu builders for maintainability
 */
void ShowColorMenu(HWND hwnd) {
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    HMENU hMenu = CreatePopupMenu();
    
    /* Edit mode toggle */
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(NULL, L"Edit Mode"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* Build submenus using modular functions */
    BuildTimeoutActionSubmenu(hMenu);
    BuildPresetManagementSubmenu(hMenu);
    
    /* Hotkey settings */
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(NULL, L"Hotkey Settings"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildFormatSubmenu(hMenu);
    BuildFontSubmenu(hMenu);
    BuildColorSubmenu(hMenu);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildAnimationSubmenu(hMenu);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildHelpSubmenu(hMenu);

    /* Exit */
    AppendMenuW(hMenu, MF_STRING, 109,
                GetLocalizedString(NULL, L"Exit"));
    
    /* Display menu */
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
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    HMENU hMenu = CreatePopupMenu();
    
    HMENU hTimerManageMenu = CreatePopupMenu();
    
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(NULL, L"Resume") : 
                                    GetLocalizedString(NULL, L"Pause");
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || 
                      (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0)));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(NULL, L"Start Over"));
    
    const wchar_t* visibilityText = IsWindowVisible(hwnd) ?
        GetLocalizedString(NULL, L"Hide Window") :
        GetLocalizedString(NULL, L"Show Window");
    
    AppendMenuW(hTimerManageMenu, MF_STRING, CLOCK_IDC_TOGGLE_VISIBILITY, visibilityText);
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerManageMenu,
               GetLocalizedString(NULL, L"Timer Control"));
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    HMENU hTimeMenu = CreatePopupMenu();
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_CURRENT_TIME,
               GetLocalizedString(NULL, L"Show Current Time"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_24HOUR_FORMAT,
               GetLocalizedString(NULL, L"24-Hour Format"));
    
    AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_SHOW_SECONDS,
               GetLocalizedString(NULL, L"Show Seconds"));
    
    AppendMenuW(hMenu, MF_POPUP,
               (UINT_PTR)hTimeMenu,
               GetLocalizedString(NULL, L"Time Display"));

    /* Build Pomodoro submenu using dedicated module */
    BuildPomodoroMenu(hMenu);

    AppendMenuW(hMenu, MF_STRING | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_COUNT_UP_START,
               GetLocalizedString(NULL, L"Count Up"));

    AppendMenuW(hMenu, MF_STRING, 101, 
                GetLocalizedString(NULL, L"Countdown"));

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