#include "tray_animation_menu_internal.h"

AnimMenuIdMapEntry g_animMenuIdMap[MAX_ANIM_ENTRIES];
int g_animMenuIdMapCount = 0;
AnimEntry g_animMenuCache[MAX_ANIM_ENTRIES];
int g_animMenuCacheCount = 0;
BOOL g_animMenuCacheReady = FALSE;
BOOL g_animMenuCacheFailed = FALSE;
SRWLOCK g_animMenuCacheLock = SRWLOCK_INIT;
SRWLOCK g_animScanThreadLock = SRWLOCK_INIT;
HANDLE g_hAnimScanThread = NULL;
HANDLE g_hRetiredAnimScanThread = NULL;
DirectoryWatcher g_animFolderWatcher = {0};
volatile LONG g_animScanShuttingDown = 0;
volatile LONG g_animScanGeneration = 0;
volatile LONG g_animMenuLastScanTick = 0;

BOOL AnimationMenu_IsScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_animScanShuttingDown, 0, 0) != 0;
}

BOOL AnimationMenu_IsScanCanceled(LONG generation) {
    return AnimationMenu_IsScanShuttingDown() ||
           InterlockedCompareExchange(&g_animScanGeneration, 0, 0) != generation;
}

BOOL AnimationMenu_IsCacheRecentlyScanned(DWORD now) {
    DWORD lastScan = (DWORD)InterlockedCompareExchange(
        &g_animMenuLastScanTick, 0, 0);
    if (lastScan == 0 || (DWORD)(now - lastScan) >=
            ANIMATION_MENU_SCAN_REFRESH_COOLDOWN_MS) return FALSE;
    AcquireSRWLockShared(&g_animMenuCacheLock);
    BOOL ready = g_animMenuCacheReady || g_animMenuCacheFailed;
    ReleaseSRWLockShared(&g_animMenuCacheLock);
    return ready;
}

void AnimationMenu_MarkScanStartFailure(DWORD now) {
    AcquireSRWLockExclusive(&g_animMenuCacheLock);
    ZeroMemory(g_animMenuCache, sizeof(g_animMenuCache));
    g_animMenuCacheCount = 0;
    g_animMenuCacheReady = FALSE;
    g_animMenuCacheFailed = TRUE;
    InterlockedExchange(&g_animMenuLastScanTick, (LONG)now);
    ReleaseSRWLockExclusive(&g_animMenuCacheLock);
}

BOOL AnimationMenu_CleanupRetiredScanThreadLocked(DWORD waitMs) {
    if (!g_hRetiredAnimScanThread) return TRUE;
    DWORD wait = WaitForSingleObject(g_hRetiredAnimScanThread, waitMs);
    if (wait != WAIT_OBJECT_0) {
        if (wait == WAIT_FAILED) {
            LOG_WARNING("Retired animation menu scan wait failed: %lu",
                        GetLastError());
        }
        return FALSE;
    }
    CloseHandle(g_hRetiredAnimScanThread);
    g_hRetiredAnimScanThread = NULL;
    return TRUE;
}

BOOL AnimationMenu_CopyStringExact(
    const char* source, char* output, size_t outputSize) {
    if (!output || outputSize == 0) return FALSE;
    output[0] = '\0';
    if (!source) return FALSE;
    size_t length = strlen(source);
    if (length >= outputSize) return FALSE;
    memcpy(output, source, length + 1);
    return TRUE;
}

void ResetAnimationMenuIdMap(void) {
    ZeroMemory(g_animMenuIdMap, sizeof(g_animMenuIdMap));
    g_animMenuIdMapCount = 0;
}

BOOL RememberAnimationMenuId(UINT id, const char* relativePath) {
    if (!relativePath || g_animMenuIdMapCount >= MAX_ANIM_ENTRIES) return FALSE;
    AnimMenuIdMapEntry entry = {0};
    entry.id = id;
    if (!AnimationMenu_CopyStringExact(
            relativePath, entry.relativePath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Animation menu path is too long: %s", relativePath);
        return FALSE;
    }
    g_animMenuIdMap[g_animMenuIdMapCount++] = entry;
    return TRUE;
}

void ForgetLastAnimationMenuId(UINT id) {
    if (g_animMenuIdMapCount <= 0 ||
        g_animMenuIdMap[g_animMenuIdMapCount - 1].id != id) return;
    g_animMenuIdMapCount--;
    ZeroMemory(&g_animMenuIdMap[g_animMenuIdMapCount],
               sizeof(g_animMenuIdMap[0]));
}

const char* FindAnimationMenuPath(UINT id) {
    for (int i = 0; i < g_animMenuIdMapCount; i++) {
        if (g_animMenuIdMap[i].id == id) {
            return g_animMenuIdMap[i].relativePath;
        }
    }
    return NULL;
}
