/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring
 */

#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "config.h"
#include "notification.h"
#include "../resource/resource.h"
#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define PLUGIN_DISPLAY_RETAIN_WCHARS 4096

/* ============================================================================
 * Shared State (exported for plugin_exit.c)
 * ============================================================================ */

wchar_t* g_pluginDisplayText = NULL;
size_t g_pluginDisplayTextLen = 0;
BOOL g_hasPluginData = FALSE;

static void ClearPluginDisplayTextLocked(void) {
    if (!g_pluginDisplayText) return;

    if (g_pluginDisplayTextLen > PLUGIN_DISPLAY_RETAIN_WCHARS) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
        g_pluginDisplayTextLen = 0;
        return;
    }

    g_pluginDisplayText[0] = L'\0';
}

static BOOL EnsurePluginDisplayTextCapacityLocked(size_t requiredChars) {
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

/* ============================================================================
 * Internal State
 * ============================================================================ */

static BOOL g_pluginModeActive = FALSE;
static volatile LONG g_forceNextUpdate = FALSE;
static CRITICAL_SECTION g_dataCS;
static SRWLOCK g_pluginDataLifecycleLock = SRWLOCK_INIT;
static BOOL g_pluginDataInitialized = FALSE;
static BOOL g_pluginDataLocksInitialized = FALSE;
static BOOL g_pluginDataResourcesRetained = FALSE;

/* Watcher thread */
static HANDLE g_hWatchThread = NULL;
static HANDLE g_hWatchStopEvent = NULL;
static HANDLE g_hWatchWakeEvent = NULL;
static HWND g_hNotifyWnd = NULL;
static CRITICAL_SECTION g_watchCS;
static CONDITION_VARIABLE g_watchStopCompleted = CONDITION_VARIABLE_INIT;
static BOOL g_watchStopInProgress = FALSE;
static volatile LONG g_isRunning = FALSE;

#define PLUGIN_DATA_REDRAW_TIMER_ID 42424
#define PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS 100
#define PLUGIN_DATA_WATCHER_SHUTDOWN_WAIT_MS 2000
#define PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS 1000
#define PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS 1000
#define PLUGIN_DATA_WATCHER_START_FAILURE_COOLDOWN_MS 2000

static DWORD g_lastPluginDataRedrawTick = 0;
static DWORD g_watchStartFailureCooldownUntil = 0;
static volatile LONG g_pluginDataRedrawQueued = 0;
static volatile LONG g_pluginDataRedrawTimerArmed = 0;
static volatile LONG g_pluginDataTimerRecheckQueued = 0;
static HWND g_pluginDataRedrawTimerHwnd = NULL;

static BOOL PluginTextHasCatimeTagW(const wchar_t* text) {
    if (!text) return FALSE;

    const wchar_t* start = wcsstr(text, L"<catime>");
    const wchar_t* end = wcsstr(text, L"</catime>");
    return start && end && end > start;
}

static BOOL PluginDisplayHasCatimeTagLocked(void) {
    return g_pluginModeActive &&
           g_hasPluginData &&
           g_pluginDisplayText &&
           PluginTextHasCatimeTagW(g_pluginDisplayText);
}

static void QueuePluginDataTimerRecheck(void) {
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 1);
}

static BOOL PluginData_BeginUse(void) {
    AcquireSRWLockShared(&g_pluginDataLifecycleLock);
    if (!g_pluginDataInitialized) {
        ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
        return FALSE;
    }
    return TRUE;
}

static void PluginData_EndUse(void) {
    ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
}

static BOOL IsValidPluginDataNotifyWindow(HWND hwnd) {
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

static void RecheckPluginDataTimerIfQueued(HWND hwnd) {
    if (InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0) != 0 &&
        IsValidPluginDataNotifyWindow(hwnd)) {
        ResetTimerWithInterval(hwnd);
    }
}

static void StopPluginDataRedrawTimer(HWND fallbackHwnd) {
    HWND timerHwnd = g_pluginDataRedrawTimerHwnd ? g_pluginDataRedrawTimerHwnd : fallbackHwnd;
    if (InterlockedCompareExchange(&g_pluginDataRedrawTimerArmed, 0, 0) != 0 &&
        IsValidPluginDataNotifyWindow(timerHwnd)) {
        KillTimer(timerHwnd, PLUGIN_DATA_REDRAW_TIMER_ID);
    }
    g_pluginDataRedrawTimerHwnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
}

static void CALLBACK PluginDataRedrawTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    if (msg != WM_TIMER ||
        id != PLUGIN_DATA_REDRAW_TIMER_ID ||
        hwnd != g_pluginDataRedrawTimerHwnd ||
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
    
    /* Parse number */
    while (*numEnd >= '0' && *numEnd <= '9') {
        numEnd++;
    }

    /* Check for closing > */
    if (*numEnd != '>' || numEnd == numStart) {
        return FALSE;  /* Invalid format */
    }

    int fps = 0;
    if (!ParseNonNegativeIntLimitedA(numStart, numEnd, &fps) || fps <= 0) {
        return FALSE;
    }

    /* Convert fps to poll interval: interval = 1000 / fps */
    DWORD interval = 1000 / fps;

    /* Clamp to valid range */
    if (interval < MIN_POLL_INTERVAL_MS) interval = MIN_POLL_INTERVAL_MS;
    if (interval > MAX_POLL_INTERVAL_MS) interval = MAX_POLL_INTERVAL_MS;

    *intervalOut = interval;
    if (fpsOut) {
        *fpsOut = fps;
    }
    return TRUE;
}

