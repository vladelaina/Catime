/**
 * @file tray_animation_preview.c
 * @brief Preview request queue, decoder worker, and cancellation.
 */

#include "tray_animation_core_internal.h"

static BOOL QueueAnimationLoadRequest(
    const char* name, BOOL fromPath, BOOL commit, BOOL persistConfig) {
    if (!name || !*name) return FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL queued = FALSE;
    char requestedName[MAX_PATH] = {0};
    if (!CopyStringExactA(name, requestedName, sizeof(requestedName))) {
        LOG_WARNING("Ignoring animation preview request because the name is too long: %s", name);
        goto done;
    }

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    CleanupCompletedPreviewWorkerLocked();

    BOOL duplicateRequest = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    duplicateRequest =
        (g_isPreviewActive &&
         g_previewAnimationFromPath == fromPath &&
         !commit &&
         g_previewAnimationName[0] != '\0' &&
         _stricmp(g_previewAnimationName, requestedName) == 0) ||
        (g_previewWorkerThread &&
         g_pendingPreviewFromPath == fromPath &&
         g_pendingPreviewCommit == commit &&
         (!commit || g_pendingPreviewPersist == persistConfig) &&
         g_pendingPreviewName[0] != '\0' &&
         _stricmp(g_pendingPreviewName, requestedName) == 0);
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
    if (duplicateRequest) {
        queued = TRUE;
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    if (!EnsurePreviewWorkerStartedLocked()) {
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    SignalPreviewDecodeCancelLocked();

    InterlockedIncrement(&g_previewRequestSerial);

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    CopyStringExactA(requestedName, g_pendingPreviewName, sizeof(g_pendingPreviewName));
    g_pendingPreviewFromPath = fromPath;
    g_pendingPreviewCommit = commit;
    g_pendingPreviewPersist = persistConfig;
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    if (g_previewRequestEvent) {
        WakePreviewWorkerLocked();
        queued = TRUE;
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

done:
    EndTrayAnimationRuntimeUse();
    return queued;
}

BOOL QueueAnimationPreviewRequest(const char* name, BOOL fromPath) {
    return QueueAnimationLoadRequest(name, fromPath, FALSE, FALSE);
}

BOOL QueueAnimationCommitRequest(const char* name, BOOL persistConfig) {
    return QueueAnimationLoadRequest(name, FALSE, TRUE, persistConfig);
}

/**
 * @brief Queue animation preview from file path
 */
BOOL PreviewAnimationFromFile(HWND hwnd, const char* filePath) {
    (void)hwnd;
    return QueueAnimationPreviewRequest(filePath, TRUE);
}

/**
 * @brief Preview worker thread
 */
DWORD WINAPI PreviewWorkerThread(LPVOID param) {
    (void)param;

    HANDLE waitHandles[2] = { g_previewStopEvent, g_previewRequestEvent };
    for (;;) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult != WAIT_OBJECT_0 + 1) {
            WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: request wait failed (result=%lu, error=%lu)",
                     waitResult, GetLastError());
            break;
        }

        if (!WaitForPreviewRequestQuiet(g_previewStopEvent, g_previewRequestEvent)) {
            break;
        }

        char requestedName[MAX_PATH] = {0};
        BOOL requestedFromPath = FALSE;
        BOOL requestedCommit = FALSE;
        BOOL requestedPersist = FALSE;
        LONG requestSerial = 0;

        AcquireSRWLockExclusive(&g_previewWorkerLock);

        requestSerial = InterlockedCompareExchange(&g_previewRequestSerial, 0, 0);
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
            requestedCommit = g_pendingPreviewCommit;
            requestedPersist = g_pendingPreviewPersist;
            LeaveCriticalSection(&g_animCriticalSection);
        } else {
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
            requestedCommit = g_pendingPreviewCommit;
            requestedPersist = g_pendingPreviewPersist;
        }

        if (!requestedName[0]) {
            ReleaseSRWLockExclusive(&g_previewWorkerLock);
            continue;
        }

        if (g_previewCancelEvent) {
            ResetEvent(g_previewCancelEvent);
        }

        ReleaseSRWLockExclusive(&g_previewWorkerLock);

        LoadedAnimation tempAnim;
        LoadedAnimation oldPreview;
        LoadedAnimation oldMain;
        LoadedAnimation_Init(&tempAnim);
        LoadedAnimation_Init(&oldPreview);
        LoadedAnimation_Init(&oldMain);

        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        MemoryPool* localPool = AnimationNeedsDecodePool(requestedName)
            ? MemoryPool_Create(MEMORY_POOL_SIZE)
            : NULL;
        if (requestedFromPath) {
            LoadAnimationFromPathWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                            g_previewCancelEvent);
        } else {
            LoadAnimationByNameWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                          g_previewCancelEvent);
        }
        if (localPool) {
            MemoryPool_Destroy(localPool);
        }

        if (g_previewStopEvent && WaitForSingleObject(g_previewStopEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            break;
        }

        if (g_previewCancelEvent && WaitForSingleObject(g_previewCancelEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        if (!BeginTrayAnimationRuntimeUse()) {
            LoadedAnimation_Free(&oldMain);
            LoadedAnimation_Free(&oldPreview);
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        BOOL shouldApply = FALSE;
        BOOL canActivate = (tempAnim.count > 0 ||
                            tempAnim.sourceType == ANIM_SOURCE_PERCENT ||
                            tempAnim.sourceType == ANIM_SOURCE_CAPSLOCK ||
                            _stricmp(requestedName, "__none__") == 0);
        BOOL committedAnimation = FALSE;
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
        }

        if (InterlockedCompareExchange(&g_previewRequestSerial, 0, 0) == requestSerial &&
            g_pendingPreviewFromPath == requestedFromPath &&
            g_pendingPreviewCommit == requestedCommit &&
            g_pendingPreviewPersist == requestedPersist &&
            g_pendingPreviewName[0] != '\0' &&
            _stricmp(g_pendingPreviewName, requestedName) == 0) {
            if (requestedCommit && canActivate) {
                SwapLoadedAnimation(&oldMain, &g_mainAnimation);
                SwapLoadedAnimation(&g_mainAnimation, &tempAnim);
                LoadedAnimation_Init(&tempAnim);
                g_mainIndex = 0;
                CopyStringExactA(requestedName, g_animationName,
                                 sizeof(g_animationName));
                if (g_isPreviewActive) {
                    SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                    LoadedAnimation_Init(&g_previewAnimation);
                }
                g_previewIndex = 0;
                g_isPreviewActive = FALSE;
                g_previewAnimationFromPath = FALSE;
                g_previewAnimationName[0] = '\0';
                ResetFramePlaybackState();
                shouldApply = TRUE;
                committedAnimation = TRUE;
            } else if (!requestedCommit && canActivate) {
                SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                SwapLoadedAnimation(&g_previewAnimation, &tempAnim);
                LoadedAnimation_Init(&tempAnim);
                g_previewIndex = 0;
                ResetFramePlaybackState();
                g_isPreviewActive = TRUE;
                g_previewAnimationFromPath = requestedFromPath;
                CopyStringExactA(requestedName, g_previewAnimationName,
                                 sizeof(g_previewAnimationName));
                shouldApply = TRUE;
            } else {
                shouldApply = TRUE;
                if (requestedCommit && g_isPreviewActive) {
                    SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                    LoadedAnimation_Init(&g_previewAnimation);
                    g_previewIndex = 0;
                    g_isPreviewActive = FALSE;
                    g_previewAnimationFromPath = FALSE;
                    g_previewAnimationName[0] = '\0';
                    ResetFramePlaybackState();
                }
                WriteLog(LOG_LEVEL_WARNING,
                         "PreviewWorkerThread: %s failed for '%s'",
                         requestedCommit ? "commit" : "preview",
                         requestedName);
            }
        }

        if (shouldApply) {
            g_pendingPreviewFromPath = FALSE;
            g_pendingPreviewCommit = FALSE;
            g_pendingPreviewPersist = FALSE;
            g_pendingPreviewName[0] = '\0';
        }

        if (IsAnimCriticalSectionReady()) {
            LeaveCriticalSection(&g_animCriticalSection);
        }

        LoadedAnimation_Free(&oldMain);
        LoadedAnimation_Free(&oldPreview);
        LoadedAnimation_Free(&tempAnim);

        if (committedAnimation && requestedPersist &&
            !WriteAnimationNameToConfigIfChanged(requestedName)) {
            WriteLog(LOG_LEVEL_WARNING,
                     "PreviewWorkerThread: animation applied but configuration could not be saved for '%s'",
                     requestedName);
        }
        if (shouldApply) {
            PostPreviewLoadedMessage();
        }

        EndTrayAnimationRuntimeUse();
    }

    CleanupRetiredPreviewWorkerOnExit();
    return 0;
}

/**
 * @brief Start animation preview
 */
BOOL StartAnimationPreview(const char* name) {
    return QueueAnimationPreviewRequest(name, FALSE);
}
