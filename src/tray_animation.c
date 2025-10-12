/**
 * @file tray_animation.c
 * @brief RunCat-like tray icon animation implementation
 */

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "../include/tray.h"
#include "../include/config.h"
#include "../include/tray_menu.h"

/**
 * @brief Timer ID for tray animation
 */
#define TRAY_ANIM_TIMER_ID 42420

/** @brief Max frames supported */
#define MAX_TRAY_FRAMES 64

/** @brief Loaded icon frames and state */
static HICON g_trayIcons[MAX_TRAY_FRAMES];
static int g_trayIconCount = 0;
static int g_trayIconIndex = 0;
static UINT g_trayInterval = 0;
static HWND g_trayHwnd = NULL;
static char g_animationName[64] = "cat"; /** current folder under animations */

/** @brief Build cat animation folder path: %LOCALAPPDATA%\Catime\resources\animations\cat */
static void BuildAnimationFolder(const char* name, char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%s%s", base, name);
    } else {
        snprintf(path, size, "%s\\%s", base, name);
    }
}

/** @brief Free all loaded icon frames */
static void FreeTrayIcons(void) {
    for (int i = 0; i < g_trayIconCount; ++i) {
        if (g_trayIcons[i]) {
            DestroyIcon(g_trayIcons[i]);
            g_trayIcons[i] = NULL;
        }
    }
    g_trayIconCount = 0;
    g_trayIconIndex = 0;
}

/** @brief Load sequential .ico files starting from 1.ico upward */
static void LoadTrayIcons(void) {
    FreeTrayIcons();

    char folder[MAX_PATH] = {0};
    BuildAnimationFolder(g_animationName, folder, sizeof(folder));

    /** Enumerate all .ico files, pick those whose base name is a positive integer, sort ascending */
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wFolder, MAX_PATH);

    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*.ico", wFolder);

    typedef struct { int hasNum; int num; wchar_t name[MAX_PATH]; wchar_t path[MAX_PATH]; } AnimFile;
    AnimFile files[MAX_TRAY_FRAMES];
    int fileCount = 0;

    WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            /** Extract filename without extension */
            wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
            if (!dot) continue;
            size_t nameLen = (size_t)(dot - ffd.cFileName);
            if (nameLen == 0 || nameLen >= MAX_PATH) continue;

            int hasNum = 0;
            int numVal = 0;
            /** Find first continuous digit run and parse as number */
            for (size_t i = 0; i < nameLen; ++i) {
                if (iswdigit(ffd.cFileName[i])) {
                    hasNum = 1;
                    numVal = 0;
                    while (i < nameLen && iswdigit(ffd.cFileName[i])) {
                        numVal = numVal * 10 + (ffd.cFileName[i] - L'0');
                        i++;
                    }
                    break;
                }
            }

            if (fileCount < MAX_TRAY_FRAMES) {
                files[fileCount].hasNum = hasNum;
                files[fileCount].num = numVal;
                wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                files[fileCount].name[nameLen] = L'\0';
                _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                fileCount++;
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    if (fileCount == 0) {
        return;
    }

    int cmpAnimFile(const void* a, const void* b) {
        const AnimFile* fa = (const AnimFile*)a;
        const AnimFile* fb = (const AnimFile*)b;
        if (fa->hasNum && fb->hasNum) {
            if (fa->num < fb->num) return -1;
            if (fa->num > fb->num) return 1;
            /** tie-breaker by name */
            return _wcsicmp(fa->name, fb->name);
        }
        /** fallback: case-insensitive name compare */
        return _wcsicmp(fa->name, fb->name);
    }

    qsort(files, (size_t)fileCount, sizeof(AnimFile), cmpAnimFile);

    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        if (hIcon) {
            g_trayIcons[g_trayIconCount++] = hIcon;
        }
    }
}

/** @brief Advance to next icon frame and apply to tray */
static void AdvanceTrayFrame(void) {
    if (!g_trayHwnd || g_trayIconCount <= 0) return;
    if (g_trayIconIndex >= g_trayIconCount) g_trayIconIndex = 0;

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_trayIcons[g_trayIconIndex];

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    g_trayIconIndex = (g_trayIconIndex + 1) % g_trayIconCount;
}

/** @brief Window-proc level timer callback shim */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;
    AdvanceTrayFrame();
}

void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_trayInterval = intervalMs > 0 ? intervalMs : 150; /** default ~6-7 fps */

    /** Read current animation name from config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[64] = {0};
    ReadIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", "%LOCALAPPDATA%\\Catime\\resources\\animations\\cat", nameBuf, sizeof(nameBuf), config_path);
    if (nameBuf[0] != '\0') {
        const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
        if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
                g_animationName[sizeof(g_animationName) - 1] = '\0';
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        }
    }

    LoadTrayIcons();

    if (g_trayIconCount > 0) {
        AdvanceTrayFrame();
        SetTimer(hwnd, TRAY_ANIM_TIMER_ID, g_trayInterval, (TIMERPROC)TrayAnimTimerProc);
    }
}

void StopTrayAnimation(HWND hwnd) {
    KillTimer(hwnd, TRAY_ANIM_TIMER_ID);
    FreeTrayIcons();
    g_trayHwnd = NULL;
}

/**
 * @brief Get current animation folder name
 */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

/**
 * @brief Set and persist current animation folder; reload frames
 */
BOOL SetCurrentAnimationName(const char* name) {
    if (!name || !*name) return FALSE;

    /** Validate the folder contains any numeric-named .ico */
    char folder[MAX_PATH] = {0};
    BuildAnimationFolder(name, folder, sizeof(folder));
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wFolder, MAX_PATH);
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*.ico", wFolder);
    BOOL hasAny = FALSE;
    WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
            if (!dot) continue;
            size_t nameLen = (size_t)(dot - ffd.cFileName);
            if (nameLen == 0) continue;
            BOOL allDigits = TRUE;
            for (size_t i = 0; i < nameLen; ++i) { if (!iswdigit(ffd.cFileName[i])) { allDigits = FALSE; break; } }
            if (allDigits) { hasAny = TRUE; break; }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }
    if (!hasAny) return FALSE;

    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    /** Persist to config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", g_animationName);
    WriteIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", animPath, config_path);

    /** Reload frames and reset index; ensure timer is running */
    LoadTrayIcons();
    g_trayIconIndex = 0;
    if (g_trayHwnd && g_trayIconCount > 0) {
        AdvanceTrayFrame();
        if (!IsWindow(g_trayHwnd)) return TRUE;
        KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
        SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, g_trayInterval, (TIMERPROC)TrayAnimTimerProc);
    }
    return TRUE;
}


static void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH);
    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wRoot);

        WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (nextId == id) {
                        char folderUtf8[MAX_PATH] = {0};
                        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, folderUtf8, MAX_PATH, NULL, NULL);
                        if (SetCurrentAnimationName(folderUtf8)) {
                            return TRUE;
                        }
                        break;
                    }
                    nextId++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
        return TRUE;
    }
    return FALSE;
}

