/**
 * @file plugin_data_state.c
 * @brief Shared state, display storage, and lifecycle locking.
 */

#include "plugin_data_internal.h"

wchar_t* g_pluginDisplayText = NULL;
size_t g_pluginDisplayTextLen = 0;
BOOL g_hasPluginData = FALSE;

BOOL g_pluginModeActive = FALSE;
volatile LONG g_forceNextUpdate = FALSE;
CRITICAL_SECTION g_dataCS;
SRWLOCK g_pluginDataLifecycleLock = SRWLOCK_INIT;
BOOL g_pluginDataInitialized = FALSE;
BOOL g_pluginDataLocksInitialized = FALSE;
BOOL g_pluginDataResourcesRetained = FALSE;

HANDLE g_hWatchThread = NULL;
HANDLE g_hWatchStopEvent = NULL;
HANDLE g_hWatchWakeEvent = NULL;
HWND g_hNotifyWnd = NULL;
CRITICAL_SECTION g_watchCS;
CONDITION_VARIABLE g_watchStopCompleted = CONDITION_VARIABLE_INIT;
BOOL g_watchStopInProgress = FALSE;
volatile LONG g_isRunning = FALSE;

DWORD g_lastPluginDataRedrawTick = 0;
DWORD g_watchStartFailureCooldownUntil = 0;
volatile LONG g_pluginDataRedrawQueued = 0;
volatile LONG g_pluginDataRedrawTimerArmed = 0;
volatile LONG g_pluginDataTimerRecheckQueued = 0;
HWND g_pluginDataRedrawTimerHwnd = NULL;

char* g_lastContent = NULL;
size_t g_lastContentSize = 0;
size_t g_lastContentCapacity = 0;
FILETIME g_lastOutputWriteTime = {0};
ULONGLONG g_lastOutputFileSize = 0;
BOOL g_hasLastOutputFileState = FALSE;
wchar_t g_pluginOutputDirectory[MAX_PATH] = {0};
wchar_t g_displaySourcePath[MAX_PATH] = {0};
volatile LONG g_pollIntervalMs = DEFAULT_POLL_INTERVAL_MS;
DWORD g_lastNotifyTime = 0;
PendingNotification g_pendingNotify = {0};

void ClearPluginDisplayTextLocked(void) {
    if (!g_pluginDisplayText) return;

    if (g_pluginDisplayTextLen > PLUGIN_DISPLAY_RETAIN_WCHARS) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
        g_pluginDisplayTextLen = 0;
        return;
    }

    g_pluginDisplayText[0] = L'\0';
}

BOOL EnsurePluginDisplayTextCapacityLocked(size_t requiredChars) {
    if (requiredChars == 0) return FALSE;
    if (requiredChars > SIZE_MAX / sizeof(wchar_t)) {
        LOG_ERROR("PluginData: Display buffer size overflow (%zu chars)", requiredChars);
        return FALSE;
    }

    if (g_pluginDisplayText &&
        g_pluginDisplayTextLen > PLUGIN_DISPLAY_RETAIN_WCHARS &&
        requiredChars <= PLUGIN_DISPLAY_RETAIN_WCHARS) {
        wchar_t* resized = (wchar_t*)realloc(
            g_pluginDisplayText,
            PLUGIN_DISPLAY_RETAIN_WCHARS * sizeof(wchar_t));
        if (resized) {
            g_pluginDisplayText = resized;
            g_pluginDisplayTextLen = PLUGIN_DISPLAY_RETAIN_WCHARS;
            g_pluginDisplayText[0] = L'\0';
        }
    }

    if (g_pluginDisplayText && g_pluginDisplayTextLen >= requiredChars) {
        return TRUE;
    }

    wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, requiredChars * sizeof(wchar_t));
    if (!newBuf) {
        LOG_ERROR("PluginData: Failed to allocate %zu bytes", requiredChars * sizeof(wchar_t));
        return FALSE;
    }

    g_pluginDisplayText = newBuf;
    g_pluginDisplayTextLen = requiredChars;
    return TRUE;
}

BOOL PluginTextHasCatimeTagW(const wchar_t* text) {
    if (!text) return FALSE;

    const wchar_t* start = wcsstr(text, L"<catime>");
    const wchar_t* end = wcsstr(text, L"</catime>");
    return start && end && end > start;
}

BOOL PluginDisplayHasCatimeTagLocked(void) {
    return g_pluginModeActive &&
           g_hasPluginData &&
           g_pluginDisplayText &&
           PluginTextHasCatimeTagW(g_pluginDisplayText);
}

void QueuePluginDataTimerRecheck(void) {
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 1);
}

BOOL PluginData_BeginUse(void) {
    AcquireSRWLockShared(&g_pluginDataLifecycleLock);
    if (!g_pluginDataInitialized) {
        ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
        return FALSE;
    }
    return TRUE;
}

void PluginData_EndUse(void) {
    ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
}

void ResetPendingNotificationLocked(void) {
    ZeroMemory(&g_pendingNotify, sizeof(g_pendingNotify));
    g_lastNotifyTime = 0;
}
