/**
 * @file config_path.c
 * @brief Configuration path and resource folder management
 *
 * Manages configuration file paths, resource folder creation, and first-run detection.
 */
#include "config.h"
#include "config_path_internal.h"
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
#include <shellapi.h>
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

static BOOL ResolveConfigPathUtf8(char* outPathUtf8, size_t outSize) {
    if (!outPathUtf8 || outSize == 0 || outSize > INT_MAX) return FALSE;
    outPathUtf8[0] = '\0';

    wchar_t ciConfigRoot[MAX_PATH] = {0};
    if (ConfigPath_ResolveCiRootW(ciConfigRoot, _countof(ciConfigRoot)) &&
        ConfigPath_BuildFromLocalAppData(ciConfigRoot, outPathUtf8, outSize)) {
        return TRUE;
    }

    wchar_t effectiveLocalAppData[MAX_PATH] = {0};
    if (ConfigPath_ResolveEffectiveLocalAppDataW(effectiveLocalAppData, MAX_PATH) &&
        ConfigPath_BuildFromLocalAppData(effectiveLocalAppData, outPathUtf8, outSize)) {
        return TRUE;
    }

    /* Never escape the package-private data root when package detection worked. */
    if (IsRunningPackagedApp()) {
        return FALSE;
    }

    return ConfigPath_BuildFromUserProfile(outPathUtf8, outSize);
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
