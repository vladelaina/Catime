/**
 * @file config_path.c
 * @brief Configuration path and resource folder management
 *
 * Manages configuration file paths, resource folder creation, and first-run detection.
 */
#include "config.h"
#include "utils/string_convert.h"
#include "utils/path_utils.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>

#define CONFIG_PATH_UNINITIALIZED 0
#define CONFIG_PATH_INITIALIZING  1
#define CONFIG_PATH_INITIALIZED   2
#define CONFIG_PATH_INIT_SPINS    64

static char g_cachedConfigPath[MAX_PATH] = {0};
static wchar_t g_cachedConfigBaseDir[MAX_PATH] = {0};
static volatile LONG g_configPathInitState = CONFIG_PATH_UNINITIALIZED;

static void CopyConfigPathOut(char* path, size_t size, const char* value) {
    if (!path || size == 0) return;
    if (!value || strlen(value) >= size) {
        path[0] = '\0';
        return;
    }
    strcpy_s(path, size, value);
}

static void WaitForConfigPathInit(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_configPathInitState, 0, 0) ==
           CONFIG_PATH_INITIALIZING) {
        Sleep(spins++ < CONFIG_PATH_INIT_SPINS ? 0 : 1);
    }
}

static BOOL EnsureDirectoryExistsW(const wchar_t* path) {
    if (!path || !*path) return FALSE;

    if (CreateDirectoryW(path, NULL)) {
        return TRUE;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL IsDirectoryCreateResultOk(int createResult, const wchar_t* path) {
    if (createResult == ERROR_SUCCESS) {
        return TRUE;
    }

    if (createResult != ERROR_ALREADY_EXISTS && createResult != ERROR_FILE_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL BuildConfigPathFromLocalAppData(const wchar_t* wLocalAppData,
                                            char* outPathUtf8,
                                            size_t outSize) {
    if (!outPathUtf8 || outSize == 0) {
        return FALSE;
    }
    outPathUtf8[0] = '\0';
    if (!wLocalAppData || !*wLocalAppData || outSize > INT_MAX) {
        return FALSE;
    }

    wchar_t wDir[MAX_PATH] = {0};
    if (_snwprintf_s(wDir, MAX_PATH, _TRUNCATE, L"%s\\Catime", wLocalAppData) < 0 ||
        !EnsureDirectoryExistsW(wDir)) {
        return FALSE;
    }

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (_snwprintf_s(wConfigPath, MAX_PATH, _TRUNCATE,
                     L"%s\\Catime\\config.ini", wLocalAppData) < 0) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wConfigPath, -1,
                            outPathUtf8, (int)outSize, NULL, NULL) <= 0) {
        outPathUtf8[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL BuildConfigPathFromUserProfile(char* outPathUtf8, size_t outSize) {
    if (!outPathUtf8 || outSize == 0) {
        return FALSE;
    }
    outPathUtf8[0] = '\0';
    if (outSize > INT_MAX) {
        return FALSE;
    }

    wchar_t wUserProfile[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", wUserProfile, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return FALSE;
    }

    wchar_t wDir[MAX_PATH] = {0};
    if (_snwprintf_s(wDir, MAX_PATH, _TRUNCATE,
                     L"%s\\AppData\\Local\\Catime", wUserProfile) < 0 ||
        !EnsureDirectoryExistsW(wDir)) {
        return FALSE;
    }

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (_snwprintf_s(wConfigPath, MAX_PATH, _TRUNCATE,
                     L"%s\\AppData\\Local\\Catime\\config.ini", wUserProfile) < 0) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wConfigPath, -1,
                            outPathUtf8, (int)outSize, NULL, NULL) <= 0) {
        outPathUtf8[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL ResolveConfigPathUtf8(char* outPathUtf8, size_t outSize) {
    if (!outPathUtf8 || outSize == 0 || outSize > INT_MAX) return FALSE;
    outPathUtf8[0] = '\0';

    PWSTR wLocalAppData = NULL;
    HRESULT hr = E_FAIL;

    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        typedef HRESULT (WINAPI *PFN_SHGetKnownFolderPath)(const GUID*, DWORD, HANDLE, PWSTR*);
        PFN_SHGetKnownFolderPath pfn =
            (PFN_SHGetKnownFolderPath)GetProcAddress(hShell, "SHGetKnownFolderPath");
        if (pfn) {
            static const GUID folderIdLocalAppDataGuid = {
                0xF1B32785, 0x6FBA, 0x4FCF,
                {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91}
            };
            hr = pfn(&folderIdLocalAppDataGuid, 0, NULL, &wLocalAppData);
        }
        FreeLibrary(hShell);
    }

    if (SUCCEEDED(hr) && wLocalAppData) {
        BOOL ok = BuildConfigPathFromLocalAppData(wLocalAppData, outPathUtf8, outSize);
        CoTaskMemFree(wLocalAppData);
        if (ok) return TRUE;
    }

    wchar_t wLegacy[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, wLegacy)) &&
        BuildConfigPathFromLocalAppData(wLegacy, outPathUtf8, outSize)) {
        return TRUE;
    }

    return BuildConfigPathFromUserProfile(outPathUtf8, outSize);
}

static BOOL BuildConfigBaseDirFromPathUtf8(const char* configPathUtf8,
                                           wchar_t* outDir,
                                           size_t outDirSize) {
    if (!configPathUtf8 || !*configPathUtf8 ||
        !outDir || outDirSize == 0 || outDirSize > INT_MAX) {
        return FALSE;
    }
    outDir[0] = L'\0';

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1,
                            wConfigPath, MAX_PATH) == 0) {
        return FALSE;
    }

    wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
    if (!lastSep) {
        lastSep = wcsrchr(wConfigPath, L'/');
    }
    if (!lastSep) {
        return FALSE;
    }
    *lastSep = L'\0';

    if (wcslen(wConfigPath) >= outDirSize) {
        return FALSE;
    }
    wcscpy_s(outDir, outDirSize, wConfigPath);
    return TRUE;
}

static BOOL GetConfigBaseDirW(wchar_t* outDir, size_t outDirSize) {
    if (!outDir || outDirSize == 0 || outDirSize > INT_MAX) return FALSE;
    outDir[0] = L'\0';

    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, sizeof(configPathUtf8));
    if (configPathUtf8[0] == '\0') {
        return FALSE;
    }

    if (g_cachedConfigBaseDir[0] != L'\0') {
        if (wcslen(g_cachedConfigBaseDir) >= outDirSize) {
            return FALSE;
        }
        wcscpy_s(outDir, outDirSize, g_cachedConfigBaseDir);
        return TRUE;
    }

    return BuildConfigBaseDirFromPathUtf8(configPathUtf8, outDir, outDirSize);
}

