        ReleaseSRWLockExclusive(&g_fontMenuCacheLock);
    }

    free(entries);
    return 0;
}

void FontMenu_RequestScanAsync(void) {
    AcquireSRWLockExclusive(&g_fontScanThreadLock);

    if (!CleanupRetiredFontScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (!g_hRetiredFontScanThread &&
        InterlockedCompareExchange(&g_fontScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_fontScanShuttingDown, 0);
    }

    if (IsFontMenuScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (g_hFontScanThread) {
        DWORD wait = WaitForSingleObject(g_hFontScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hFontScanThread);
            g_hFontScanThread = NULL;
        } else {
            ReleaseSRWLockExclusive(&g_fontScanThreadLock);
            return;
        }
    }

    DWORD now = GetTickCount();
    if (IsFontMenuCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    LONG generation = InterlockedCompareExchange(&g_fontScanGeneration, 0, 0);
    HANDLE hThread = CreateThread(NULL, 0, FontScanThread,
                                  (LPVOID)(INT_PTR)generation, 0, NULL);
    if (hThread) {
        g_hFontScanThread = hThread;
    } else {
        LOG_WARNING("Failed to start font menu scan thread");
        MarkFontMenuScanStartFailure(now);
    }

    ReleaseSRWLockExclusive(&g_fontScanThreadLock);
}

static void InvalidateFontMenuScanCooldown(void) {
    InterlockedExchange(&g_fontMenuLastScanTick, 0);
}

static void OnFontFolderChanged(void* context) {
    (void)context;
    InvalidateFontMenuScanCooldown();
    FontMenu_RequestScanAsync();
}

static void StartFontFolderWatcher(void) {
    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderW(fontsPath, MAX_PATH, TRUE)) {
        LOG_WARNING("Font folder watcher could not resolve fonts path");
        return;
    }

    DirectoryWatcher_Start(&g_fontFolderWatcher,
                           fontsPath,
                           TRUE,
                           DIRECTORY_WATCHER_DEFAULT_FILTER,
                           DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS,
                           OnFontFolderChanged,
                           NULL,
                           "FontFolderWatcher");
}

static void StopFontFolderWatcher(void) {
    DirectoryWatcher_Stop(&g_fontFolderWatcher, ASYNC_FONT_SCAN_STOP_TIMEOUT_MS);
}

void FontMenu_Initialize(void) {
    AcquireSRWLockExclusive(&g_fontScanThreadLock);
    if (!CleanupRetiredFontScanThreadLocked(ASYNC_FONT_SCAN_STOP_TIMEOUT_MS)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (g_hFontScanThread) {
        DWORD wait = WaitForSingleObject(g_hFontScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hFontScanThread);
            g_hFontScanThread = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_fontScanThreadLock);

    InterlockedIncrement(&g_fontScanGeneration);
    InterlockedExchange(&g_fontScanShuttingDown, 0);
    StartFontFolderWatcher();
}

void FontMenu_Shutdown(void) {
    HANDLE hThread = NULL;

    StopFontFolderWatcher();

    AcquireSRWLockExclusive(&g_fontScanThreadLock);
    InterlockedIncrement(&g_fontScanGeneration);
    InterlockedExchange(&g_fontScanShuttingDown, 1);
    hThread = g_hFontScanThread;
    ReleaseSRWLockExclusive(&g_fontScanThreadLock);

    if (hThread) {
        DWORD wait = WaitForSingleObject(hThread, ASYNC_FONT_SCAN_STOP_TIMEOUT_MS);
        if (wait != WAIT_OBJECT_0) {
            LOG_WARNING("Font menu scan stop timed out after %lu ms (wait=%lu, error=%lu)",
                        (DWORD)ASYNC_FONT_SCAN_STOP_TIMEOUT_MS,
                        wait,
                        GetLastError());
            if (wait == WAIT_TIMEOUT) {
                AcquireSRWLockExclusive(&g_fontScanThreadLock);
                if (g_hFontScanThread == hThread) {
                    g_hFontScanThread = NULL;
                    if (CleanupRetiredFontScanThreadLocked(0)) {
                        g_hRetiredFontScanThread = hThread;
                    } else {
                        CloseHandle(hThread);
                    }
                }
                ReleaseSRWLockExclusive(&g_fontScanThreadLock);
            }
        } else {
            AcquireSRWLockExclusive(&g_fontScanThreadLock);
            if (g_hFontScanThread == hThread) {
                CloseHandle(g_hFontScanThread);
                g_hFontScanThread = NULL;
            } else {
                CloseHandle(hThread);
            }
            ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        }
    }

    AcquireSRWLockExclusive(&g_fontMenuCacheLock);
    ZeroMemory(g_fontMenuCache, sizeof(g_fontMenuCache));
    g_fontMenuCacheCount = 0;
    g_fontMenuCacheReady = FALSE;
    g_fontMenuCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_fontMenuCacheLock);
    InterlockedExchange(&g_fontMenuLastScanTick, 0);

    ResetFontMenuIdMap();
}

/* ============================================================================
 * Menu Building
 * ============================================================================ */

/**
 * @brief Comparator for font entries (by relative path)
 */
typedef struct {
    const FontEntry* entry;
} FontEntrySortItem;

static int CompareFontEntries(const void* a, const void* b) {
    const FontEntrySortItem* ea = a;
    const FontEntrySortItem* eb = b;
    return NaturalPathCompareW(ea->entry->relativePath, eb->entry->relativePath);
}

static BOOL IsFontEntryCurrent(const FontEntry* entry, const wchar_t* currentFontRelPath) {
    return entry &&
           currentFontRelPath &&
           currentFontRelPath[0] != L'\0' &&
           _wcsicmp(entry->relativePath, currentFontRelPath) == 0;
}

/**
 * @brief Find or create a submenu with the given name
 */
static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name, BOOL shouldCheck) {
    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_STATE;
    wchar_t buffer[MAX_PATH] = {0};

    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && _wcsicmp(buffer, name) == 0) {
                if (shouldCheck && !(mii.fState & MFS_CHECKED)) {
                    mii.fState |= MFS_CHECKED;
                    SetMenuItemInfoW(hParent, i, TRUE, &mii);
                }
                return mii.hSubMenu;
            }
        }
    }

    HMENU hSub = CreatePopupMenu();
    if (!hSub) return NULL;

    UINT flags = MF_POPUP;
    if (shouldCheck) flags |= MF_CHECKED;
    if (!AppendMenuW(hParent, flags, (UINT_PTR)hSub, name)) {
        DestroyMenu(hSub);
        return NULL;
    }
    return hSub;
}

