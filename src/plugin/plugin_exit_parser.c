#include "plugin/plugin_exit.h"
#include "plugin_exit_internal.h"
#include "log.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static BOOL ParseExitSeconds(const wchar_t* start, const wchar_t* end,
                             int* seconds) {
    if (!start || !end || !seconds) return FALSE;
    *seconds = 3;
    const wchar_t* cursor = start + 6;
    while (cursor < end && (*cursor == L' ' || *cursor == L'\t')) cursor++;
    if (cursor >= end) return TRUE;
    int parsed = 0;
    BOOL hasDigit = FALSE;
    while (cursor < end && *cursor != L' ' && *cursor != L'\t') {
        if (*cursor < L'0' || *cursor > L'9') return FALSE;
        int digit = (int)(*cursor - L'0');
        if (parsed > (INT_MAX - digit) / 10) return FALSE;
        parsed = parsed * 10 + digit;
        hasDigit = TRUE;
        cursor++;
    }
    while (cursor < end) {
        if (*cursor != L' ' && *cursor != L'\t') return FALSE;
        cursor++;
    }
    if (!hasDigit || parsed <= 0) return FALSE;
    *seconds = parsed > MAX_EXIT_COUNTDOWN_SECONDS
        ? MAX_EXIT_COUNTDOWN_SECONDS : parsed;
    return TRUE;
}

static BOOL CopyExitTemplate(wchar_t** destination, const wchar_t* source,
                             size_t length) {
    free(*destination);
    *destination = NULL;
    if (length == 0) return TRUE;
    if (!source || length > SIZE_MAX / sizeof(wchar_t) - 1) return FALSE;
    wchar_t* value = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!value) return FALSE;
    memcpy(value, source, length * sizeof(wchar_t));
    value[length] = L'\0';
    *destination = value;
    return TRUE;
}

BOOL PluginExit_ParseTag(wchar_t* text, int* textLen, size_t maxLen) {
    if (!text || !textLen) return FALSE;
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
    int seconds = 0;
    if (!start || !end || end <= start ||
        !ParseExitSeconds(start, end, &seconds)) {
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }
    size_t prefixLen = (size_t)(start - text);
    wchar_t* suffixStart = end + 7;
    size_t suffixLen = wcslen(suffixStart);
    if (!CopyExitTemplate(&g_exitPrefix, text, prefixLen) ||
        !CopyExitTemplate(&g_exitSuffix, suffixStart, suffixLen)) {
        LOG_WARNING("PluginExit: Failed to allocate countdown template");
        goto fail;
    }
    wchar_t countdownNum[16];
    _snwprintf_s(countdownNum, _countof(countdownNum), _TRUNCATE,
                 L"%d", seconds);
    size_t numLen = wcslen(countdownNum);
    if (prefixLen > SIZE_MAX - numLen ||
        prefixLen + numLen > SIZE_MAX - suffixLen ||
        prefixLen + numLen + suffixLen > SIZE_MAX - 1) goto fail;
    size_t newLen = prefixLen + numLen + suffixLen + 1;
    if (newLen <= maxLen) {
        memmove(start + numLen, suffixStart,
                (suffixLen + 1) * sizeof(wchar_t));
        memcpy(start, countdownNum, numLen * sizeof(wchar_t));
        *textLen = (int)(prefixLen + numLen + suffixLen);
    }
    if (!EnsureExitStopEventLocked()) {
        MarkExitStartFailure(now);
        goto fail;
    }
    InterlockedExchange(&g_exitInProgress, TRUE);
    g_exitThread = CreateThread(NULL, 0, ExitCountdownThread,
                                (LPVOID)(intptr_t)seconds, 0, NULL);
    if (!g_exitThread) {
        InterlockedExchange(&g_exitInProgress, FALSE);
        WakeAllConditionVariable(&g_exitStateChanged);
        LOG_ERROR("PluginExit: Failed to create countdown thread");
        MarkExitStartFailure(now);
        goto fail;
    }
    g_exitStartFailureCooldownUntil = 0;
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
fail:
    InterlockedExchange(&g_exitInProgress, FALSE);
    FreeExitTemplatesLocked();
    WakeAllConditionVariable(&g_exitStateChanged);
    ReleaseSRWLockExclusive(&g_exitLock);
    return FALSE;
}
