/**
 * @file plugin_manager_scan_snapshot.c
 * @brief Directory snapshot collection and failure cache.
 */

#include "plugin_manager_internal.h"

BOOL ScanPluginDirSnapshotRecursive(const wchar_t* folderPath,
                                           const wchar_t* relativePath,
                                           PluginDirSnapshot* snapshot,
                                           int depth) {
    if (!folderPath || !snapshot) return FALSE;

    if (depth >= MAX_PLUGIN_RECURSION_DEPTH) {
        snapshot->truncated = TRUE;
        return TRUE;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE,
                               L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        snapshot->truncated = TRUE;
        return TRUE;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND &&
            error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            LOG_WARNING("Failed to snapshot plugin folder: %ls (error=%lu)",
                        folderPath, error);
            return FALSE;
        }
        return TRUE;
    }

    BOOL ok = TRUE;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (++snapshot->scannedEntries > MAX_PLUGIN_SCAN_ENTRIES) {
            snapshot->truncated = TRUE;
            break;
        }

        wchar_t childRelativePath[MAX_PATH];
        if (!BuildRelativePathW(childRelativePath, MAX_PATH,
                                relativePath, findData.cFileName)) {
            snapshot->truncated = TRUE;
            continue;
        }

        BOOL isDirectory =
            (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory &&
            !(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            wchar_t childPath[MAX_PATH];
            int pathWritten = _snwprintf_s(childPath, MAX_PATH, _TRUNCATE,
                                           L"%s\\%s", folderPath, findData.cFileName);
            if (pathWritten < 0 || pathWritten >= MAX_PATH) {
                snapshot->truncated = TRUE;
                continue;
            }
            if (!ScanPluginDirSnapshotRecursive(childPath, childRelativePath,
                                                snapshot, depth + 1)) {
                ok = FALSE;
                break;
            }
            if (snapshot->truncated) {
                break;
            }
        } else if (!isDirectory && IsSupportedPluginFileW(findData.cFileName)) {
            snapshot->entryCount++;
            UpdateLatestWriteTime(&snapshot->lastWriteTime, &findData.ftLastWriteTime);
            MixPluginSnapshotPath(snapshot, childRelativePath);
            MixPluginSnapshotHash(snapshot,
                                  ((ULONGLONG)findData.ftLastWriteTime.dwHighDateTime << 32) |
                                  findData.ftLastWriteTime.dwLowDateTime);
            MixPluginSnapshotHash(snapshot,
                                  ((ULONGLONG)findData.nFileSizeHigh << 32) |
                                  findData.nFileSizeLow);
        }
    } while (FindNextFileW(hFind, &findData));

    DWORD findError = snapshot->truncated ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (ok && findError != ERROR_NO_MORE_FILES) {
        LOG_WARNING("Plugin snapshot enumeration failed: %ls (error=%lu)",
                    folderPath, findError);
        ok = FALSE;
    }
    return ok;
}

BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    ZeroMemory(snapshot, sizeof(*snapshot));
    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(pluginDir, GetFileExInfoStandard, &attrs)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            LOG_WARNING("Failed to stat plugin directory snapshot (error=%lu)", error);
            return FALSE;
        }
        snapshot->exists = FALSE;
        return TRUE;
    }

    if (!(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        snapshot->exists = FALSE;
        return TRUE;
    }

    snapshot->exists = TRUE;
    snapshot->contentHash = 1469598103934665603ull;
    return ScanPluginDirSnapshotRecursive(pluginDir, L"", snapshot, 0);
}

BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                                    const PluginDirSnapshot* b) {
    if (!a || !b) return FALSE;
    if (a->exists != b->exists) return FALSE;
    if (!a->exists) return TRUE;
    if (a->entryCount != b->entryCount) return FALSE;
    if (a->contentHash != b->contentHash) return FALSE;
    if (a->truncated != b->truncated) return FALSE;

    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime) == 0;
}

BOOL IsAsyncScanFailureRecentlyCachedLocked(BOOL hasSnapshot,
                                                   const PluginDirSnapshot* snapshot,
                                                   DWORD now) {
    DWORD cooldownUntil =
        (DWORD)InterlockedCompareExchange(&g_asyncScanFailureCooldownUntil, 0, 0);
    if (!g_asyncScanHasFailureSnapshot ||
        cooldownUntil == 0 ||
        (LONG)(cooldownUntil - now) <= 0) {
        return FALSE;
    }

    if (g_asyncScanFailureHadSnapshot != hasSnapshot) {
        return FALSE;
    }
    if (!hasSnapshot) {
        return TRUE;
    }

    return PluginDirSnapshotsEqual(snapshot, &g_asyncScanFailureSnapshot);
}

void MarkAsyncScanFailureLocked(BOOL hasSnapshot,
                                       const PluginDirSnapshot* snapshot) {
    g_asyncScanHasFailureSnapshot = TRUE;
    g_asyncScanFailureHadSnapshot = hasSnapshot;
    if (hasSnapshot && snapshot) {
        g_asyncScanFailureSnapshot = *snapshot;
    } else {
        ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    }
    DWORD now = GetTickCount();
    DWORD cooldownUntil = now + ASYNC_PLUGIN_SCAN_FAILURE_COOLDOWN_MS;
    InterlockedExchange(&g_asyncScanFailureCooldownUntil,
                        (LONG)(cooldownUntil ? cooldownUntil : 1));
}

void ClearAsyncScanFailureLocked(void) {
    g_asyncScanHasFailureSnapshot = FALSE;
    g_asyncScanFailureHadSnapshot = FALSE;
    ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    InterlockedExchange(&g_asyncScanFailureCooldownUntil, 0);
}