/**
 * @brief Get configuration file path with automatic directory creation
 */
void GetConfigPath(char* path, size_t size) {
    if (!path || size == 0) return;

    if (InterlockedCompareExchange(&g_configPathInitState, 0, 0) ==
        CONFIG_PATH_INITIALIZED) {
        CopyConfigPathOut(path, size, g_cachedConfigPath);
        return;
    }

    if (InterlockedCompareExchange(&g_configPathInitState,
                                   CONFIG_PATH_INITIALIZING,
                                   CONFIG_PATH_UNINITIALIZED) !=
        CONFIG_PATH_UNINITIALIZED) {
        WaitForConfigPathInit();
        if (InterlockedCompareExchange(&g_configPathInitState, 0, 0) ==
            CONFIG_PATH_INITIALIZED) {
            CopyConfigPathOut(path, size, g_cachedConfigPath);
        } else {
            path[0] = '\0';
        }
        return;
    }

    if (ResolveConfigPathUtf8(g_cachedConfigPath, sizeof(g_cachedConfigPath)) &&
        BuildConfigBaseDirFromPathUtf8(g_cachedConfigPath,
                                       g_cachedConfigBaseDir,
                                       MAX_PATH)) {
        InterlockedExchange(&g_configPathInitState, CONFIG_PATH_INITIALIZED);
        CopyConfigPathOut(path, size, g_cachedConfigPath);
        return;
    }

    /* Critical failure: cannot determine config path */
    LOG_ERROR("Failed to determine configuration path - all methods failed (SHGetKnownFolderPath, SHGetFolderPathW, USERPROFILE)");
    g_cachedConfigPath[0] = '\0';
    g_cachedConfigBaseDir[0] = '\0';
    InterlockedExchange(&g_configPathInitState, CONFIG_PATH_UNINITIALIZED);
    path[0] = '\0';
}


