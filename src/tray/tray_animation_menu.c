/**
 * @file tray_animation_menu.c
 * @brief Public animation menu construction and command handling
 */

#include "tray_animation_menu_internal.h"
#include "taskbar_monitor.h"
#include "tray/tray.h"

BOOL SetCurrentAnimationName(const char* name);

typedef struct {
    const AnimEntry* entry;
} AnimEntrySortItem;

static int CompareAnimEntries(const void* first, const void* second) {
    const AnimEntrySortItem* left = first;
    const AnimEntrySortItem* right = second;
    return NaturalPathCompareA(
        left->entry->relativePath, right->entry->relativePath);
}

static void AppendTaskbarMonitorOptions(HMENU menu) {
    AppendMenuW(
        menu,
        MF_STRING | (TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_NETWORK)
                ? MF_CHECKED : MF_UNCHECKED),
        CLOCK_IDM_TASKBAR_MONITOR_NETWORK,
        GetLocalizedString(NULL, L"Taskbar Network Speed"));
    AppendMenuW(
        menu,
        MF_STRING | (TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_CPU_MEMORY)
                ? MF_CHECKED : MF_UNCHECKED),
        CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY,
        GetLocalizedString(NULL, L"Taskbar CPU and Memory"));
}

static HMENU EnsureSubMenu(HMENU parent, const wchar_t* name) {
    if (!parent || !name) return NULL;
    int count = GetMenuItemCount(parent);
    MENUITEMINFOW item = {0};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_STRING | MIIM_SUBMENU;
    wchar_t buffer[MAX_PATH] = {0};
    for (int i = 0; i < count; i++) {
        item.dwTypeData = buffer;
        item.cch = MAX_PATH;
        if (GetMenuItemInfoW(parent, i, TRUE, &item) && item.hSubMenu &&
            _wcsicmp(buffer, name) == 0) {
            return item.hSubMenu;
        }
    }
    HMENU submenu = CreatePopupMenu();
    if (!submenu) return NULL;
    if (!AppendMenuW(parent, MF_POPUP, (UINT_PTR)submenu, name)) {
        DestroyMenu(submenu);
        return NULL;
    }
    return submenu;
}

static void BuildAnimationMenuFromEntries(
    HMENU rootMenu, const AnimEntry* entries, int count,
    const char* currentAnimation, UINT* nextId) {
    if (!rootMenu || !entries || count <= 0 ||
        count > MAX_ANIM_ENTRIES || !nextId) return;

    AnimEntrySortItem sortedEntries[MAX_ANIM_ENTRIES];
    for (int i = 0; i < count; i++) sortedEntries[i].entry = &entries[i];
    qsort(sortedEntries, count, sizeof(sortedEntries[0]), CompareAnimEntries);

    for (int i = 0; i < count; i++) {
        const AnimEntry* entry = sortedEntries[i].entry;
        if (entry->isSpecial) continue;
        char pathCopy[MAX_PATH];
        strncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = '\0';
        char* context = NULL;
        char* token = strtok_s(pathCopy, "\\/", &context);
        HMENU currentMenu = rootMenu;
        while (token) {
            char* nextToken = strtok_s(NULL, "\\/", &context);
            wchar_t wideName[MAX_PATH];
            if (MultiByteToWideChar(
                    CP_UTF8, 0, token, -1, wideName, MAX_PATH) <= 0) break;
            if (nextToken) {
                currentMenu = EnsureSubMenu(currentMenu, wideName);
                if (!currentMenu) break;
            } else {
                UINT flags = MF_STRING;
                if (currentAnimation && strcmp(
                        currentAnimation, entry->relativePath) == 0) {
                    flags |= MF_CHECKED;
                }
                if (*nextId < CLOCK_IDM_ANIMATIONS_END && currentMenu &&
                    RememberAnimationMenuId(*nextId, entry->relativePath)) {
                    if (AppendMenuW(
                            currentMenu, flags, *nextId, wideName)) {
                        (*nextId)++;
                    } else {
                        ForgetLastAnimationMenuId(*nextId);
                    }
                }
            }
            token = nextToken;
        }
    }
}

