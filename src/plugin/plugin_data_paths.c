/**
 * @file plugin_data_paths.c
 * @brief Plugin output-directory and source-path helpers.
 */

#include "plugin_data_internal.h"

/*
 * Design note: output.txt is a small local IPC surface scoped to the active
 * plugin folder. Catime starts the plugin script and displays that folder's
 * output.txt while plugin mode is active. Outside plugin mode, Catime
 * intentionally ignores this file.
 * Catime-owned local state text such as "Loading..." or "FAIL" is also allowed
 * to override the file-driven content when needed.
 */
/* Plugin output file name */
#define PLUGIN_OUTPUT_FILENAME "output.txt"
#define PLUGIN_OUTPUT_FILENAME_W L"output.txt"

BOOL GetDefaultPluginOutputDirectoryW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)INT_MAX) {
        return FALSE;
    }
    buffer[0] = L'\0';

    char pluginDirUtf8[MAX_PATH] = {0};
    GetPluginsFolderPath(pluginDirUtf8, MAX_PATH);
    if (pluginDirUtf8[0] == '\0' ||
        MultiByteToWideChar(CP_UTF8, 0, pluginDirUtf8, -1,
                            buffer, (int)bufferSize) <= 0) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

BOOL SetDefaultPluginOutputDirectoryLocked(void) {
    wchar_t defaultDir[MAX_PATH];
    if (!GetDefaultPluginOutputDirectoryW(defaultDir, MAX_PATH)) {
        g_pluginOutputDirectory[0] = L'\0';
        return FALSE;
    }

    wcsncpy(g_pluginOutputDirectory, defaultDir, MAX_PATH - 1);
    g_pluginOutputDirectory[MAX_PATH - 1] = L'\0';
    return TRUE;
}

BOOL EnsurePluginOutputDirectoryLocked(void) {
    if (g_pluginOutputDirectory[0] != L'\0') {
        return TRUE;
    }
    return SetDefaultPluginOutputDirectoryLocked();
}

BOOL GetPluginOutputDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    EnterCriticalSection(&g_dataCS);
    BOOL ok = EnsurePluginOutputDirectoryLocked();
    if (ok) {
        wcsncpy(buffer, g_pluginOutputDirectory, bufferSize - 1);
        buffer[bufferSize - 1] = L'\0';
        ok = wcslen(g_pluginOutputDirectory) < bufferSize;
    }
    LeaveCriticalSection(&g_dataCS);
    return ok;
}

/**
 * @brief Get plugin output file path
 * @return TRUE if successful, FALSE otherwise
 */
BOOL GetPluginOutputPathW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    wchar_t outputDir[MAX_PATH];
    if (!GetPluginOutputDirectory(outputDir, MAX_PATH)) {
        buffer[0] = L'\0';
        return FALSE;
    }

    int written = _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                               L"%s\\%s", outputDir, PLUGIN_OUTPUT_FILENAME_W);
    if (written < 0 || (size_t)written >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

void SetDisplaySourcePathLocked(const wchar_t* sourcePath) {
    if (!sourcePath || sourcePath[0] == L'\0' || wcslen(sourcePath) >= MAX_PATH) {
        g_displaySourcePath[0] = L'\0';
        return;
    }

    wcsncpy(g_displaySourcePath, sourcePath, MAX_PATH - 1);
    g_displaySourcePath[MAX_PATH - 1] = L'\0';
}

BOOL GetDirectoryFromPathW(const wchar_t* path,
                                  wchar_t* directory,
                                  size_t directorySize) {
    if (!path || !directory || directorySize == 0) return FALSE;
    directory[0] = L'\0';
    size_t pathLen = wcslen(path);
    if (pathLen == 0 || pathLen >= directorySize) return FALSE;

    wcsncpy(directory, path, directorySize - 1);
    directory[directorySize - 1] = L'\0';

    wchar_t* lastSlash = wcsrchr(directory, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(directory, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == directory) {
        directory[0] = L'\0';
        return FALSE;
    }

    *lastSlash = L'\0';
    return directory[0] != L'\0';
}

/**
 * @brief Ensure plugin output directory exists
 * @note Only creates directory, does NOT clear or modify output.txt
 *       User may have pre-defined content in output.txt
 */
void EnsureOutputDirExistsW(const wchar_t* filePath) {
    /* First ensure the directory exists */
    wchar_t dirPath[MAX_PATH];
    if (!filePath || wcslen(filePath) >= MAX_PATH) return;
    wcsncpy(dirPath, filePath, MAX_PATH - 1);
    dirPath[MAX_PATH - 1] = L'\0';

    /* Find last backslash to get directory path */
    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        /* Create directory (and parent directories if needed) */
        /* SHCreateDirectoryExW creates all intermediate directories */
        SHCreateDirectoryExW(NULL, dirPath, NULL);
    }
}
