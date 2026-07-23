#include "tray_animation_menu_internal.h"

BOOL GetAnimationsFolderPathW(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0 || size > INT_MAX) return FALSE;
    char utf8Path[MAX_PATH];
    GetAnimationsFolderPath(utf8Path, MAX_PATH);
    if (utf8Path[0] == '\0' || MultiByteToWideChar(
            CP_UTF8, 0, utf8Path, -1, outPath, (int)size) <= 0) {
        if (outPath) outPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL IsAnimationFile(const wchar_t* fileName) {
    const wchar_t* extension = fileName ? wcsrchr(fileName, L'.') : NULL;
    if (!extension) return FALSE;
    return _wcsicmp(extension, L".gif") == 0 ||
           _wcsicmp(extension, L".webp") == 0 ||
           _wcsicmp(extension, L".ani") == 0 ||
           _wcsicmp(extension, L".ico") == 0 ||
           _wcsicmp(extension, L".png") == 0 ||
           _wcsicmp(extension, L".jpg") == 0 ||
           _wcsicmp(extension, L".jpeg") == 0 ||
           _wcsicmp(extension, L".bmp") == 0 ||
           _wcsicmp(extension, L".tif") == 0 ||
           _wcsicmp(extension, L".tiff") == 0;
}

static BOOL AddAnimEntry(
    AnimScanContext* context, const char* fileName,
    const char* relativePath, BOOL isSpecial) {
    if (context->count >= context->capacity) {
        if (!context->full) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Animation list capacity reached (%d), stopping scan",
                     context->capacity);
        }
        context->full = TRUE;
        return FALSE;
    }
    AnimEntry* entry = &context->entries[context->count++];
    safe_strncpy(entry->fileName, fileName, sizeof(entry->fileName));
    safe_strncpy(entry->relativePath, relativePath,
                 sizeof(entry->relativePath));
    entry->isSpecial = isSpecial;
    return TRUE;
}

static void ScanAnimationFolderRecursive(
    const wchar_t* folderPath, const char* relativePath,
    AnimScanContext* context, int depth, LONG generation) {
    if (!context || context->full ||
        AnimationMenu_IsScanCanceled(generation)) return;
    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Max recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(
        searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Animation scan path is too long: %ls", folderPath);
        context->failed = TRUE;
        return;
    }
    WIN32_FIND_DATAW findData;
    HANDLE findHandle = FindFirstFileW(searchPath, &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Animation folder scan failed: %ls (error=%lu)",
                     folderPath, error);
            context->failed = TRUE;
        }
        return;
    }

    BOOL stoppedEarly = FALSE;
    do {
        if (context->full || AnimationMenu_IsScanCanceled(generation)) {
            stoppedEarly = TRUE;
            break;
        }
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) continue;
        if (context->scannedEntries >= MAX_ANIM_SCAN_ENTRIES) {
            if (!context->truncated) {
                WriteLog(LOG_LEVEL_WARNING,
                         "Animation scan entry limit reached (%d)",
                         MAX_ANIM_SCAN_ENTRIES);
            }
            context->truncated = TRUE;
            context->full = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        context->scannedEntries++;
        BOOL isDirectory =
            (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!isDirectory && !IsAnimationFile(findData.cFileName)) continue;

        char fileNameUtf8[MAX_ANIM_NAME_LENGTH];
        if (WideCharToMultiByte(
                CP_UTF8, 0, findData.cFileName, -1,
                fileNameUtf8, MAX_ANIM_NAME_LENGTH, NULL, NULL) <= 0) continue;
        char newRelativePath[MAX_PATH];
        if (relativePath[0] == '\0') {
            safe_strncpy(newRelativePath, fileNameUtf8,
                         sizeof(newRelativePath));
        } else if (snprintf(
                       newRelativePath, MAX_PATH, "%s\\%s",
                       relativePath, fileNameUtf8) < 0) continue;
        if (isDirectory) {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            wchar_t fullPath[MAX_PATH];
            int fullLength = _snwprintf_s(
                fullPath, MAX_PATH, _TRUNCATE,
                L"%s\\%s", folderPath, findData.cFileName);
            if (fullLength < 0 || fullLength >= MAX_PATH) continue;
            ScanAnimationFolderRecursive(
                fullPath, newRelativePath, context, depth + 1, generation);
            if (context->failed) {
                stoppedEarly = TRUE;
                break;
            }
        } else if (!AddAnimEntry(
                       context, fileNameUtf8, newRelativePath, FALSE)) {
            stoppedEarly = TRUE;
            break;
        }
    } while (FindNextFileW(findHandle, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(findHandle);
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Animation folder enumeration failed: %ls (error=%lu)",
                 folderPath, findError);
        context->failed = TRUE;
    }
}

static int ScanAnimationsFolder(
    AnimEntry* entries, int capacity, LONG generation) {
    AnimScanContext context = {0};
    context.entries = entries;
    context.capacity = capacity;
    wchar_t animationPath[MAX_PATH];
    if (!GetAnimationsFolderPathW(animationPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get animations folder path");
        return ANIMATION_MENU_SCAN_FAILED;
    }
    DWORD attributes = GetFileAttributesW(animationPath);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return 0;
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to stat animations folder: %ls (error=%lu)",
                 animationPath, error);
        return ANIMATION_MENU_SCAN_FAILED;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Animations path is not a folder: %ls", animationPath);
        return ANIMATION_MENU_SCAN_FAILED;
    }
    ScanAnimationFolderRecursive(
        animationPath, "", &context, 0, generation);
    if (context.failed || AnimationMenu_IsScanCanceled(generation)) {
        return ANIMATION_MENU_SCAN_FAILED;
    }
    WriteLog(LOG_LEVEL_INFO,
             "Animation scan complete: %d animations found%s",
             context.count, context.truncated ? " (truncated)" : "");
    return context.count;
}

DWORD WINAPI AnimationScanThread(LPVOID parameter) {
    LONG generation = (LONG)(INT_PTR)parameter;
    AnimEntry* entries = malloc(
        (size_t)MAX_ANIM_ENTRIES * sizeof(*entries));
    int count = ANIMATION_MENU_SCAN_FAILED;
    if (entries) {
        ZeroMemory(entries, (size_t)MAX_ANIM_ENTRIES * sizeof(*entries));
        count = ScanAnimationsFolder(
            entries, MAX_ANIM_ENTRIES, generation);
    } else {
        LOG_WARNING("Failed to allocate animation menu scan buffer");
    }
    BOOL scanFailed = count < 0;
    if (!AnimationMenu_IsScanCanceled(generation)) {
        AcquireSRWLockExclusive(&g_animMenuCacheLock);
        if (!AnimationMenu_IsScanCanceled(generation)) {
            if (!scanFailed && count > 0 && entries) {
                memcpy(g_animMenuCache, entries,
                       (size_t)count * sizeof(AnimEntry));
            }
            if (scanFailed) count = 0;
            if (count < MAX_ANIM_ENTRIES) {
                ZeroMemory(&g_animMenuCache[count],
                           (size_t)(MAX_ANIM_ENTRIES - count) *
                           sizeof(AnimEntry));
            }
            g_animMenuCacheCount = count;
            g_animMenuCacheReady = !scanFailed;
            g_animMenuCacheFailed = scanFailed;
            InterlockedExchange(
                &g_animMenuLastScanTick, (LONG)GetTickCount());
        }
        ReleaseSRWLockExclusive(&g_animMenuCacheLock);
    }
    free(entries);
    return 0;
}