static size_t ClampUtf8DisplayInputLength(const char* content, size_t contentLen) {
    if (!content || contentLen <= PLUGIN_DISPLAY_MAX_INPUT_BYTES) {
        return contentLen;
    }

    size_t len = PLUGIN_DISPLAY_MAX_INPUT_BYTES;
    size_t seqStart = len;
    while (seqStart > 0 && (((unsigned char)content[seqStart - 1] & 0xC0u) == 0x80u)) {
        seqStart--;
    }

    if (seqStart == 0) {
        return 0;
    }

    if (seqStart == len) {
        unsigned char last = (unsigned char)content[len - 1];
        if ((last & 0x80u) == 0 || (last & 0xC0u) == 0xC0u) {
            return len - (((last & 0xC0u) == 0xC0u) ? 1u : 0u);
        }
        return len - 1;
    }

    unsigned char lead = (unsigned char)content[seqStart - 1];
    size_t expected = 0;
    if ((lead & 0xE0u) == 0xC0u) expected = 2;
    else if ((lead & 0xF0u) == 0xE0u) expected = 3;
    else if ((lead & 0xF8u) == 0xF0u) expected = 4;
    else return seqStart - 1;

    return (seqStart - 1 + expected <= len) ? len : seqStart - 1;
}

/**
 * @brief Remove <fps:N> tag from wide string for display
 */
static void RemoveFpsTagW(wchar_t* text) {
    if (!text) return;
    
    wchar_t* fpsStart = wcsstr(text, L"<fps:");
    if (!fpsStart) return;
    
    wchar_t* fpsEnd = wcschr(fpsStart, L'>');
    if (!fpsEnd) return;
    
    /* Move everything after tag to tag position */
    memmove(fpsStart, fpsEnd + 1, (wcslen(fpsEnd + 1) + 1) * sizeof(wchar_t));
}

/**
 * @brief Parse and process <notify> tags from wide string
 * 
 * Supported formats:
 * - <notify>message</notify>             - Use default notification type and timeout
 * - <notify:catime>message</notify>      - Force Catime notification window
 * - <notify:os>message</notify>          - Force OS system notification
 * - <notify:modal>message</notify>       - Force system modal dialog
 * - <notify:catime:5000>message</notify> - Catime notification with custom timeout (ms)
 * 
 * @param text The text to parse (will be modified to remove tags)
 * @param hwnd Window handle for notification display
 * 
 * @note This function posts a message to the main thread to show the notification,
 *       avoiding UI operations from the background watcher thread.
 */
