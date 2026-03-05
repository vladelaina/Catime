/**
 * @file main_timer.c
 * @brief High-precision main window timer implementation
 * 
 * Uses multimedia timer (timeSetEvent) for precise timing.
 * Posts WM message to main thread for thread-safe window updates.
 */

#include "timer/main_timer.h"
#include "../../resource/resource.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

/* Timer state */
static MMRESULT g_mainTimerId = 0;
static HWND g_mainHwnd = NULL;
static UINT g_timerInterval = 20;
static BOOL g_highPrecisionActive = FALSE;
static UINT g_timerResolutionMs = 0;
static volatile LONG g_tickMessagePending = 0;

/**
 * @brief Multimedia timer callback (worker thread)
 * Posts message to main thread for rendering
 */
static void CALLBACK MainTimerCallback(UINT uTimerID, UINT uMsg,
                                       DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    
    if (g_mainHwnd && IsWindow(g_mainHwnd)) {
        /* Coalesce pending tick messages to avoid queue backlog under UI load. */
        if (InterlockedCompareExchange(&g_tickMessagePending, 1, 0) == 0) {
            if (!PostMessage(g_mainHwnd, CLOCK_WM_MAIN_TIMER_TICK, 0, 0)) {
                InterlockedExchange(&g_tickMessagePending, 0);
            }
        }
    }
}

static UINT NormalizeInterval(UINT intervalMs) {
    return intervalMs > 0 ? intervalMs : 20;
}

static BOOL StartSetTimerFallback(void) {
    if (!g_mainHwnd) return FALSE;
    KillTimer(g_mainHwnd, TIMER_ID_MAIN);
    return SetTimer(g_mainHwnd, TIMER_ID_MAIN, g_timerInterval, NULL) != 0;
}

static BOOL StartMultimediaTimer(void) {
    g_mainTimerId = timeSetEvent(
        g_timerInterval,
        1,
        MainTimerCallback,
        0,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );
    return g_mainTimerId != 0;
}

BOOL MainTimer_Init(HWND hwnd, UINT intervalMs) {
    if (!hwnd) return FALSE;
    
    /* Cleanup any existing timer */
    MainTimer_Cleanup();
    
    g_mainHwnd = hwnd;
    g_timerInterval = NormalizeInterval(intervalMs);
    
    /* Set system timer resolution to 1ms for precision */
    MMRESULT res = timeBeginPeriod(1);
    if (res != TIMERR_NOERROR) {
        /* Try 2ms as fallback */
        res = timeBeginPeriod(2);
        if (res != TIMERR_NOERROR) {
            /* Fall back to standard SetTimer */
            g_highPrecisionActive = FALSE;
            return StartSetTimerFallback();
        }
        g_timerResolutionMs = 2;
    } else {
        g_timerResolutionMs = 1;
    }
    
    if (!StartMultimediaTimer()) {
        /* Fallback to SetTimer */
        if (g_timerResolutionMs > 0) {
            timeEndPeriod(g_timerResolutionMs);
            g_timerResolutionMs = 0;
        }
        g_highPrecisionActive = FALSE;
        return StartSetTimerFallback();
    }
    
    g_highPrecisionActive = TRUE;
    return TRUE;
}

BOOL MainTimer_Start(HWND hwnd, UINT intervalMs) {
    if (!hwnd) return FALSE;

    if (!g_mainHwnd || g_mainHwnd != hwnd) {
        return MainTimer_Init(hwnd, intervalMs);
    }

    UINT normalized = NormalizeInterval(intervalMs);

    if (g_highPrecisionActive) {
        if (g_mainTimerId == 0) {
            g_timerInterval = normalized;
            if (!StartMultimediaTimer()) {
                g_highPrecisionActive = FALSE;
                return StartSetTimerFallback();
            }
        } else if (normalized != g_timerInterval) {
            g_timerInterval = normalized;
            timeKillEvent(g_mainTimerId);
            if (!StartMultimediaTimer()) {
                g_highPrecisionActive = FALSE;
                return StartSetTimerFallback();
            }
        }
        KillTimer(g_mainHwnd, TIMER_ID_MAIN);
        return TRUE;
    }

    g_timerInterval = normalized;
    return StartSetTimerFallback();
}

void MainTimer_Stop(void) {
    if (g_mainTimerId != 0) {
        timeKillEvent(g_mainTimerId);
        g_mainTimerId = 0;
    }

    if (g_mainHwnd) {
        KillTimer(g_mainHwnd, TIMER_ID_MAIN);
    }

    InterlockedExchange(&g_tickMessagePending, 0);
}

void MainTimer_NotifyTickHandled(void) {
    InterlockedExchange(&g_tickMessagePending, 0);
}

void MainTimer_SetInterval(UINT intervalMs) {
    UINT normalized = NormalizeInterval(intervalMs);
    if (normalized == g_timerInterval) {
        if (g_mainHwnd) {
            MainTimer_Start(g_mainHwnd, g_timerInterval);
        }
        return;
    }

    g_timerInterval = normalized;

    if (g_highPrecisionActive && g_mainTimerId != 0) {
        /* Kill old timer and create new one with updated interval */
        timeKillEvent(g_mainTimerId);

        if (!StartMultimediaTimer()) {
            /* Fallback if recreation fails */
            g_highPrecisionActive = FALSE;
            StartSetTimerFallback();
        }
    } else if (g_mainHwnd) {
        MainTimer_Start(g_mainHwnd, g_timerInterval);
    }
}

void MainTimer_Cleanup(void) {
    MainTimer_Stop();

    if (g_timerResolutionMs > 0) {
        timeEndPeriod(g_timerResolutionMs);
        g_timerResolutionMs = 0;
    }

    g_highPrecisionActive = FALSE;
    g_mainHwnd = NULL;
}

BOOL MainTimer_IsHighPrecision(void) {
    return g_highPrecisionActive;
}
