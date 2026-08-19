/**
 * @file startup_shortcut_paths.c
 * @brief Startup shortcut paths, legacy marker cleanup, and process locking
 */
#include "startup_internal.h"

#include "config.h"
#include "log.h"
#include "utils/path_utils.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <wchar.h>

#define STARTUP_MARKER_FILENAME L"startup_shortcut_target.txt"
#define STARTUP_SHORTCUT_MUTEX_NAME \
    L"Local\\Vladelaina.Catime.StartupShortcut"
#define STARTUP_SHORTCUT_LOCK_TIMEOUT_MS 250

BOOL StartupPaths_GetShortcutPath(wchar_t* output, size_t outputSize) {
    wchar_t startupFolder[MAX_PATH] = {0};
    HRESULT result;

    if (!output || outputSize < MAX_PATH ||
        outputSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    output[0] = L'\0';

    result = SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupFolder);
    if (FAILED(result)) {
        LOG_ERROR("Failed to get startup folder path, hr=0x%08X",
                  (unsigned int)result);
        return FALSE;
    }
    if (!PathCombineW(output, startupFolder, STARTUP_LINK_FILENAME)) {
        LOG_ERROR("Failed to combine startup path");
        return FALSE;
    }
    return TRUE;
}

BOOL StartupPaths_GetExecutablePath(wchar_t* output, size_t outputSize) {
    if (!GetShortcutExecutablePathW(output, outputSize)) {
        LOG_ERROR("Failed to get executable path");
        return FALSE;
    }
    return TRUE;
}

static BOOL GetLegacyMarkerPath(wchar_t* output, size_t outputSize) {
    char configPath[MAX_PATH] = {0};
    wchar_t configPathW[MAX_PATH] = {0};
    wchar_t configDir[MAX_PATH] = {0};
    wchar_t* lastSlash;
    wchar_t* lastForwardSlash;

    if (!output || outputSize < MAX_PATH ||
        outputSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    output[0] = L'\0';

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') return FALSE;
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, configPathW,
                            (int)_countof(configPathW)) == 0) {
        LOG_WARNING("Failed to convert config path for startup shortcut marker");
        return FALSE;
    }
    if (wcscpy_s(configDir, _countof(configDir), configPathW) != 0) {
        return FALSE;
    }

    lastSlash = wcsrchr(configDir, L'\\');
    lastForwardSlash = wcsrchr(configDir, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == configDir) return FALSE;
    *lastSlash = L'\0';

    if (!PathCombineW(output, configDir, STARTUP_MARKER_FILENAME)) {
        LOG_WARNING("Failed to build startup shortcut marker path");
        return FALSE;
    }
    return TRUE;
}

void StartupPaths_RemoveLegacyMarker(void) {
    wchar_t markerPath[MAX_PATH] = {0};
    if (GetLegacyMarkerPath(markerPath, _countof(markerPath))) {
        (void)DeleteFileW(markerPath);
    }
}

BOOL StartupPaths_AcquireLock(StartupShortcutLock* lock) {
    DWORD waitResult;

    if (!lock) return FALSE;
    lock->handle = CreateMutexW(NULL, FALSE, STARTUP_SHORTCUT_MUTEX_NAME);
    lock->acquired = FALSE;
    if (!lock->handle) {
        LOG_WARNING("Failed to create startup shortcut mutex, error=%lu",
                    GetLastError());
        return FALSE;
    }

    waitResult = WaitForSingleObject(lock->handle,
                                     STARTUP_SHORTCUT_LOCK_TIMEOUT_MS);
    if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
        lock->acquired = TRUE;
        return TRUE;
    }

    LOG_WARNING("Failed to acquire startup shortcut mutex, result=%lu error=%lu",
                waitResult, GetLastError());
    CloseHandle(lock->handle);
    lock->handle = NULL;
    return FALSE;
}

void StartupPaths_ReleaseLock(StartupShortcutLock* lock) {
    if (!lock || !lock->handle) return;
    if (lock->acquired) ReleaseMutex(lock->handle);
    CloseHandle(lock->handle);
    lock->handle = NULL;
    lock->acquired = FALSE;
}