/**
 * @brief Build font menu from scanned entries
 */
static void BuildFontMenuFromEntries(HMENU hRootMenu, const FontEntry* entries, int count,
                                     const wchar_t* currentFontRelPath, int* fontId) {
    if (!entries || count <= 0 || count > MAX_FONT_ENTRIES) return;

    /* Sort entries for consistent display */
    FontEntrySortItem sortedEntries[MAX_FONT_ENTRIES];

    for (int i = 0; i < count; i++) {
        sortedEntries[i].entry = &entries[i];
    }

    qsort(sortedEntries, count, sizeof(sortedEntries[0]), CompareFontEntries);

    /* Collect parent directories of selected fonts */
    enum { MAX_PARENT_DIRS = 100 };
    wchar_t (*parentDirs)[MAX_PATH] = NULL;
    int parentDirCount = 0;

    for (int i = 0; i < count; i++) {
        if (IsFontEntryCurrent(sortedEntries[i].entry, currentFontRelPath)) {
            if (!parentDirs) {
                parentDirs = (wchar_t (*)[MAX_PATH])calloc(MAX_PARENT_DIRS,
                                                           sizeof(*parentDirs));
                if (!parentDirs) {
                    LOG_WARNING("Failed to allocate font menu parent directory buffer");
                    break;
                }
            }

            wchar_t pathCopy[MAX_PATH];
            wcsncpy(pathCopy, sortedEntries[i].entry->relativePath, MAX_PATH - 1);
            pathCopy[MAX_PATH - 1] = L'\0';

            wchar_t currentPath[MAX_PATH] = L"";
            wchar_t* context = NULL;
            wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);