/**
 * @brief Build full path to a resources subfolder and ensure it exists
 */
static void GetResourceSubfolderPathUtf8(const wchar_t* wSubFolder, char* outPathUtf8, size_t outSize) {
    if (outPathUtf8 && outSize > 0) {
        outPathUtf8[0] = '\0';
    }

    wchar_t wConfigDir[MAX_PATH] = {0};
    if (!GetConfigBaseDirW(wConfigDir, MAX_PATH)) {
        return;
    }

    wchar_t wFolder[MAX_PATH] = {0};
    if (_snwprintf_s(wFolder, MAX_PATH, _TRUNCATE, L"%s\\%s",
                     wConfigDir, wSubFolder) < 0) {
        return;
    }

    /** Ensure directory exists (creates intermediate directories) */
    int createResult = SHCreateDirectoryExW(NULL, wFolder, NULL);
    if (!IsDirectoryCreateResultOk(createResult, wFolder)) {
        LOG_WARNING("Failed to create resource folder: %ls (error=%d)",
                    wFolder, createResult);
        return;
    }

    if (outPathUtf8 && outSize > 0) {
        if (outSize > INT_MAX ||
            WideCharToMultiByte(CP_UTF8, 0, wFolder, -1,
                                outPathUtf8, (int)outSize, NULL, NULL) == 0) {
            outPathUtf8[0] = '\0';
        }
    }
}

/**
 * @brief Ensure default resources subfolder structure exists
 */
static void EnsureDefaultResourceSubfolders(void) {
    const wchar_t* subfolders[] = {
        L"resources",
        L"resources\\audio",
        L"resources\\fonts",
        L"resources\\animations",
        L"resources\\plugins"
    };
    for (size_t i = 0; i < sizeof(subfolders)/sizeof(subfolders[0]); ++i) {
        GetResourceSubfolderPathUtf8(subfolders[i], NULL, 0);
    }
}


/**
 * @brief Extract filename from full path with UTF-8 support
 */
void ExtractFileName(const char* path, char* name, size_t nameSize) {
    ExtractFileNameU8(path, name, nameSize);
}


/**
 * @brief Create resource folder structure in config directory
 */
void CheckAndCreateResourceFolders() {
    char config_path[MAX_PATH] = {0};
    
    GetConfigPath(config_path, MAX_PATH);
    if (config_path[0] != '\0') {
        EnsureDefaultResourceSubfolders();
    }
}


/**
 * @brief Check if this is the first run of the application
 */
BOOL IsFirstRun(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    if (!FileExists(config_path)) {
        return TRUE;
    }
    
    char firstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", firstRun, sizeof(firstRun), config_path);
    
    return (strcmp(firstRun, "TRUE") == 0);
}

/**
 * @brief Set first run flag to FALSE
 */
BOOL SetFirstRunCompleted(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    return WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", config_path);
}


/**
 * @brief Get audio resources folder path with automatic directory creation
 */
void GetAudioFolderPath(char* path, size_t size) {
    if (!path || size == 0) return;
    GetResourceSubfolderPathUtf8(L"resources\\audio", path, size);
}


/**
 * @brief Get animations resources folder path and ensure it exists
 */
void GetAnimationsFolderPath(char* path, size_t size) {
    if (!path || size == 0) return;
    GetResourceSubfolderPathUtf8(L"resources\\animations", path, size);
}


/**
 * @brief Get plugins resources folder path and ensure it exists
 */
void GetPluginsFolderPath(char* path, size_t size) {
    if (!path || size == 0) return;
    GetResourceSubfolderPathUtf8(L"resources\\plugins", path, size);
}


/**
 * @brief Check if desktop shortcut verification has been completed
 */
BOOL IsShortcutCheckDone(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Read shortcut check status from general section */
    return ReadIniBool(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", FALSE, config_path);
}

/**
 * @brief Mark desktop shortcut verification as completed
 */
BOOL SetShortcutCheckDone(BOOL done) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    /** Write shortcut check status to general section */
    return WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE",
                          done ? "TRUE" : "FALSE", config_path);
}

