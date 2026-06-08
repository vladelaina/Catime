/**
 * @file plugin_exit.c
 * @brief Plugin exit countdown management
 * 
 * Handles <exit>N</exit> tag parsing and countdown display.
 * When countdown completes, sends message to stop plugin.
 */

#include "plugin/plugin_exit.h"
#include "../resource/resource.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/* ============================================================================
 * State
 * ============================================================================ */

static volatile LONG g_exitInProgress = FALSE;
static SRWLOCK g_exitLock = SRWLOCK_INIT;
static CONDITION_VARIABLE g_exitStateChanged = CONDITION_VARIABLE_INIT;
static BOOL g_exitThreadStopInProgress = FALSE;
static HANDLE g_exitThread = NULL;
static HANDLE g_exitStopEvent = NULL;

#define MAX_EXIT_COUNTDOWN_SECONDS 3600
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define EXIT_COUNTDOWN_SHUTDOWN_WAIT_MS 2000
#define EXIT_COUNTDOWN_START_FAILURE_COOLDOWN_MS 2000

/* Template for countdown display */
static wchar_t* g_exitPrefix = NULL;
static wchar_t* g_exitSuffix = NULL;
static DWORD g_exitLastStartFailureTick = 0;

/* Shared resources from plugin_data */
static HWND g_notifyWnd = NULL;
static CRITICAL_SECTION* g_dataCS = NULL;

/* External: display text buffer from plugin_data */
extern wchar_t* g_pluginDisplayText;
extern size_t g_pluginDisplayTextLen;
extern BOOL g_hasPluginData;

typedef struct {
    const wchar_t* prefix;
    const wchar_t* suffix;
    HWND notifyWnd;
    CRITICAL_SECTION* dataCS;
    HANDLE stopEvent;
} ExitCountdownSnapshot;

/* ============================================================================
 * Exit Countdown Thread
 * ============================================================================ */

static BOOL IsExitInProgress(void) {
    return InterlockedCompareExchange(&g_exitInProgress, FALSE, FALSE) != FALSE;
}

static BOOL IsExitStartFailureCoolingDown(DWORD now) {
    return g_exitLastStartFailureTick != 0 &&
           (DWORD)(now - g_exitLastStartFailureTick) <
               EXIT_COUNTDOWN_START_FAILURE_COOLDOWN_MS;
}

static void MarkExitStartFailure(DWORD now) {
    g_exitLastStartFailureTick = now ? now : 1;
}

static BOOL IsValidPluginNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static void CleanupCompletedExitThreadHandleLocked(void) {
    HANDLE threadHandle = g_exitThread;
    if (!threadHandle) return;
    if (g_exitThreadStopInProgress) return;

    if (WaitForSingleObject(threadHandle, 0) == WAIT_OBJECT_0) {
        g_exitThread = NULL;
        CloseHandle(threadHandle);
        WakeAllConditionVariable(&g_exitStateChanged);
    }
}

static void FreeExitTemplatesLocked(void) {
    if (g_exitPrefix) {
        free(g_exitPrefix);
        g_exitPrefix = NULL;
    }
    if (g_exitSuffix) {
        free(g_exitSuffix);
        g_exitSuffix = NULL;
    }
}

static BOOL EnsureExitStopEventLocked(void) {
    if (!g_exitStopEvent) {
        g_exitStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_exitStopEvent) {
            LOG_ERROR("PluginExit: Failed to create stop event");
            return FALSE;
        }
    }

    ResetEvent(g_exitStopEvent);
    return TRUE;
}

static void SnapshotExitCountdownState(ExitCountdownSnapshot* snapshot) {
    if (!snapshot) return;

    AcquireSRWLockShared(&g_exitLock);
    snapshot->prefix = g_exitPrefix;
    snapshot->suffix = g_exitSuffix;
    snapshot->notifyWnd = g_notifyWnd;
    snapshot->dataCS = g_dataCS;
    snapshot->stopEvent = g_exitStopEvent;
    ReleaseSRWLockShared(&g_exitLock);
}

