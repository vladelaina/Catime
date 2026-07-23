#ifndef TRAY_ANIMATION_MENU_INTERNAL_H
#define TRAY_ANIMATION_MENU_INTERNAL_H

#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "log.h"
#include "language.h"
#include "utils/directory_watcher.h"
#include "utils/string_safe.h"

#include <limits.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ANIM_ENTRIES 200
#define MAX_RECURSION_DEPTH 10
#define MAX_ANIM_NAME_LENGTH 260
#define MAX_ANIM_SCAN_ENTRIES 4096
#define ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS 2000
#define ANIMATION_MENU_SCAN_REFRESH_COOLDOWN_MS 10000
#define ANIMATION_MENU_SCAN_FAILED (-1)

typedef struct {
    char fileName[MAX_ANIM_NAME_LENGTH];
    char relativePath[MAX_PATH];
    BOOL isSpecial;
} AnimEntry;

typedef struct {
    AnimEntry* entries;
    int count;
    int capacity;
    int scannedEntries;
    BOOL truncated;
    BOOL full;
    BOOL failed;
} AnimScanContext;

typedef struct {
    UINT id;
    char relativePath[MAX_PATH];
} AnimMenuIdMapEntry;

extern AnimMenuIdMapEntry g_animMenuIdMap[MAX_ANIM_ENTRIES];
extern int g_animMenuIdMapCount;
extern AnimEntry g_animMenuCache[MAX_ANIM_ENTRIES];
extern int g_animMenuCacheCount;
extern BOOL g_animMenuCacheReady;
extern BOOL g_animMenuCacheFailed;
extern SRWLOCK g_animMenuCacheLock;
extern SRWLOCK g_animScanThreadLock;
extern HANDLE g_hAnimScanThread;
extern HANDLE g_hRetiredAnimScanThread;
extern DirectoryWatcher g_animFolderWatcher;
extern volatile LONG g_animScanShuttingDown;
extern volatile LONG g_animScanGeneration;
extern volatile LONG g_animMenuLastScanTick;

BOOL AnimationMenu_IsScanShuttingDown(void);
BOOL AnimationMenu_IsScanCanceled(LONG generation);
BOOL AnimationMenu_IsCacheRecentlyScanned(DWORD now);
void AnimationMenu_MarkScanStartFailure(DWORD now);
BOOL AnimationMenu_CleanupRetiredScanThreadLocked(DWORD waitMs);
BOOL AnimationMenu_CopyStringExact(
    const char* source, char* output, size_t outputSize);
void ResetAnimationMenuIdMap(void);
BOOL RememberAnimationMenuId(UINT id, const char* relativePath);
void ForgetLastAnimationMenuId(UINT id);
const char* FindAnimationMenuPath(UINT id);

BOOL GetAnimationsFolderPathW(wchar_t* outPath, size_t size);
DWORD WINAPI AnimationScanThread(LPVOID parameter);
void StartAnimationFolderWatcher(void);
void StopAnimationFolderWatcher(void);

#endif
