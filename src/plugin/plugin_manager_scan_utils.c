/**
 * @file plugin_manager_scan_utils.c
 * @brief Plugin filename filtering, hashing, and recursive enumeration.
 */

#include "plugin_manager_internal.h"
#include "plugin/plugin_extensions.h"

BOOL MatchesPluginPatternExtension(const wchar_t* ext, const char* pattern) {
    if (!ext || !pattern) return FALSE;
    if (pattern[0] == '*') {
        pattern++;
    }

    while (*ext && *pattern) {
        if (ToLowerAsciiW(*ext) != ToLowerAsciiW((wchar_t)(unsigned char)*pattern)) {
            return FALSE;
        }
        ext++;
        pattern++;
    }

    return *ext == L'\0' && *pattern == '\0';
}

BOOL IsSupportedPluginFileW(const wchar_t* fileName) {
    const wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext) return FALSE;

    for (size_t i = 0; i < PLUGIN_EXTENSION_COUNT; i++) {
        if (MatchesPluginPatternExtension(ext, PLUGIN_EXTENSIONS[i])) {
            return TRUE;
        }
    }

    return FALSE;
}

void UpdateLatestWriteTime(FILETIME* target, const FILETIME* candidate) {
    if (!target || !candidate) return;
    if (target->dwLowDateTime == 0 && target->dwHighDateTime == 0) {
        *target = *candidate;
        return;
    }
    if (CompareFileTime(candidate, target) > 0) {
        *target = *candidate;
    }
}

void MixPluginSnapshotHash(PluginDirSnapshot* snapshot, ULONGLONG value) {
    if (!snapshot) return;
    snapshot->contentHash ^= value;
    snapshot->contentHash *= 1099511628211ull;
}

void MixPluginSnapshotPath(PluginDirSnapshot* snapshot, const wchar_t* path) {
    if (!snapshot || !path) return;

    while (*path) {
        MixPluginSnapshotHash(snapshot, (ULONGLONG)ToLowerAsciiW(*path));
        path++;
    }
    MixPluginSnapshotHash(snapshot, 0xffull);
}

BOOL BuildRelativePathW(wchar_t* outPath, size_t outSize,
                               const wchar_t* parentRelativePath,
                               const wchar_t* fileName) {
    if (!outPath || outSize == 0 || !fileName) return FALSE;

    if (!parentRelativePath || parentRelativePath[0] == L'\0') {
        wcsncpy(outPath, fileName, outSize - 1);
        outPath[outSize - 1] = L'\0';
        return wcslen(fileName) < outSize;
    }

    int written = _snwprintf_s(outPath, outSize, _TRUNCATE,
                               L"%s\\%s", parentRelativePath, fileName);
    return written >= 0 && (size_t)written < outSize;
}

BOOL AddPluginEntry(PluginScanContext* ctx,
                           const wchar_t* pluginDir,
                           const wchar_t* fileName,
                           const wchar_t* relativePath) {
    if (!ctx || !pluginDir || !fileName || !relativePath) return FALSE;

    if (ctx->count >= MAX_PLUGINS) {
        if (!ctx->full) {
            LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
        }
        ctx->full = TRUE;
        return FALSE;
    }

    PluginInfo* plugin = &ctx->plugins[ctx->count];
    memset(plugin, 0, sizeof(*plugin));

    wcsncpy(plugin->name, fileName, 63);
    plugin->name[63] = L'\0';
    ExtractDisplayName(fileName, plugin->displayName, 64);

    int pathWritten = _snwprintf_s(plugin->path, MAX_PATH, _TRUNCATE,
                                   L"%s\\%s", pluginDir, relativePath);
    if (pathWritten < 0 || pathWritten >= MAX_PATH) {
        LOG_WARNING("Plugin path is too long: %ls", relativePath);
        return TRUE;
    }

    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));
    ctx->count++;
    return TRUE;
}

void ScanPluginFolderRecursive(const wchar_t* pluginDir,
                                      const wchar_t* folderPath,
                                      const wchar_t* relativePath,
                                      PluginScanContext* ctx,
                                      int depth,
                                      LONG generation) {
    if (!ctx || ctx->full || ctx->failed ||
        IsAsyncScanShuttingDown() ||
        !IsAsyncScanGenerationCurrent(generation)) {
        return;
    }

    if (depth >= MAX_PLUGIN_RECURSION_DEPTH) {
        LOG_WARNING("Plugin scan recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE,
                               L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        LOG_WARNING("Plugin scan path is too long: %ls", folderPath);
        ctx->failed = TRUE;
        return;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND &&
            error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            LOG_WARNING("Plugin folder scan failed: %ls (error=%lu)",
                        folderPath, error);
            ctx->failed = TRUE;
        }
        return;
    }

    BOOL stoppedEarly = FALSE;
    do {
        if (ctx->full || IsAsyncScanShuttingDown() ||
            !IsAsyncScanGenerationCurrent(generation)) {
            stoppedEarly = TRUE;
            break;
        }

        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (++ctx->scannedEntries > MAX_PLUGIN_SCAN_ENTRIES) {
            LOG_WARNING("Plugin directory scan limit reached (%d entries)",
                        MAX_PLUGIN_SCAN_ENTRIES);
            ctx->full = TRUE;
            stoppedEarly = TRUE;
            break;
        }

        wchar_t childRelativePath[MAX_PATH];
        if (!BuildRelativePathW(childRelativePath, MAX_PATH,
                                relativePath, findData.cFileName)) {
            LOG_WARNING("Plugin relative path is too long: %ls", findData.cFileName);
            continue;
        }

        BOOL isDirectory =
            (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory) {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                continue;
            }

            wchar_t childFullPath[MAX_PATH];
            int pathWritten = _snwprintf_s(childFullPath, MAX_PATH, _TRUNCATE,
                                           L"%s\\%s", folderPath, findData.cFileName);
            if (pathWritten < 0 || pathWritten >= MAX_PATH) {
                LOG_WARNING("Plugin folder path is too long: %ls", childRelativePath);
                continue;
            }

            ScanPluginFolderRecursive(pluginDir, childFullPath, childRelativePath,
                                      ctx, depth + 1, generation);
            if (ctx->failed || ctx->full) {
                stoppedEarly = TRUE;
                break;
            }
        } else if (IsSupportedPluginFileW(findData.cFileName)) {
            if (!AddPluginEntry(ctx, pluginDir, findData.cFileName,
                                childRelativePath)) {
                stoppedEarly = TRUE;
                break;
            }
        }
    } while (FindNextFileW(hFind, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        LOG_WARNING("Plugin folder enumeration failed: %ls (error=%lu)",
                    folderPath, findError);
        ctx->failed = TRUE;
    }
}

/**
 * @brief Comparator for plugin sorting (by display name, natural order)
 */