static DWORD WINAPI ExitCountdownThread(LPVOID lpParam) {
    int seconds = (int)(intptr_t)lpParam;

    while (seconds > 0 && IsExitInProgress()) {
        /* Build display: prefix + number + suffix */
        wchar_t countdownNum[16];
        _snwprintf_s(countdownNum, 16, _TRUNCATE, L"%d", seconds);

        wchar_t stackDisplay[512];
        wchar_t* heapDisplay = NULL;
        wchar_t* displayText = stackDisplay;
        size_t displayLen = 0;
        HWND notifyWnd = NULL;
        CRITICAL_SECTION* dataCS = NULL;
        HANDLE stopEvent = NULL;
        BOOL displayReady = FALSE;

        AcquireSRWLockShared(&g_exitLock);
        notifyWnd = g_notifyWnd;
        dataCS = g_dataCS;
        stopEvent = g_exitStopEvent;
        {
            size_t prefixLen = g_exitPrefix ? wcslen(g_exitPrefix) : 0;
            size_t suffixLen = g_exitSuffix ? wcslen(g_exitSuffix) : 0;
            size_t numLen = wcslen(countdownNum);
            if (prefixLen > SIZE_MAX - numLen ||
                prefixLen + numLen > SIZE_MAX - suffixLen ||
                prefixLen + numLen + suffixLen > SIZE_MAX - 1) {
                ReleaseSRWLockShared(&g_exitLock);
                InterlockedExchange(&g_exitInProgress, FALSE);
                return 0;
            }
            size_t totalLen = prefixLen + numLen + suffixLen + 1;
            if (totalLen > _countof(stackDisplay)) {
                if (totalLen > SIZE_MAX / sizeof(wchar_t)) {
                    ReleaseSRWLockShared(&g_exitLock);
                    InterlockedExchange(&g_exitInProgress, FALSE);
                    return 0;
                }
                heapDisplay = (wchar_t*)malloc(totalLen * sizeof(wchar_t));
                if (!heapDisplay) {
                    LOG_WARNING("PluginExit: Failed to allocate countdown text buffer (%zu bytes)",
                                totalLen * sizeof(wchar_t));
                    ReleaseSRWLockShared(&g_exitLock);
                    InterlockedExchange(&g_exitInProgress, FALSE);
                    return 0;
                }
                displayText = heapDisplay;
            }

            displayText[0] = L'\0';
            if (g_exitPrefix) {
                wcsncat_s(displayText, totalLen, g_exitPrefix, prefixLen);
            }
            wcsncat_s(displayText, totalLen, countdownNum, numLen);
            if (g_exitSuffix) {
                wcsncat_s(displayText, totalLen, g_exitSuffix, suffixLen);
            }
            displayLen = totalLen;
            displayReady = TRUE;
        }
        ReleaseSRWLockShared(&g_exitLock);

        if (displayReady && dataCS) {
            EnterCriticalSection(dataCS);

            if (g_pluginDisplayText == NULL || g_pluginDisplayTextLen < displayLen) {
                wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, displayLen * sizeof(wchar_t));
                if (newBuf) {
                    g_pluginDisplayText = newBuf;
                    g_pluginDisplayTextLen = displayLen;
                } else {
                    LOG_WARNING("PluginExit: Failed to resize display buffer to %zu bytes",
                                displayLen * sizeof(wchar_t));
                }
            }

            if (g_pluginDisplayText && g_pluginDisplayTextLen >= displayLen) {
                memcpy(g_pluginDisplayText, displayText, displayLen * sizeof(wchar_t));
                g_hasPluginData = TRUE;
            }

            LeaveCriticalSection(dataCS);
        }
        free(heapDisplay);

        if (IsValidPluginNotifyWindow(notifyWnd)) {
            InvalidateRect(notifyWnd, NULL, FALSE);
        }

        if (stopEvent && WaitForSingleObject(stopEvent, 1000) == WAIT_OBJECT_0) {
            break;
        }
        seconds--;
    }

    if (IsExitInProgress()) {
        ExitCountdownSnapshot snapshot = {0};
        SnapshotExitCountdownState(&snapshot);
        if (IsValidPluginNotifyWindow(snapshot.notifyWnd)) {
            PostMessage(snapshot.notifyWnd, CLOCK_WM_PLUGIN_EXIT, 0, 0);
        }
    }

    InterlockedExchange(&g_exitInProgress, FALSE);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL PluginExit_Init(HWND hwnd, CRITICAL_SECTION* dataCS) {
    AcquireSRWLockExclusive(&g_exitLock);
    CleanupCompletedExitThreadHandleLocked();
    if (g_exitThread || g_exitThreadStopInProgress) {
        InterlockedExchange(&g_exitInProgress, FALSE);
        if (g_exitStopEvent) {
            SetEvent(g_exitStopEvent);
        }
        LOG_WARNING("PluginExit: Init deferred because a previous countdown thread is still retiring");
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    g_notifyWnd = hwnd;
    g_dataCS = dataCS;
    InterlockedExchange(&g_exitInProgress, FALSE);
    g_exitThreadStopInProgress = FALSE;
    g_exitLastStartFailureTick = 0;
    FreeExitTemplatesLocked();
    if (!EnsureExitStopEventLocked()) {
        MarkExitStartFailure(GetTickCount());
        g_notifyWnd = NULL;
        g_dataCS = NULL;
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }
    WakeAllConditionVariable(&g_exitStateChanged);
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

static BOOL PluginExit_CancelWithTimeout(DWORD waitMs);

BOOL PluginExit_Shutdown(void) {
    BOOL stopped = PluginExit_CancelWithTimeout(EXIT_COUNTDOWN_SHUTDOWN_WAIT_MS);
    if (!stopped) {
        LOG_WARNING("PluginExit: Countdown thread did not stop before shutdown timeout");
        return FALSE;
    }
    AcquireSRWLockExclusive(&g_exitLock);
    if (g_exitStopEvent) {
        CloseHandle(g_exitStopEvent);
        g_exitStopEvent = NULL;
    }
    g_notifyWnd = NULL;
    g_dataCS = NULL;
    InterlockedExchange(&g_exitInProgress, FALSE);
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

BOOL PluginExit_IsInProgress(void) {
    AcquireSRWLockExclusive(&g_exitLock);
    CleanupCompletedExitThreadHandleLocked();
    BOOL inProgress = IsExitInProgress();
    ReleaseSRWLockExclusive(&g_exitLock);
    return inProgress;
}

BOOL PluginExit_ParseTag(wchar_t* text, int* textLen, size_t maxLen) {
    if (!text || !textLen) return FALSE;
    BOOL templatesTouched = FALSE;

    /*
     * This function is called while plugin_data holds g_dataCS. Do not call
     * PluginExit_Cancel() here: it waits for the countdown thread, and that
     * thread also enters g_dataCS while updating the display text.
     */
    AcquireSRWLockExclusive(&g_exitLock);
    CleanupCompletedExitThreadHandleLocked();
    if (IsExitInProgress() || g_exitThread || g_exitThreadStopInProgress) {
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    DWORD now = GetTickCount();
    if (IsExitStartFailureCoolingDown(now)) {
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    wchar_t* start = wcsstr(text, L"<exit>");
    wchar_t* end = wcsstr(text, L"</exit>");

    if (!start || !end || end <= start) {
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }
    
    /* Parse countdown seconds (default 3) */
    int seconds = 3;
    const wchar_t* numStart = start + 6;
    BOOL validNumber = TRUE;
    
    if (numStart < end) {
        /* Skip whitespace */
        while (numStart < end && (*numStart == L' ' || *numStart == L'\t')) {
            numStart++;
        }
        
        if (numStart < end) {
            /* Validate numeric content */
            const wchar_t* p = numStart;
            while (p < end && *p != L' ' && *p != L'\t') {
                if (*p < L'0' || *p > L'9') {
                    validNumber = FALSE;
                    break;
                }
                p++;
            }

            if (validNumber) {
                int parsed = 0;
                const wchar_t* parsePtr = numStart;
                while (parsePtr < end && *parsePtr >= L'0' && *parsePtr <= L'9') {
                    int digit = (int)(*parsePtr - L'0');
                    if (parsed > (INT_MAX - digit) / 10) {
                        validNumber = FALSE;
                        break;
                    }
                    parsed = parsed * 10 + digit;
                    parsePtr++;
                }
                if (validNumber) {
                    if (parsed > 0) {
                        seconds = parsed > MAX_EXIT_COUNTDOWN_SECONDS
                            ? MAX_EXIT_COUNTDOWN_SECONDS
                            : parsed;
                    } else {
                        validNumber = FALSE;
                    }
                }
            }
        }
    }
    
    if (!validNumber) {
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }
    
    /* Save prefix (text before <exit>) */
    size_t prefixLen = start - text;
    templatesTouched = TRUE;
    if (g_exitPrefix) {
        free(g_exitPrefix);
        g_exitPrefix = NULL;
    }
    if (prefixLen > 0) {
        if (prefixLen > SIZE_MAX / sizeof(wchar_t) - 1) {
            goto fail_with_template_cleanup;
        }
        g_exitPrefix = (wchar_t*)malloc((prefixLen + 1) * sizeof(wchar_t));
        if (g_exitPrefix) {
            wcsncpy(g_exitPrefix, text, prefixLen);
            g_exitPrefix[prefixLen] = L'\0';
        } else {
            LOG_WARNING("PluginExit: Failed to allocate prefix buffer (%zu bytes)", (prefixLen + 1) * sizeof(wchar_t));
            goto fail_with_template_cleanup;
        }
    }
    
    /* Save suffix (text after </exit>) */
    wchar_t* suffixStart = end + 7;
    if (g_exitSuffix) {
        free(g_exitSuffix);
        g_exitSuffix = NULL;
    }
    if (*suffixStart) {
        size_t suffixLen = wcslen(suffixStart);
        if (suffixLen > SIZE_MAX / sizeof(wchar_t) - 1) {
            goto fail_with_template_cleanup;
        }
        g_exitSuffix = (wchar_t*)malloc((suffixLen + 1) * sizeof(wchar_t));
        if (g_exitSuffix) {
            wcsncpy(g_exitSuffix, suffixStart, suffixLen);
            g_exitSuffix[suffixLen] = L'\0';
        } else {
            LOG_WARNING("PluginExit: Failed to allocate suffix buffer (%zu bytes)", (suffixLen + 1) * sizeof(wchar_t));
            goto fail_with_template_cleanup;
        }
    }
    
    /* Replace <exit>N</exit> with just N */
    wchar_t countdownNum[16];
    _snwprintf_s(countdownNum, 16, _TRUNCATE, L"%d", seconds);
    
    size_t suffixLen = wcslen(suffixStart);
    size_t numLen = wcslen(countdownNum);
    if (prefixLen > SIZE_MAX - numLen ||
        prefixLen + numLen > SIZE_MAX - suffixLen ||
        prefixLen + numLen + suffixLen > SIZE_MAX - 1) {
        goto fail_with_template_cleanup;
    }
    size_t newLen = prefixLen + numLen + suffixLen + 1;
    
    if (newLen <= maxLen) {
        memmove(start + numLen, suffixStart, (suffixLen + 1) * sizeof(wchar_t));
        memcpy(start, countdownNum, numLen * sizeof(wchar_t));
        *textLen = (int)(prefixLen + numLen + suffixLen);
    }
    
    /* Start countdown thread */
    if (!EnsureExitStopEventLocked()) {
        MarkExitStartFailure(now);
        goto fail_with_template_cleanup;
    }
    InterlockedExchange(&g_exitInProgress, TRUE);
    g_exitThread = CreateThread(NULL, 0, ExitCountdownThread,
                                (LPVOID)(intptr_t)seconds, 0, NULL);
    if (!g_exitThread) {
        InterlockedExchange(&g_exitInProgress, FALSE);
        WakeAllConditionVariable(&g_exitStateChanged);
        LOG_ERROR("PluginExit: Failed to create countdown thread");
        MarkExitStartFailure(now);
        goto fail_with_template_cleanup;
    }
    g_exitLastStartFailureTick = 0;
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;

fail_with_template_cleanup:
    InterlockedExchange(&g_exitInProgress, FALSE);
    if (templatesTouched) {
        FreeExitTemplatesLocked();
    }
    WakeAllConditionVariable(&g_exitStateChanged);
    ReleaseSRWLockExclusive(&g_exitLock);
    return FALSE;
}

static BOOL PluginExit_CancelWithTimeout(DWORD waitMs) {
    HANDLE waitThread = NULL;
    BOOL ownsThreadStop = FALSE;
    BOOL stopped = TRUE;

    AcquireSRWLockExclusive(&g_exitLock);
    InterlockedExchange(&g_exitInProgress, FALSE);
    if (g_exitStopEvent) {
        SetEvent(g_exitStopEvent);
    }

    while (g_exitThreadStopInProgress) {
        SleepConditionVariableSRW(&g_exitStateChanged, &g_exitLock, INFINITE, 0);
    }

    CleanupCompletedExitThreadHandleLocked();
    if (g_exitThread) {
        waitThread = g_exitThread;
        g_exitThreadStopInProgress = TRUE;
        ownsThreadStop = TRUE;
    }
    ReleaseSRWLockExclusive(&g_exitLock);

    if (waitThread) {
        DWORD waitResult = WaitForSingleObject(waitThread, waitMs);
        stopped = (waitResult == WAIT_OBJECT_0);
        if (!stopped) {
            if (waitResult == WAIT_TIMEOUT) {
                LOG_WARNING("PluginExit: Countdown cancel timed out after %lu ms", waitMs);
            } else {
                LOG_WARNING("PluginExit: Countdown cancel wait failed (wait=%lu, error=%lu)",
                            waitResult, GetLastError());
            }
        }
    }

    AcquireSRWLockExclusive(&g_exitLock);
    if (ownsThreadStop && stopped) {
        if (g_exitThread) {
            CloseHandle(g_exitThread);
            g_exitThread = NULL;
        }
        g_exitThreadStopInProgress = FALSE;
        WakeAllConditionVariable(&g_exitStateChanged);
    }

    if (ownsThreadStop && !stopped) {
        g_exitThreadStopInProgress = FALSE;
        WakeAllConditionVariable(&g_exitStateChanged);
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    if (g_exitStopEvent) {
        ResetEvent(g_exitStopEvent);
    }

    FreeExitTemplatesLocked();
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

void PluginExit_Cancel(void) {
    (void)PluginExit_CancelWithTimeout(INFINITE);
}