BOOL BuildAnimationMenu(HMENU menu, const char* currentAnimationName) {
    if (!menu) return FALSE;
    ResetAnimationMenuIdMap();
    AnimationMenu_RequestScanAsync();

    int builtinCount = 0;
    const BuiltinAnimDef* builtins = GetBuiltinAnims(&builtinCount);
    for (int i = 0; i < builtinCount; i++) {
        UINT flags = MF_STRING;
        if (currentAnimationName && _stricmp(
                currentAnimationName, builtins[i].name) == 0) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(
            menu, flags, builtins[i].menuId,
            GetLocalizedString(NULL, builtins[i].menuLabel));
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendTaskbarMonitorOptions(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    BOOL cacheReady = FALSE;
    int animationCount = 0;
    AnimEntry* snapshot = malloc(
        (size_t)MAX_ANIM_ENTRIES * sizeof(*snapshot));
    if (!snapshot) LOG_WARNING("Failed to allocate animation menu cache snapshot");
    AcquireSRWLockShared(&g_animMenuCacheLock);
    cacheReady = g_animMenuCacheReady || g_animMenuCacheFailed;
    animationCount = g_animMenuCacheCount;
    if (animationCount > MAX_ANIM_ENTRIES) animationCount = MAX_ANIM_ENTRIES;
    if (animationCount > 0 && snapshot) {
        memcpy(snapshot, g_animMenuCache,
               (size_t)animationCount * sizeof(*snapshot));
    } else if (animationCount > 0) {
        animationCount = 0;
        cacheReady = FALSE;
    }
    ReleaseSRWLockShared(&g_animMenuCacheLock);

    if (animationCount > 0) {
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        BuildAnimationMenuFromEntries(
            menu, snapshot, animationCount,
            currentAnimationName, &nextId);
    }
    free(snapshot);
    if (animationCount <= 0 && !cacheReady) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0,
                    GetLocalizedString(NULL, L"Loading..."));
    } else if (animationCount <= 0) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0,
                    GetLocalizedString(
                        NULL, L"(Supports GIF, WebP, ANI, PNG, etc.)"));
    }
    return animationCount > 0;
}

BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    (void)hwnd;
    if (id == CLOCK_IDM_ANIM_SPEED_ORIGINAL ||
        id == CLOCK_IDM_ANIM_SPEED_MEMORY ||
        id == CLOCK_IDM_ANIM_SPEED_CPU ||
        id == CLOCK_IDM_ANIM_SPEED_TIMER ||
        id == CLOCK_IDM_ANIM_SPEED_FIXED) return FALSE;
    if (id == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY ||
        id == CLOCK_IDM_TASKBAR_MONITOR_NETWORK) {
        TaskbarMonitorOption option =
            id == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY
                ? TASKBAR_MONITOR_OPTION_CPU_MEMORY
                : TASKBAR_MONITOR_OPTION_NETWORK;
        BOOL enabled = !TaskbarMonitor_IsOptionEnabled(option);
        if (TaskbarMonitor_SetOptionEnabled(option, enabled)) {
            RefreshTrayBackgroundWorkState();
        }
        return TRUE;
    }
    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }
    const BuiltinAnimDef* definition = GetBuiltinAnimDefById(id);
    if (definition) return SetCurrentAnimationName(definition->name);
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
        const char* path = FindAnimationMenuPath(id);
        return path ? SetCurrentAnimationName(path) : FALSE;
    }
    return FALSE;
}

void OpenAnimationsFolder(void) {
    char basePath[MAX_PATH] = {0};
    GetAnimationsFolderPath(basePath, sizeof(basePath));
    if (basePath[0] == '\0') return;
    wchar_t widePath[MAX_PATH] = {0};
    if (MultiByteToWideChar(
            CP_UTF8, 0, basePath, -1, widePath, MAX_PATH) <= 0) return;
    ShellExecuteW(NULL, L"open", widePath, NULL, NULL, SW_SHOWNORMAL);
}

BOOL GetAnimationNameFromMenuId(
    UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';
    const BuiltinAnimDef* definition = GetBuiltinAnimDefById(id);
    if (definition) return AnimationMenu_CopyStringExact(
        definition->name, outPath, outPathSize);
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
        const char* path = FindAnimationMenuPath(id);
        return path && AnimationMenu_CopyStringExact(
            path, outPath, outPathSize);
    }
    return FALSE;
}
