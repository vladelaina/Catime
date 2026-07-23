#include "dialog_notification_audio_internal.h"

wchar_t g_soundFileCache[NOTIFICATION_SOUND_ENTRY_LIMIT][MAX_PATH];
int g_soundFileCacheCount = 0;
BOOL g_soundFileCacheReady = FALSE;
BOOL g_soundFileCacheFailed = FALSE;
SRWLOCK g_soundFileCacheLock = SRWLOCK_INIT;
SRWLOCK g_soundScanThreadLock = SRWLOCK_INIT;
SRWLOCK g_soundCacheNotifyLock = SRWLOCK_INIT;
HANDLE g_hSoundScanThread = NULL;
HANDLE g_hRetiredSoundScanThread = NULL;
DirectoryWatcher g_soundFolderWatcher = {0};
HWND g_soundCacheNotifyHwnd = NULL;
volatile LONG g_soundScanShuttingDown = 0;
volatile LONG g_soundScanGeneration = 0;
volatile LONG g_soundFileLastScanTick = 0;

BOOL NotificationAudio_IsScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_soundScanShuttingDown, 0, 0) != 0;
}

BOOL NotificationAudio_IsScanCanceled(LONG generation) {
    return NotificationAudio_IsScanShuttingDown() ||
           InterlockedCompareExchange(&g_soundScanGeneration, 0, 0) != generation;
}

BOOL NotificationAudio_IsCurrentProcessWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

void NotificationAudio_NotifyCacheUpdated(void) {
    HWND hwnd = NULL;
    AcquireSRWLockShared(&g_soundCacheNotifyLock);
    hwnd = g_soundCacheNotifyHwnd;
    ReleaseSRWLockShared(&g_soundCacheNotifyLock);

    if (NotificationAudio_IsCurrentProcessWindow(hwnd)) {
        PostMessageW(hwnd, WM_NOTIFICATION_SOUND_CACHE_UPDATED, 0, 0);
    }
}

BOOL NotificationAudio_IsCacheRecentlyScanned(DWORD now) {
    DWORD lastScanTick = (DWORD)InterlockedCompareExchange(
        &g_soundFileLastScanTick, 0, 0);
    if (lastScanTick == 0 ||
        (DWORD)(now - lastScanTick) >=
            NOTIFICATION_SOUND_SCAN_REFRESH_COOLDOWN_MS) {
        return FALSE;
    }

    AcquireSRWLockShared(&g_soundFileCacheLock);
    BOOL recentlyScanned = g_soundFileCacheReady || g_soundFileCacheFailed;
    ReleaseSRWLockShared(&g_soundFileCacheLock);
    return recentlyScanned;
}

BOOL NotificationAudio_IsSupportedFileName(const wchar_t* fileName) {
    const wchar_t* ext = fileName ? wcsrchr(fileName, L'.') : NULL;
    return ext && (_wcsicmp(ext, L".mp3") == 0 ||
                   _wcsicmp(ext, L".wav") == 0);
}

BOOL NotificationAudio_GetCurrentFileName(
    const char* currentFile, wchar_t* outFileName, size_t outSize) {
    if (!currentFile || currentFile[0] == '\0' || !outFileName ||
        outSize == 0) {
        return FALSE;
    }

    wchar_t wideSoundFile[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, currentFile, -1,
                            wideSoundFile, MAX_PATH) <= 0) {
        return FALSE;
    }

    wchar_t* fileName = wcsrchr(wideSoundFile, L'\\');
    fileName = fileName ? fileName + 1 : wideSoundFile;
    if (fileName[0] == L'\0') {
        return FALSE;
    }

    wcsncpy_s(outFileName, outSize, fileName, _TRUNCATE);
    return TRUE;
}

BOOL NotificationAudio_StoreCache(
    const wchar_t* files, int fileCount, LONG generation) {
    if (NotificationAudio_IsScanCanceled(generation) ||
        !files || fileCount < 0) {
        return FALSE;
    }
    if (fileCount > NOTIFICATION_SOUND_ENTRY_LIMIT) {
        fileCount = NOTIFICATION_SOUND_ENTRY_LIMIT;
    }

    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    if (NotificationAudio_IsScanCanceled(generation)) {
        ReleaseSRWLockExclusive(&g_soundFileCacheLock);
        return FALSE;
    }

    ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
    if (fileCount > 0) {
        memcpy(g_soundFileCache, files,
               (size_t)fileCount * MAX_PATH * sizeof(wchar_t));
    }
    g_soundFileCacheCount = fileCount;
    g_soundFileCacheReady = TRUE;
    g_soundFileCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);

    InterlockedExchange(&g_soundFileLastScanTick, (LONG)GetTickCount());
    NotificationAudio_NotifyCacheUpdated();
    return TRUE;
}

void NotificationAudio_MarkCacheScanFailed(void) {
    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    if (!NotificationAudio_IsScanShuttingDown()) {
        ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
        g_soundFileCacheCount = 0;
        g_soundFileCacheReady = FALSE;
        g_soundFileCacheFailed = TRUE;
        InterlockedExchange(&g_soundFileLastScanTick, (LONG)GetTickCount());
    }
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);
    NotificationAudio_NotifyCacheUpdated();
}

int NotificationAudio_CopyCache(
    wchar_t files[][MAX_PATH], int capacity, BOOL* cacheReady) {
    if (cacheReady) {
        *cacheReady = FALSE;
    }
    if (!files || capacity <= 0) {
        return 0;
    }

    AcquireSRWLockShared(&g_soundFileCacheLock);
    if (cacheReady) {
        *cacheReady = g_soundFileCacheReady || g_soundFileCacheFailed;
    }
    int count = g_soundFileCacheCount;
    if (count > capacity) {
        count = capacity;
    }
    if (count > 0) {
        memcpy(files, g_soundFileCache,
               (size_t)count * MAX_PATH * sizeof(wchar_t));
    }
    ReleaseSRWLockShared(&g_soundFileCacheLock);
    return count;
}
