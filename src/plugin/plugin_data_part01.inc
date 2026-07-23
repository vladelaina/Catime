        !IsValidPluginDataNotifyWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, PLUGIN_DATA_REDRAW_TIMER_ID);
    g_pluginDataRedrawTimerHwnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
    g_lastPluginDataRedrawTick = GetTickCount();
    InvalidateRect(hwnd, NULL, FALSE);
    RecheckPluginDataTimerIfQueued(hwnd);
}

static void RequestPluginDataRedraw(HWND hwnd) {
    if (!IsValidPluginDataNotifyWindow(hwnd)) return;

    if (InterlockedCompareExchange(&g_pluginDataRedrawTimerArmed, 0, 0) != 0) {
        if (g_pluginDataRedrawTimerHwnd &&
            IsValidPluginDataNotifyWindow(g_pluginDataRedrawTimerHwnd)) {
            return;
        }
        StopPluginDataRedrawTimer(hwnd);
    }

    if (InterlockedCompareExchange(&g_pluginDataRedrawQueued, 1, 0) == 0) {
        if (!PostMessage(hwnd, CLOCK_WM_PLUGIN_DATA_REDRAW, 0, 0)) {
            InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        }
    }
}

void PluginData_HandleRedrawRequest(HWND hwnd) {
    if (!IsValidPluginDataNotifyWindow(hwnd)) {
        InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
        return;
    }
    if (!PluginData_BeginUse()) {
        InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
        return;
    }

    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    BOOL recheckTimer = InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0) != 0;

    DWORD now = GetTickCount();
    DWORD elapsed = now - g_lastPluginDataRedrawTick;
    if (g_lastPluginDataRedrawTick == 0 || elapsed >= PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS) {
        StopPluginDataRedrawTimer(hwnd);
        g_lastPluginDataRedrawTick = now;
        InvalidateRect(hwnd, NULL, FALSE);
        PluginData_EndUse();
        if (recheckTimer) {
            QueuePluginDataTimerRecheck();
            RecheckPluginDataTimerIfQueued(hwnd);
        }
        return;
    }

    if (!SetTimer(hwnd, PLUGIN_DATA_REDRAW_TIMER_ID,
                  PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS - elapsed,
                  PluginDataRedrawTimerProc)) {
        g_pluginDataRedrawTimerHwnd = NULL;
        InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
        g_lastPluginDataRedrawTick = now;
        InvalidateRect(hwnd, NULL, FALSE);
    } else {
        g_pluginDataRedrawTimerHwnd = hwnd;
        InterlockedExchange(&g_pluginDataRedrawTimerArmed, 1);
    }
    PluginData_EndUse();
    if (recheckTimer) {
        QueuePluginDataTimerRecheck();
        RecheckPluginDataTimerIfQueued(hwnd);
    }
}

static BOOL ParseNonNegativeIntLimitedA(const char* start, const char* end, int* outValue) {
    int value = 0;
    if (!start || !end || !outValue || start >= end) return FALSE;

    while (start < end) {
        int digit;
        if (*start < '0' || *start > '9') return FALSE;
        digit = *start - '0';
        if (value > (INT_MAX - digit) / 10) return FALSE;
        value = value * 10 + digit;
        start++;
    }

    *outValue = value;
    return TRUE;
}

static BOOL ParseNonNegativeIntLimitedW(const wchar_t* start, const wchar_t* end, int* outValue) {
    int value = 0;
    if (!start || !end || !outValue || start >= end) return FALSE;

    while (start < end) {
        int digit;
        if (*start < L'0' || *start > L'9') return FALSE;
        digit = (int)(*start - L'0');
        if (value > (INT_MAX - digit) / 10) return FALSE;
        value = value * 10 + digit;
        start++;
    }

    *outValue = value;
    return TRUE;
}

/* Cache for change detection */
static char* g_lastContent = NULL;
static size_t g_lastContentSize = 0;
static size_t g_lastContentCapacity = 0;
static FILETIME g_lastOutputWriteTime = {0};
static ULONGLONG g_lastOutputFileSize = 0;
static BOOL g_hasLastOutputFileState = FALSE;
static wchar_t g_pluginOutputDirectory[MAX_PATH] = {0};
static wchar_t g_displaySourcePath[MAX_PATH] = {0};

/* Dynamic poll interval (controlled by <fps:N> tag) */
#define DEFAULT_POLL_INTERVAL_MS 500
#define MIN_POLL_INTERVAL_MS PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS
#define MAX_POLL_INTERVAL_MS 5000
#define MAX_PLUGIN_OUTPUT_BYTES (10ull * 1024ull * 1024ull)
#define PLUGIN_DISPLAY_MAX_INPUT_BYTES 4096
#define PLUGIN_OUTPUT_READ_LIMIT_BYTES (PLUGIN_DISPLAY_MAX_INPUT_BYTES + 4)
#define PLUGIN_OUTPUT_STACK_BUFFER_BYTES 1024
#define PLUGIN_DISPLAY_STACK_WCHARS 1024
#define PLUGIN_LAST_CONTENT_RETAIN_BYTES (64 * 1024)
#define MAX_CHANGE_DEBOUNCE_MS 50
#define PLUGIN_OUTPUT_FILE_SHARE (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
static volatile LONG g_pollIntervalMs = DEFAULT_POLL_INTERVAL_MS;