static void ParseAndShowNotifyTagW(wchar_t* text, HWND hwnd) {
    if (!text || !hwnd) return;
    
    /* Only process notifications when plugin mode is active */
    if (!g_pluginModeActive) return;
    
    wchar_t* searchStart = text;
    
    while (1) {
        /* Find <notify tag start */
        wchar_t* notifyStart = wcsstr(searchStart, L"<notify");
        if (!notifyStart) break;
        
        /* Find closing > of opening tag */
        wchar_t* tagEnd = wcschr(notifyStart, L'>');
        if (!tagEnd) break;
        
        /* Find </notify> closing tag */
        wchar_t* closeTag = wcsstr(tagEnd, L"</notify>");
        if (!closeTag) break;
        
        /* Extract message content (between > and </notify>) */
        size_t msgLen = closeTag - (tagEnd + 1);
        if (msgLen == 0 || msgLen > 1024) {
            /* Empty or too long message, skip this tag */
            searchStart = closeTag + 9;
            continue;
        }
        
        wchar_t message[1025] = {0};
        wcsncpy(message, tagEnd + 1, msgLen);
        message[msgLen] = L'\0';
        
        /* Parse notification type and timeout from tag attributes */
        /* Format: <notify> or <notify:type> or <notify:type:timeout> */
        int notifyType = -1;  /* -1 = use default */
        int customTimeout = 0;  /* 0 = use default */
        
        wchar_t* colonPos = wcschr(notifyStart + 7, L':');
        if (colonPos && colonPos < tagEnd) {
            /* Has type parameter */
            const wchar_t* typeStart = colonPos + 1;
            wchar_t* typeEnd = wcschr(typeStart, L':');
            if (!typeEnd || typeEnd > tagEnd) {
                typeEnd = tagEnd;
            }
            
            size_t typeLen = typeEnd - typeStart;
            if (typeLen > 0 && typeLen < 16) {
                wchar_t typeStr[16] = {0};
                wcsncpy(typeStr, typeStart, typeLen);
                typeStr[typeLen] = L'\0';
                
                if (_wcsicmp(typeStr, L"catime") == 0) {
                    notifyType = NOTIFICATION_TYPE_CATIME;
                } else if (_wcsicmp(typeStr, L"os") == 0) {
                    notifyType = NOTIFICATION_TYPE_OS;
                } else if (_wcsicmp(typeStr, L"modal") == 0) {
                    notifyType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
            }
            
            /* Check for timeout parameter (only for toast) */
            if (typeEnd < tagEnd && *typeEnd == L':') {
                const wchar_t* timeoutStart = typeEnd + 1;
                ParseNonNegativeIntLimitedW(timeoutStart, tagEnd, &customTimeout);
                if (customTimeout < 0) customTimeout = 0;
                if (customTimeout > 60000) customTimeout = 60000;  /* Max 60 seconds */
            }
        }
        
        /* Throttle: check if enough time has passed since last notification */
        /* Note: GetTickCount wraps around after ~49.7 days, but the subtraction
         * still works correctly due to unsigned arithmetic */
        DWORD now = GetTickCount();
        DWORD elapsed = now - g_lastNotifyTime;
        if (elapsed >= NOTIFY_MIN_INTERVAL_MS) {
            /* Store pending notification for main thread to process
             * Note: This is called from ParseContent which already holds g_dataCS,
             * so we don't need to acquire the lock here. The main thread will
             * acquire the lock in PluginData_ProcessPendingNotification. */
            wcsncpy(g_pendingNotify.message, message, 1024);
            g_pendingNotify.message[1024] = L'\0';
            g_pendingNotify.type = notifyType;
            g_pendingNotify.timeout = customTimeout;
            g_pendingNotify.pending = TRUE;
            
            /* Post message to main thread to show notification */
            if (!IsValidPluginDataNotifyWindow(hwnd) ||
                !PostMessage(hwnd, WM_PLUGIN_NOTIFY, 0, 0)) {
                g_pendingNotify.pending = FALSE;
            } else {
                g_lastNotifyTime = now;
            }
        }
        
        /* Remove the entire <notify>...</notify> tag from text */
        wchar_t* afterClose = closeTag + 9;  /* Skip "</notify>" */
        memmove(notifyStart, afterClose, (wcslen(afterClose) + 1) * sizeof(wchar_t));
        
        /* Continue searching from same position (text shifted) */
        searchStart = notifyStart;
    }
}

/**
 * @brief Parse plain text content and update display text
 * 
 * The file content is displayed as-is, supporting:
 * - Plain text
 * - Multi-line text (real newlines, no \n escaping needed)
 * - Markdown formatting
 */
static PluginParseResult ParseContent(const char* content, size_t contentLen,
                                      BOOL* displayChangedOut,
                                      BOOL* timerRecheckOut) {
    if (displayChangedOut) {
        *displayChangedOut = FALSE;
    }
    if (timerRecheckOut) {
        *timerRecheckOut = FALSE;
    }
    if (!content || contentLen == 0) return PLUGIN_PARSE_FAILED;

    DWORD parsedPollInterval = 0;
    BOOL hasFpsTag = TryParseFpsPollInterval(content, &parsedPollInterval, NULL);
    size_t displayInputLen = ClampUtf8DisplayInputLength(content, contentLen);

    /* Convert outside g_dataCS so paint-time reads are not blocked by UTF-8 work. */
    int requiredLen = MultiByteToWideChar(CP_UTF8, 0, content, (int)displayInputLen, NULL, 0);
    if (requiredLen <= 0) {
        return PLUGIN_PARSE_FAILED;
    }

    if (requiredLen > INT_MAX - 1) {
        return PLUGIN_PARSE_FAILED;
    }
    size_t requiredSize = (size_t)(requiredLen + 1);
    if (requiredSize > SIZE_MAX / sizeof(wchar_t)) {
        return PLUGIN_PARSE_FAILED;
    }

    wchar_t stackText[PLUGIN_DISPLAY_STACK_WCHARS];
    wchar_t* heapText = NULL;
    wchar_t* displayText = stackText;
    if (requiredSize > _countof(stackText)) {
        heapText = (wchar_t*)malloc(requiredSize * sizeof(wchar_t));
        if (!heapText) {
            LOG_ERROR("PluginData: Failed to allocate %zu bytes", requiredSize * sizeof(wchar_t));
            return PLUGIN_PARSE_TRANSIENT_FAILURE;
        }
        displayText = heapText;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, content, (int)displayInputLen,
                                  displayText, (int)requiredSize);
    if (len <= 0) {
        free(heapText);
        return PLUGIN_PARSE_FAILED;
    }
    displayText[len] = L'\0';

    /* Remove BOM if present */
    if (displayText[0] == 0xFEFF) {
        memmove(displayText, &displayText[1], len * sizeof(wchar_t));
        len--;
    }

    /* Trim trailing whitespace */
    while (len > 0 && (displayText[len - 1] == L'\n' ||
                       displayText[len - 1] == L'\r' ||
                       displayText[len - 1] == L' ')) {
        displayText[--len] = L'\0';
    }

    /* Remove <fps:N> tag from display before taking the shared data lock. */
    RemoveFpsTagW(displayText);

    EnterCriticalSection(&g_dataCS);

    if (!g_pluginModeActive) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_FAILED;
    }

    BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();

    ApplyContentPollInterval(hasFpsTag, parsedPollInterval);

    if (PluginExit_IsInProgress()) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_OK;
    }

    /* Process <notify> tags while holding g_dataCS for pending-notification state. */
    ParseAndShowNotifyTagW(displayText, g_hNotifyWnd);

    len = (int)wcslen(displayText);
    size_t displaySize = (size_t)len + 1;

    BOOL displayChanged = !g_hasPluginData ||
                          !g_pluginDisplayText ||
                          wcscmp(g_pluginDisplayText, displayText) != 0;
    BOOL hasExitTag = wcsstr(displayText, L"<exit>") != NULL &&
                      wcsstr(displayText, L"</exit>") != NULL;

    if (!displayChanged && !hasExitTag) {
        g_hasPluginData = TRUE;
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_OK;
    }

    if (!EnsurePluginDisplayTextCapacityLocked(displaySize)) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_TRANSIENT_FAILURE;
    }

    memcpy(g_pluginDisplayText, displayText, displaySize * sizeof(wchar_t));

    /* Process <exit> tag - if countdown starts, set data flag and return */
    if (PluginExit_ParseTag(g_pluginDisplayText, &len, g_pluginDisplayTextLen)) {
        g_hasPluginData = TRUE;
        BOOL hasCatimeTag = PluginDisplayHasCatimeTagLocked();
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        if (displayChangedOut) {
            *displayChangedOut = TRUE;
        }
        if (timerRecheckOut && hadCatimeTag != hasCatimeTag) {
            *timerRecheckOut = TRUE;
        }
        return PLUGIN_PARSE_OK;
    }

    g_hasPluginData = TRUE;
    BOOL hasCatimeTag = PluginDisplayHasCatimeTagLocked();
    LeaveCriticalSection(&g_dataCS);

    free(heapText);
    if (displayChangedOut) {
        *displayChangedOut = displayChanged;
    }
    if (timerRecheckOut && hadCatimeTag != hasCatimeTag) {
        *timerRecheckOut = TRUE;
    }
    return PLUGIN_PARSE_OK;
}

/*
 * Design note: output.txt is intentionally a stable, shared local IPC surface,
 * but it is only consumed while plugin mode is active.
 * During plugin mode, the active plugin and other same-user helper processes may
 * write compatible content here to build broader local automation flows.
 * Outside plugin mode, Catime intentionally ignores this file.
 * Catime-owned local state text such as "Loading..." or "FAIL" is also allowed
 * to override the file-driven content when needed.
 */
/* Plugin output file name */
#define PLUGIN_OUTPUT_FILENAME "output.txt"
#define PLUGIN_OUTPUT_FILENAME_W L"output.txt"

