#include "tray_animation_menu_internal.h"

void AnimationMenu_RequestScanAsync(void) {
    AcquireSRWLockExclusive(&g_animScanThreadLock);
    if (!AnimationMenu_CleanupRetiredScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }
    if (!g_hRetiredAnimScanThread &&
        InterlockedCompareExchange(&g_animScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_animScanShuttingDown, 0);
    }
    if (AnimationMenu_IsScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }
    if (g_hAnimScanThread) {
        DWORD wait = WaitForSingleObject(g_hAnimScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hAnimScanThread);
            g_hAnimScanThread = NULL;
        } else {
            ReleaseSRWLockExclusive(&g_animScanThreadLock);
            return;
        }
    }
    DWORD now = GetTickCount();
    if (AnimationMenu_IsCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }
    LONG generation = InterlockedCompareExchange(
        &g_animScanGeneration, 0, 0);
    HANDLE thread = CreateThread(
        NULL, 0, AnimationScanThread,
        (LPVOID)(INT_PTR)generation, 0, NULL);
    if (thread) {
        g_hAnimScanThread = thread;
    } else {
        LOG_WARNING("Failed to start animation menu scan thread");
        AnimationMenu_MarkScanStartFailure(now);
    }
    ReleaseSRWLockExclusive(&g_animScanThreadLock);
}

static void OnAnimationFolderChanged(void* context) {
    (void)context;
    InterlockedExchange(&g_animMenuLastScanTick, 0);
    AnimationMenu_RequestScanAsync();
}

void StartAnimationFolderWatcher(void) {
    wchar_t animationPath[MAX_PATH];
    if (!GetAnimationsFolderPathW(animationPath, MAX_PATH)) {
        LOG_WARNING(
            "Animation folder watcher could not resolve animations path");
        return;
    }
    DirectoryWatcher_Start(
        &g_animFolderWatcher, animationPath, TRUE,
        DIRECTORY_WATCHER_DEFAULT_FILTER,
        DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS,
        OnAnimationFolderChanged, NULL, "AnimationFolderWatcher");
}

void StopAnimationFolderWatcher(void) {
    DirectoryWatcher_Stop(
        &g_animFolderWatcher, ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS);
}

void AnimationMenu_Initialize(void) {
    AcquireSRWLockExclusive(&g_animScanThreadLock);
    if (!AnimationMenu_CleanupRetiredScanThreadLocked(
            ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS)) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }
    if (g_hAnimScanThread && WaitForSingleObject(
            g_hAnimScanThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(g_hAnimScanThread);
        g_hAnimScanThread = NULL;
    }
    ReleaseSRWLockExclusive(&g_animScanThreadLock);
    InterlockedIncrement(&g_animScanGeneration);
    InterlockedExchange(&g_animScanShuttingDown, 0);
    StartAnimationFolderWatcher();
}

static void WaitForAnimationScanThread(HANDLE thread) {
    if (!thread) return;
    DWORD wait = WaitForSingleObject(
        thread, ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS);
    if (wait != WAIT_OBJECT_0) {
        LOG_WARNING(
            "Animation menu scan stop timed out after %lu ms (wait=%lu, error=%lu)",
            (DWORD)ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS, wait, GetLastError());
        if (wait == WAIT_TIMEOUT) {
            AcquireSRWLockExclusive(&g_animScanThreadLock);
            if (g_hAnimScanThread == thread) {
                g_hAnimScanThread = NULL;
                if (AnimationMenu_CleanupRetiredScanThreadLocked(0)) {
                    g_hRetiredAnimScanThread = thread;
                } else {
                    CloseHandle(thread);
                }
            }
            ReleaseSRWLockExclusive(&g_animScanThreadLock);
        }
        return;
    }
    AcquireSRWLockExclusive(&g_animScanThreadLock);
    if (g_hAnimScanThread == thread) {
        CloseHandle(g_hAnimScanThread);
        g_hAnimScanThread = NULL;
    } else {
        CloseHandle(thread);
    }
    ReleaseSRWLockExclusive(&g_animScanThreadLock);
}

void AnimationMenu_Shutdown(void) {
    StopAnimationFolderWatcher();
    AcquireSRWLockExclusive(&g_animScanThreadLock);
    InterlockedIncrement(&g_animScanGeneration);
    InterlockedExchange(&g_animScanShuttingDown, 1);
    HANDLE thread = g_hAnimScanThread;
    ReleaseSRWLockExclusive(&g_animScanThreadLock);
    WaitForAnimationScanThread(thread);

    AcquireSRWLockExclusive(&g_animMenuCacheLock);
    ZeroMemory(g_animMenuCache, sizeof(g_animMenuCache));
    g_animMenuCacheCount = 0;
    g_animMenuCacheReady = FALSE;
    g_animMenuCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_animMenuCacheLock);
    InterlockedExchange(&g_animMenuLastScanTick, 0);
    ResetAnimationMenuIdMap();
}