/* Notification throttling */
#define NOTIFY_MIN_INTERVAL_MS 1000
static DWORD g_lastNotifyTime = 0;

/* Pending notification for main thread */
typedef struct {
    wchar_t message[1025];
    int type;           /* -1 = default, 0/1/2 = specific type */
    int timeout;        /* 0 = default, >0 = custom timeout ms */
    BOOL pending;
} PendingNotification;

static PendingNotification g_pendingNotify = {0};

static void ResetPendingNotificationLocked(void) {
    ZeroMemory(&g_pendingNotify, sizeof(g_pendingNotify));
    g_lastNotifyTime = 0;
}

typedef enum {
    PLUGIN_PARSE_FAILED = 0,
    PLUGIN_PARSE_OK,
    PLUGIN_PARSE_TRANSIENT_FAILURE
} PluginParseResult;

static BOOL IsWatcherRunning(void) {
    return InterlockedCompareExchange(&g_isRunning, FALSE, FALSE) != FALSE;
}

static void SetWatcherRunning(BOOL running) {
    InterlockedExchange(&g_isRunning, running ? TRUE : FALSE);
}

static BOOL IsWatcherStartFailureCoolingDown(DWORD now) {
    return g_watchStartFailureCooldownUntil != 0 &&
           (LONG)(g_watchStartFailureCooldownUntil - now) > 0;
}

static void MarkWatcherStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PLUGIN_DATA_WATCHER_START_FAILURE_COOLDOWN_MS;
    g_watchStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static void CloseWatcherEventsIfIdleLocked(void) {
    if (g_hWatchThread || g_watchStopInProgress) return;

    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
}

static DWORD GetPollIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_pollIntervalMs, 0, 0);
    return interval > 0 ? (DWORD)interval : DEFAULT_POLL_INTERVAL_MS;
}

static void SetPollIntervalMs(DWORD intervalMs) {
    if (intervalMs == 0) intervalMs = DEFAULT_POLL_INTERVAL_MS;
    if (intervalMs > (DWORD)LONG_MAX) intervalMs = (DWORD)LONG_MAX;
    InterlockedExchange(&g_pollIntervalMs, (LONG)intervalMs);
}

static void ApplyContentPollInterval(BOOL hasFpsTag, DWORD parsedPollInterval) {
    DWORD intervalMs = hasFpsTag ? parsedPollInterval : DEFAULT_POLL_INTERVAL_MS;
    if (GetPollIntervalMs() != intervalMs) {
        SetPollIntervalMs(intervalMs);
    }
}

static void WakeWatcherThreadLocked(void) {
    if (g_hWatchWakeEvent) {
        SetEvent(g_hWatchWakeEvent);
    }
}

static void WakeWatcherThread(void) {
    EnterCriticalSection(&g_watchCS);
    WakeWatcherThreadLocked();
    LeaveCriticalSection(&g_watchCS);
}

static void CleanupCompletedWatcherThreadLocked(void) {
    HANDLE hThread = g_hWatchThread;
    if (!hThread || g_watchStopInProgress) return;

    if (WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(hThread);
        g_hWatchThread = NULL;
        SetWatcherRunning(FALSE);
        WakeAllConditionVariable(&g_watchStopCompleted);
    }
}

static BOOL WaitForWatcherStopGateLocked(DWORD waitMs) {
    DWORD waitStart = GetTickCount();
    while (g_watchStopInProgress) {
        DWORD elapsed = GetTickCount() - waitStart;
        DWORD remaining = elapsed >= waitMs ? 0 : waitMs - elapsed;
        if (remaining == 0 ||
            !SleepConditionVariableCS(&g_watchStopCompleted, &g_watchCS, remaining)) {
            LOG_WARNING("PluginData: Timed out waiting for watcher stop gate after %lu ms",
                        waitMs);
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Parse <fps:N> tag from content without touching shared state
 */
static BOOL TryParseFpsPollInterval(const char* content, DWORD* intervalOut, int* fpsOut) {
    if (!content || !intervalOut) return FALSE;

    /* Look for <fps:N> pattern */
    const char* fpsStart = strstr(content, "<fps:");
    if (!fpsStart) return FALSE;

    const char* numStart = fpsStart + 5;  /* Skip "<fps:" */
    const char* numEnd = numStart;