/**
 * @brief Get plugin output file path
 * @return TRUE if successful, FALSE otherwise
 */
static BOOL GetPluginOutputPathW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    DWORD result = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Catime\\resources\\plugins\\" PLUGIN_OUTPUT_FILENAME_W,
        buffer,
        (DWORD)bufferSize);
    if (result == 0 || result >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Ensure plugin output directory exists
 * @note Only creates directory, does NOT clear or modify output.txt
 *       User may have pre-defined content in output.txt
 */
static void EnsureOutputDirExistsW(const wchar_t* filePath) {
    /* First ensure the directory exists */
    wchar_t dirPath[MAX_PATH];
    if (!filePath || wcslen(filePath) >= MAX_PATH) return;
    wcsncpy(dirPath, filePath, MAX_PATH - 1);
    dirPath[MAX_PATH - 1] = L'\0';

    /* Find last backslash to get directory path */
    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        /* Create directory (and parent directories if needed) */
        /* SHCreateDirectoryExW creates all intermediate directories */
        SHCreateDirectoryExW(NULL, dirPath, NULL);
    }
}

static BOOL GetPluginOutputDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    DWORD result = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Catime\\resources\\plugins",
        buffer,
        (DWORD)bufferSize);
    if (result == 0 || result >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static size_t ChooseLastContentCacheCapacity(size_t requiredSize) {
    size_t capacity = PLUGIN_OUTPUT_STACK_BUFFER_BYTES + 1;
    while (capacity < requiredSize && capacity < PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        capacity *= 2;
    }
    return capacity < requiredSize ? requiredSize : capacity;
}

static BOOL UpdateLastContentCache(const char* content, DWORD contentSize) {
    size_t requiredSize = (size_t)contentSize + 1;
    if (requiredSize > g_lastContentCapacity ||
        (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES &&
         requiredSize <= PLUGIN_LAST_CONTENT_RETAIN_BYTES)) {
        size_t newCapacity = ChooseLastContentCacheCapacity(requiredSize);
        char* newBuf = (char*)realloc(g_lastContent, newCapacity);
        if (!newBuf) {
            LOG_ERROR("PluginData: Failed to resize last-content cache to %zu bytes", newCapacity);
            if (g_lastContent && g_lastContentCapacity > 0) {
                g_lastContent[0] = '\0';
            }
            g_lastContentSize = 0;
            return FALSE;
        }
        g_lastContent = newBuf;
        g_lastContentCapacity = newCapacity;
    }

    g_lastContentSize = requiredSize;
    memcpy(g_lastContent, content, contentSize);
    g_lastContent[contentSize] = '\0';
    return TRUE;
}

static void ClearLastContentCacheLocked(void) {
    if (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        free(g_lastContent);
        g_lastContent = NULL;
        g_lastContentCapacity = 0;
        g_lastContentSize = 0;
        return;
    }
    if (g_lastContent && g_lastContentSize > 0) {
        g_lastContent[0] = '\0';
    }
    g_lastContentSize = 0;
}

static BOOL ClearPluginDisplayDataLocked(void) {
    BOOL hadDisplayData = g_hasPluginData ||
                          (g_pluginDisplayText && g_pluginDisplayText[0] != L'\0');

    g_hasPluginData = FALSE;
    ClearPluginDisplayTextLocked();
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);

    return hadDisplayData;
}

static void InvalidateLastOutputFileStateLocked(void) {
    ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    g_lastOutputFileSize = 0;
    g_hasLastOutputFileState = FALSE;
}

static void UpdateLastOutputFileStateLocked(const FILETIME* writeTime, ULONGLONG fileSize) {
    if (writeTime) {
        g_lastOutputWriteTime = *writeTime;
    } else {
        ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    }
    g_lastOutputFileSize = fileSize;
    g_hasLastOutputFileState = TRUE;
}

static void FreePluginDataBuffersLocked(void) {
    if (g_pluginDisplayText) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
    }
    g_pluginDisplayTextLen = 0;

    if (g_lastContent) {
        free(g_lastContent);
        g_lastContent = NULL;
    }
    g_lastContentSize = 0;
    g_lastContentCapacity = 0;
}

static void ResetPluginDataStateLocked(void) {
    g_pluginModeActive = FALSE;
    g_hasPluginData = FALSE;
    FreePluginDataBuffersLocked();
    InvalidateLastOutputFileStateLocked();
    ResetPendingNotificationLocked();
}

static BOOL CopyLastOutputFileStateLocked(FILETIME* writeTime, ULONGLONG* fileSize) {
    if (!g_hasLastOutputFileState || !writeTime || !fileSize) {
        return FALSE;
    }

    *writeTime = g_lastOutputWriteTime;
    *fileSize = g_lastOutputFileSize;
    return TRUE;
}

static BOOL GetPluginOutputFileStateW(const wchar_t* filePath,
                                      FILETIME* writeTime,
                                      ULONGLONG* fileSize) {
    if (!filePath || !writeTime || !fileSize) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(filePath, GetFileExInfoStandard, &data)) {
        return FALSE;
    }

    *writeTime = data.ftLastWriteTime;
    *fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    return TRUE;
}

