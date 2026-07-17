/**
 * @file config_path.c
 * @brief Configuration path and resource folder management
 *
 * Manages configuration file paths, resource folder creation, and first-run detection.
 */
#include "config.h"
#include "utils/string_convert.h"
#include "utils/path_utils.h"
#include "utils/package_identity.h"
#include "utils/win32_dynamic_loader.h"
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

static BOOL GetUserProfilePathW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", outPath, (DWORD)outSize);
    if (len > 0 && len < outSize) {
        return TRUE;
    }

    outPath[0] = L'\0';
    return SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, outPath));
}

static BOOL ResolveStandardLocalAppDataW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    PWSTR knownFolderPath = NULL;
    HRESULT hr = E_FAIL;

    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        typedef HRESULT (WINAPI *PFN_SHGetKnownFolderPath)(const GUID*, DWORD, HANDLE, PWSTR*);
        PFN_SHGetKnownFolderPath pfn = NULL;
        CATIME_LOAD_PROC_ADDRESS(hShell, "SHGetKnownFolderPath", pfn);
        if (pfn) {
            static const GUID folderIdLocalAppDataGuid = {
                0xF1B32785, 0x6FBA, 0x4FCF,
                {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91}
            };
            hr = pfn(&folderIdLocalAppDataGuid, 0, NULL, &knownFolderPath);
        }
        FreeLibrary(hShell);
    }

    if (SUCCEEDED(hr) && knownFolderPath) {
        BOOL fits = wcslen(knownFolderPath) < outSize;
        if (fits) {
            wcscpy_s(outPath, outSize, knownFolderPath);
        }
        CoTaskMemFree(knownFolderPath);
        if (fits) return TRUE;
    }

    return SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, outPath));
}

static BOOL ResolveEffectiveLocalAppDataW(wchar_t* outPath, size_t outSize) {
    if (!outPath || outSize == 0 || outSize > MAXDWORD) return FALSE;
    outPath[0] = L'\0';

    if (!IsRunningPackagedApp()) {
        return ResolveStandardLocalAppDataW(outPath, outSize);
    }

    wchar_t userProfile[MAX_PATH] = {0};
    wchar_t packageFamily[MAX_PATH] = {0};
    if (!GetUserProfilePathW(userProfile, MAX_PATH) ||
        !GetCurrentPackageFamilyNameSafeW(packageFamily, MAX_PATH)) {
        return FALSE;
    }

    int written = _snwprintf_s(
        outPath, outSize, _TRUNCATE,
        L"%s\\AppData\\Local\\Packages\\%s\\LocalCache\\Local",
        userProfile, packageFamily);
    if (written < 0) {
        outPath[0] = L'\0';
        return FALSE;
    }

    int createResult = SHCreateDirectoryExW(NULL, outPath, NULL);
    return IsDirectoryCreateResultOk(createResult, outPath);
}

BOOL GetEffectiveLocalAppDataPath(char* path, size_t size) {
    if (!path || size == 0 || size > INT_MAX) return FALSE;
    path[0] = '\0';

    wchar_t effectivePath[MAX_PATH] = {0};
    if (!ResolveEffectiveLocalAppDataW(effectivePath, MAX_PATH)) {
        return FALSE;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, effectivePath, -1,
                            path, (int)size, NULL, NULL) <= 0) {
        path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL ExpandEffectiveLocalAppDataPath(const char* value,
                                     char* expanded,
                                     size_t expandedSize) {
    if (!value || !expanded || expandedSize == 0) return FALSE;
    expanded[0] = '\0';

    static const char token[] = "%LOCALAPPDATA%";
    size_t tokenLen = sizeof(token) - 1;
    if (_strnicmp(value, token, tokenLen) != 0) {
        if (strlen(value) >= expandedSize) return FALSE;
        strcpy_s(expanded, expandedSize, value);
        return TRUE;
    }

    const char* suffix = value + tokenLen;
    char expansionRoot[MAX_PATH] = {0};

    /*
     * Older Store configurations may already contain the full package-private
     * suffix after %LOCALAPPDATA%. Expanding those against the effective root
     * would duplicate Packages\\<PFN>\\LocalCache\\Local.
     */
    if (IsRunningPackagedApp()) {
        wchar_t packageFamilyW[MAX_PATH] = {0};
        char packageFamilyUtf8[MAX_PATH] = {0};
        char legacyPrivatePrefix[MAX_PATH] = {0};
        if (GetCurrentPackageFamilyNameSafeW(packageFamilyW, MAX_PATH) &&
            WideCharToMultiByte(CP_UTF8, 0, packageFamilyW, -1,
                                packageFamilyUtf8, MAX_PATH, NULL, NULL) > 0 &&
            snprintf(legacyPrivatePrefix, sizeof(legacyPrivatePrefix),
                     "\\Packages\\%s\\LocalCache\\Local", packageFamilyUtf8) > 0 &&
            _strnicmp(suffix, legacyPrivatePrefix,
                      strlen(legacyPrivatePrefix)) == 0) {
            wchar_t standardRootW[MAX_PATH] = {0};
            if (!ResolveStandardLocalAppDataW(standardRootW, MAX_PATH) ||
                WideCharToMultiByte(CP_UTF8, 0, standardRootW, -1,
                                    expansionRoot, MAX_PATH, NULL, NULL) <= 0) {
                return FALSE;
            }
        }
    }

    if (expansionRoot[0] == '\0' &&
        !GetEffectiveLocalAppDataPath(expansionRoot, sizeof(expansionRoot))) {
        return FALSE;
    }

    int written = snprintf(expanded, expandedSize, "%s%s",
                           expansionRoot, suffix);
    if (written < 0 || (size_t)written >= expandedSize) {
        expanded[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL ResolveConfigPathUtf8(char* outPathUtf8, size_t outSize) {
    if (!outPathUtf8 || outSize == 0 || outSize > INT_MAX) return FALSE;
    outPathUtf8[0] = '\0';

    wchar_t effectiveLocalAppData[MAX_PATH] = {0};
    if (ResolveEffectiveLocalAppDataW(effectiveLocalAppData, MAX_PATH) &&
        BuildConfigPathFromLocalAppData(effectiveLocalAppData, outPathUtf8, outSize)) {
        return TRUE;
    }

    /* Never escape the package-private data root when package detection worked. */
    if (IsRunningPackagedApp()) {
        return FALSE;
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

    /* Avoid project logging here: log initialization itself depends on this path. */
    OutputDebugStringA("Catime: failed to determine configuration path\n");
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