static BOOL ProcessPluginOutputFile(const wchar_t* filePath, BOOL forceRefresh,
                                    FILETIME* lastWriteTime, ULONGLONG* lastFileSize) {
    if (!filePath || !lastWriteTime || !lastFileSize) {
        return FALSE;
    }

    HANDLE hFile = CreateFileW(
        filePath,
        GENERIC_READ,
        PLUGIN_OUTPUT_FILE_SHARE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
            *lastFileSize = 0;

            EnterCriticalSection(&g_dataCS);
            BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
            BOOL displayChanged = ClearPluginDisplayDataLocked();
            BOOL displayTimerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
            ClearLastContentCacheLocked();
            InvalidateLastOutputFileStateLocked();
            LeaveCriticalSection(&g_dataCS);
            if (displayTimerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((displayChanged || displayTimerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
            return displayChanged;
        }
        return FALSE;
    }

    FILETIME currentWriteTime = {0};
    GetFileTime(hFile, NULL, NULL, &currentWriteTime);

    LARGE_INTEGER sizeValue;
    if (!GetFileSizeEx(hFile, &sizeValue) || sizeValue.QuadPart <= 0) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = 0;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, 0);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }

    ULONGLONG fileSize64 = (ULONGLONG)sizeValue.QuadPart;
    if (!forceRefresh &&
        CompareFileTime(&currentWriteTime, lastWriteTime) == 0 &&
        fileSize64 == *lastFileSize) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (fileSize64 > MAX_PLUGIN_OUTPUT_BYTES) {
        LOG_WARNING("PluginData: Skipping output.txt larger than %llu bytes",
                    (ULONGLONG)MAX_PLUGIN_OUTPUT_BYTES);
        *lastWriteTime = currentWriteTime;
        *lastFileSize = fileSize64;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, *lastFileSize);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }

    DWORD fileSize = (DWORD)fileSize64;

    DWORD bytesToRead = fileSize;
    if (bytesToRead > PLUGIN_OUTPUT_READ_LIMIT_BYTES) {
        bytesToRead = PLUGIN_OUTPUT_READ_LIMIT_BYTES;
    }

    char stackContent[PLUGIN_OUTPUT_STACK_BUFFER_BYTES + 1];
    char* heapContent = NULL;
    char* currentContent = stackContent;
    if (bytesToRead > PLUGIN_OUTPUT_STACK_BUFFER_BYTES) {
        heapContent = (char*)malloc((size_t)bytesToRead + 1);
        if (!heapContent) {
            CloseHandle(hFile);
            return FALSE;
        }
        currentContent = heapContent;
    }

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, currentContent, bytesToRead, &bytesRead, NULL) && bytesRead > 0;
    CloseHandle(hFile);

    if (!readOk) {
        free(heapContent);
        return FALSE;
    }

    currentContent[bytesRead] = '\0';
    EnterCriticalSection(&g_dataCS);
    BOOL contentChanged = g_lastContent == NULL ||
                          g_lastContentSize != (size_t)bytesRead + 1 ||
                          memcmp(currentContent, g_lastContent, (size_t)bytesRead + 1) != 0;

    LeaveCriticalSection(&g_dataCS);

    if (!contentChanged) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = fileSize;
        EnterCriticalSection(&g_dataCS);
        UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
        LeaveCriticalSection(&g_dataCS);
    } else {
        BOOL displayChanged = FALSE;
        BOOL timerRecheck = FALSE;
        PluginParseResult parseResult =
            ParseContent(currentContent, bytesRead, &displayChanged, &timerRecheck);
        if (parseResult == PLUGIN_PARSE_OK) {
            *lastWriteTime = currentWriteTime;
            *lastFileSize = fileSize;
            EnterCriticalSection(&g_dataCS);
            UpdateLastContentCache(currentContent, bytesRead);
            UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
            LeaveCriticalSection(&g_dataCS);

            if (timerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
        } else if (parseResult == PLUGIN_PARSE_FAILED) {
            EnterCriticalSection(&g_dataCS);
            BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
            BOOL clearedDisplayChanged = ClearPluginDisplayDataLocked();
            BOOL displayTimerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
            ClearLastContentCacheLocked();
            UpdateLastOutputFileStateLocked(&currentWriteTime, fileSize);
            LeaveCriticalSection(&g_dataCS);
            *lastWriteTime = currentWriteTime;
            *lastFileSize = fileSize;

            if (displayTimerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((clearedDisplayChanged || displayTimerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
        } else {
            contentChanged = FALSE;
        }
    }

    free(heapContent);
    return contentChanged;
}

static DWORD GetChangeDebounceMs(void) {
    DWORD debounceMs = GetPollIntervalMs();
    if (debounceMs < MIN_POLL_INTERVAL_MS) debounceMs = MIN_POLL_INTERVAL_MS;
    if (debounceMs > MAX_CHANGE_DEBOUNCE_MS) debounceMs = MAX_CHANGE_DEBOUNCE_MS;
    return debounceMs;
}

static BOOL RearmChangeNotification(HANDLE* changeHandle) {
    if (!changeHandle || *changeHandle == INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    if (FindNextChangeNotification(*changeHandle)) {
        return TRUE;
    }

    FindCloseChangeNotification(*changeHandle);
    *changeHandle = INVALID_HANDLE_VALUE;
    LOG_WARNING("PluginData: Change notification lost, switching to polling");
    return FALSE;
}

static BOOL DebouncePluginOutputChange(HANDLE* changeHandle) {
    DWORD debounceMs = GetChangeDebounceMs();
    DWORD start = GetTickCount();

    while (GetTickCount() - start < debounceMs) {
        HANDLE waitHandles[3];
        DWORD waitCount = 0;
        DWORD elapsed = GetTickCount() - start;
        DWORD remaining = debounceMs - elapsed;

        waitHandles[waitCount++] = g_hWatchStopEvent;
        waitHandles[waitCount++] = g_hWatchWakeEvent;
        if (changeHandle && *changeHandle != INVALID_HANDLE_VALUE) {
            waitHandles[waitCount++] = *changeHandle;
        }

        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, remaining);
        if (waitResult == WAIT_TIMEOUT) {
            return TRUE;
        }
        if (waitResult == WAIT_OBJECT_0) {
            return FALSE;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            if (g_hWatchWakeEvent) {
                ResetEvent(g_hWatchWakeEvent);
            }
            return TRUE;
        }
        if (changeHandle && *changeHandle != INVALID_HANDLE_VALUE &&
            waitResult == WAIT_OBJECT_0 + 2) {
            RearmChangeNotification(changeHandle);
            continue;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("PluginData: Change debounce wait failed (error=%lu)", GetLastError());
            return TRUE;
        }

        return TRUE;
    }

    return TRUE;
}

/**
 * @brief Background thread to monitor plugin data file
 */
static DWORD WINAPI FileWatcherThread(LPVOID lpParam) {
    (void)lpParam;

    wchar_t filePath[MAX_PATH];
    if (!GetPluginOutputPathW(filePath, MAX_PATH)) {
        LOG_WARNING("PluginData: Failed to get output file path");
        SetWatcherRunning(FALSE);
        return 0;
    }
    
    wchar_t outputDir[MAX_PATH] = {0};
    HANDLE changeHandle = INVALID_HANDLE_VALUE;
    if (GetPluginOutputDirectory(outputDir, MAX_PATH)) {
        changeHandle = FindFirstChangeNotificationW(
            outputDir,
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE
        );
        if (changeHandle == INVALID_HANDLE_VALUE) {
            LOG_WARNING("PluginData: Change notification unavailable, falling back to polling");
        }
    }

    FILETIME lastWriteTime = {0};
    ULONGLONG lastFileSize = 0;
    EnterCriticalSection(&g_dataCS);
    CopyLastOutputFileStateLocked(&lastWriteTime, &lastFileSize);
    LeaveCriticalSection(&g_dataCS);

    while (IsWatcherRunning()) {
        BOOL forceRefresh = InterlockedExchange(&g_forceNextUpdate, FALSE) != FALSE;
        if (forceRefresh) {
            ZeroMemory(&lastWriteTime, sizeof(lastWriteTime));
            lastFileSize = 0;
        }

        ProcessPluginOutputFile(filePath, forceRefresh, &lastWriteTime, &lastFileSize);

        HANDLE waitHandles[3];
        DWORD waitCount = 0;
        waitHandles[waitCount++] = g_hWatchStopEvent;
        waitHandles[waitCount++] = g_hWatchWakeEvent;
        if (changeHandle != INVALID_HANDLE_VALUE) {
            waitHandles[waitCount++] = changeHandle;
        }

        DWORD waitTimeout = (changeHandle != INVALID_HANDLE_VALUE) ? INFINITE : GetPollIntervalMs();
        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, waitTimeout);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            if (g_hWatchWakeEvent) {
                ResetEvent(g_hWatchWakeEvent);
            }
            continue;
        }
        if (changeHandle != INVALID_HANDLE_VALUE && waitResult == WAIT_OBJECT_0 + 2) {
            RearmChangeNotification(&changeHandle);
            if (!DebouncePluginOutputChange(&changeHandle)) {
                break;
            }
            continue;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("PluginData: Watch wait failed (error=%lu), switching to polling", GetLastError());
            if (changeHandle != INVALID_HANDLE_VALUE) {
                FindCloseChangeNotification(changeHandle);
                changeHandle = INVALID_HANDLE_VALUE;
            }
            continue;
        }
    }

    if (changeHandle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(changeHandle);
    }

    SetWatcherRunning(FALSE);
    
    return 0;
}

static BOOL EnsureWatcherEvents(void) {
    if (!g_hWatchStopEvent) {
        g_hWatchStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_hWatchStopEvent) return FALSE;
    }
    if (!g_hWatchWakeEvent) {
        g_hWatchWakeEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_hWatchWakeEvent) {
            if (!g_hWatchThread && g_hWatchStopEvent) {
                CloseHandle(g_hWatchStopEvent);
                g_hWatchStopEvent = NULL;
            }
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL StartWatcherThreadIfNeeded(void) {
    EnterCriticalSection(&g_watchCS);

    if (!WaitForWatcherStopGateLocked(PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    CleanupCompletedWatcherThreadLocked();
    if (g_hWatchThread) {
        if (!IsWatcherRunning()) {
            LOG_WARNING("PluginData: Watcher is still retiring; start deferred");
            LeaveCriticalSection(&g_watchCS);
            return FALSE;
        }
        LeaveCriticalSection(&g_watchCS);
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsWatcherStartFailureCoolingDown(now)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    if (!EnsureWatcherEvents()) {
        LOG_ERROR("PluginData: Failed to create watcher events");
        MarkWatcherStartFailure(now);
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    ResetEvent(g_hWatchStopEvent);
    ResetEvent(g_hWatchWakeEvent);
    SetWatcherRunning(TRUE);
    g_hWatchThread = CreateThread(NULL, 0, FileWatcherThread, NULL, 0, NULL);
    if (!g_hWatchThread) {
        SetWatcherRunning(FALSE);
        LOG_ERROR("PluginData: Failed to start watcher thread");
        MarkWatcherStartFailure(now);
        CloseWatcherEventsIfIdleLocked();
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    g_watchStartFailureCooldownUntil = 0;
    LeaveCriticalSection(&g_watchCS);
    return TRUE;
}

static BOOL StopWatcherThreadIfIdle(DWORD waitMs) {
    HANDLE hThread = NULL;
    BOOL stopped = TRUE;

    EnterCriticalSection(&g_watchCS);

    if (!WaitForWatcherStopGateLocked(PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    CleanupCompletedWatcherThreadLocked();
    if (!g_hWatchThread) {
        LeaveCriticalSection(&g_watchCS);
        return TRUE;
    }

    hThread = g_hWatchThread;
    g_hWatchThread = NULL;
    g_watchStopInProgress = TRUE;
    SetWatcherRunning(FALSE);
    if (g_hWatchStopEvent) {
        SetEvent(g_hWatchStopEvent);
    }
    WakeWatcherThreadLocked();
    LeaveCriticalSection(&g_watchCS);

    DWORD waitResult = WaitForSingleObject(hThread, waitMs);
    DWORD waitError = (waitResult == WAIT_FAILED) ? GetLastError() : ERROR_SUCCESS;
    EnterCriticalSection(&g_watchCS);
    if (waitResult == WAIT_OBJECT_0 || WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(hThread);
        stopped = TRUE;
    } else {
        g_hWatchThread = hThread;
        LOG_WARNING("PluginData: Watcher stop wait returned %lu (error=%lu)",
                    waitResult, waitError);
        stopped = FALSE;
    }
    g_watchStopInProgress = FALSE;
    WakeAllConditionVariable(&g_watchStopCompleted);
    LeaveCriticalSection(&g_watchCS);
    return stopped;
}

static void EnsurePluginDataLocksInitialized(BOOL* initializedNow) {
    if (initializedNow) {
        *initializedNow = FALSE;
    }
    if (g_pluginDataLocksInitialized) {
        return;
    }

    InitializeCriticalSection(&g_dataCS);
    InitializeCriticalSection(&g_watchCS);
    g_pluginDataLocksInitialized = TRUE;
    if (initializedNow) {
        *initializedNow = TRUE;
    }
}

static void DeletePluginDataLocks(void) {
    if (!g_pluginDataLocksInitialized) {
        return;
    }

    DeleteCriticalSection(&g_watchCS);
    DeleteCriticalSection(&g_dataCS);
    g_pluginDataLocksInitialized = FALSE;
}

static BOOL HasRetainedWatcherThread(void) {
    BOOL retained = FALSE;
    if (!g_pluginDataLocksInitialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_watchCS);
    CleanupCompletedWatcherThreadLocked();
    retained = (g_hWatchThread != NULL || g_watchStopInProgress);
    LeaveCriticalSection(&g_watchCS);
    return retained;
}

void PluginData_Init(HWND hwnd) {
    AcquireSRWLockExclusive(&g_pluginDataLifecycleLock);
    if (g_pluginDataInitialized) {
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    BOOL locksInitializedNow = FALSE;
    EnsurePluginDataLocksInitialized(&locksInitializedNow);

    if (g_pluginDataResourcesRetained && HasRetainedWatcherThread()) {
        LOG_WARNING("PluginData: Init deferred because a previous watcher thread is still retiring");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    if (!PluginExit_Init(hwnd, &g_dataCS)) {
        LOG_WARNING("PluginData: Init deferred because plugin exit resources are still retiring");
        if (locksInitializedNow && !g_pluginDataResourcesRetained) {
            DeletePluginDataLocks();
        }
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    EnterCriticalSection(&g_watchCS);
    g_watchStopInProgress = FALSE;
    LeaveCriticalSection(&g_watchCS);

    g_hNotifyWnd = hwnd;
    g_lastPluginDataRedrawTick = 0;
    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
    InterlockedExchange(&g_forceNextUpdate, FALSE);
    g_watchStartFailureCooldownUntil = 0;
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);
    SetWatcherRunning(FALSE);

    EnterCriticalSection(&g_dataCS);
    ResetPluginDataStateLocked();
    LeaveCriticalSection(&g_dataCS);

    /* Ensure output directory exists (don't clear user's output.txt) */
    wchar_t outputPath[MAX_PATH];
    if (GetPluginOutputPathW(outputPath, MAX_PATH)) {
        EnsureOutputDirExistsW(outputPath);
    }

    g_pluginDataResourcesRetained = FALSE;
    g_pluginDataInitialized = TRUE;
    ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
}

void PluginData_Shutdown(void) {
    AcquireSRWLockExclusive(&g_pluginDataLifecycleLock);
    if (!g_pluginDataInitialized && !g_pluginDataResourcesRetained) {
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    g_pluginDataInitialized = FALSE;
    
    /* Stop watcher thread */
    BOOL watcherStopped = StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_SHUTDOWN_WAIT_MS);
    StopPluginDataRedrawTimer(g_hNotifyWnd);
    g_hNotifyWnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
    if (!watcherStopped) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Watcher resources retained because the watcher did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    EnterCriticalSection(&g_watchCS);
    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
    LeaveCriticalSection(&g_watchCS);
    
    /* Shutdown exit subsystem */
    if (!PluginExit_Shutdown()) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Exit countdown resources retained because countdown did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    
    /* Free memory */
    EnterCriticalSection(&g_dataCS);
    ResetPluginDataStateLocked();
    LeaveCriticalSection(&g_dataCS);

    DeletePluginDataLocks();
    g_pluginDataResourcesRetained = FALSE;
    ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
}

BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;
    if (!PluginData_BeginUse()) return FALSE;

    BOOL hasData = FALSE;
    EnterCriticalSection(&g_dataCS);
    
    if (g_pluginModeActive) {
        if (g_hasPluginData && g_pluginDisplayText && g_pluginDisplayText[0] != L'\0') {
            /* Has actual data */
            wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        } else {
            /* Plugin mode active but no data yet - show loading */
            wcsncpy(buffer, L"Loading...", maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        }
    }

    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return hasData;
}

void PluginData_Clear(void) {
    if (!PluginData_BeginUse()) return;

    /* Cancel any pending exit countdown */
    PluginExit_Cancel();
    
    /* Reset poll interval to default */
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);
    
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = FALSE;  // Deactivate plugin mode
    g_hasPluginData = FALSE;
    ClearPluginDisplayTextLocked();
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();
    /* Clear any pending notification to prevent stale notifications */
    ResetPendingNotificationLocked();
    LeaveCriticalSection(&g_dataCS);
    if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
        LOG_WARNING("PluginData: Watcher stop deferred while clearing plugin data");
    }
    PluginData_EndUse();
}

void PluginData_SetText(const wchar_t* text) {
    if (!text) return;
    if (!PluginData_BeginUse()) return;

    EnterCriticalSection(&g_dataCS);

    size_t rawTextLen = wcslen(text);
    if (rawTextLen > SIZE_MAX / sizeof(wchar_t) - 1) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }
    size_t textLen = rawTextLen + 1;
    if (!EnsurePluginDisplayTextCapacityLocked(textLen)) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }

    wcscpy_s(g_pluginDisplayText, g_pluginDisplayTextLen, text);
    g_hasPluginData = TRUE;
    g_pluginModeActive = TRUE;
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();

    LeaveCriticalSection(&g_dataCS);
    
    /* Clear the plugin data file to prevent showing stale content from previous plugin.
     * If clearing fails, remember the old file state as the baseline so the watcher
     * does not immediately overwrite the loading text with stale output.
     */
    wchar_t filePath[MAX_PATH];
    BOOL outputStateCaptured = FALSE;
    if (GetPluginOutputPathW(filePath, MAX_PATH)) {
        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, PLUGIN_OUTPUT_FILE_SHARE,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        } else {
            LOG_WARNING("PluginData: Failed to clear output file before setting text (error=%lu)",
                        GetLastError());
        }

        FILETIME writeTime = {0};
        ULONGLONG fileSize = 0;
        if (GetPluginOutputFileStateW(filePath, &writeTime, &fileSize)) {
            EnterCriticalSection(&g_dataCS);
            UpdateLastOutputFileStateLocked(&writeTime, fileSize);
            LeaveCriticalSection(&g_dataCS);
            outputStateCaptured = TRUE;
        }
    }
    
    InterlockedExchange(&g_forceNextUpdate, outputStateCaptured ? FALSE : TRUE);
    StartWatcherThreadIfNeeded();
    WakeWatcherThread();
    PluginData_EndUse();
}

void PluginData_SetStatusText(const wchar_t* text) {
    if (!text) return;
    if (!PluginData_BeginUse()) return;

    EnterCriticalSection(&g_dataCS);

    size_t rawTextLen = wcslen(text);
    if (rawTextLen > SIZE_MAX / sizeof(wchar_t) - 1) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }
    size_t textLen = rawTextLen + 1;
    if (!EnsurePluginDisplayTextCapacityLocked(textLen)) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }

    wcscpy_s(g_pluginDisplayText, g_pluginDisplayTextLen, text);
    g_hasPluginData = TRUE;
    g_pluginModeActive = TRUE;
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();

    LeaveCriticalSection(&g_dataCS);
    if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
        LOG_WARNING("PluginData: Watcher stop deferred while setting status text");
    }
    PluginData_EndUse();
}

void PluginData_SetActive(BOOL active) {
    if (!PluginData_BeginUse()) return;
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = active;
    if (active) {
        InterlockedExchange(&g_forceNextUpdate, FALSE);
        ClearLastContentCacheLocked();
        InvalidateLastOutputFileStateLocked();
    } else {
        // When deactivating, also clear any stale data
        g_hasPluginData = FALSE;
        ClearPluginDisplayTextLocked();
        // Clear any pending notification
        ResetPendingNotificationLocked();
    }
    LeaveCriticalSection(&g_dataCS);

    if (active) {
        /* If activating, immediately read the file content when there is no
         * current baseline.  Otherwise let the normal change check decide,
         * which preserves freshly-set loading text when output.txt is stale.
         */
        wchar_t filePath[MAX_PATH];
        if (GetPluginOutputPathW(filePath, MAX_PATH)) {
            FILETIME currentWriteTime = {0};
            ULONGLONG currentFileSize = 0;
            BOOL hasBaseline = FALSE;
            EnterCriticalSection(&g_dataCS);
            hasBaseline = CopyLastOutputFileStateLocked(&currentWriteTime, &currentFileSize);
            LeaveCriticalSection(&g_dataCS);
            ProcessPluginOutputFile(filePath, !hasBaseline, &currentWriteTime, &currentFileSize);
        }
        StartWatcherThreadIfNeeded();
    } else {
        PluginExit_Cancel();
        if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
            LOG_WARNING("PluginData: Watcher stop deferred while deactivating plugin data");
        }
    }
    PluginData_EndUse();
}

BOOL PluginData_IsActive(void) {
    if (!PluginData_BeginUse()) return FALSE;
    BOOL active;
    EnterCriticalSection(&g_dataCS);
    active = g_pluginModeActive;
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return active;
}

BOOL PluginData_HasCatimeTag(void) {
    if (!PluginData_BeginUse()) return FALSE;
    BOOL hasTag = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (g_pluginModeActive && g_hasPluginData && g_pluginDisplayText) {
        // Check for <catime> and </catime> tags
        const wchar_t* start = wcsstr(g_pluginDisplayText, L"<catime>");
        const wchar_t* end = wcsstr(g_pluginDisplayText, L"</catime>");
        if (start && end && end > start) {
            hasTag = TRUE;
        }
    }
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return hasTag;
}

void PluginData_ProcessPendingNotification(HWND hwnd) {
    if (!PluginData_BeginUse()) return;

    /* Copy pending notification data under lock, then process outside lock */
    PendingNotification localNotify;

    EnterCriticalSection(&g_dataCS);
    if (!g_pendingNotify.pending) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }
    
    /* Copy to local and clear pending flag */
    memcpy(&localNotify, &g_pendingNotify, sizeof(PendingNotification));
    g_pendingNotify.pending = FALSE;
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();

    int notifyType = localNotify.type;
    int customTimeout = localNotify.timeout;

    /* Show notification based on type */
    if (notifyType == -1) {
        /* Use default configured type */
        if (customTimeout > 0 &&
            g_AppConfig.notification.display.type == NOTIFICATION_TYPE_CATIME) {
            ShowToastNotificationWithTimeout(hwnd, localNotify.message, customTimeout);
        } else {
            ShowNotification(hwnd, localNotify.message);
        }
    } else if (notifyType == NOTIFICATION_TYPE_CATIME) {
        if (customTimeout > 0) {
            ShowToastNotificationWithTimeout(hwnd, localNotify.message, customTimeout);
        } else {
            ShowToastNotification(hwnd, localNotify.message);
        }
    } else if (notifyType == NOTIFICATION_TYPE_SYSTEM_MODAL) {
        ShowModalNotification(hwnd, localNotify.message);
    } else if (notifyType == NOTIFICATION_TYPE_OS) {
        /* Convert to UTF-8 for tray notification */
        char msgUtf8[2048] = {0};
        if (WideCharToMultiByte(CP_UTF8, 0, localNotify.message, -1, msgUtf8, sizeof(msgUtf8), NULL, NULL) <= 0) {
            LOG_WARNING("PluginData: Failed to convert OS notification message to UTF-8");
            return;
        }
        extern void ShowTrayNotification(HWND hwnd, const char* message);
        ShowTrayNotification(hwnd, msgUtf8);
    }
}

