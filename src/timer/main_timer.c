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

/**
 * @brief Multimedia timer callback (worker thread)
 * Posts message to main thread for rendering
 */
static void CALLBACK MainTimerCallback(UINT uTimerID, UINT uMsg,
                                       DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    
    if (g_mainHwnd && IsWindow(g_mainHwnd)) {
        PostMessage(g_mainHwnd, CLOCK_WM_MAIN_TIMER_TICK, 0, 0);
    }
}

BOOL MainTimer_Init(HWND hwnd, UINT intervalMs) {
    if (!hwnd) return FALSE;
    
    /* Cleanup any existing timer */
    MainTimer_Cleanup();
    
    g_mainHwnd = hwnd;
    g_timerInterval = intervalMs > 0 ? intervalMs : 20;
    
    /* Set system timer resolution to 1ms for precision */
    MMRESULT res = timeBeginPeriod(1);
    if (res != TIMERR_NOERROR) {
        /* Try 2ms as fallback */
        res = timeBeginPeriod(2);
        if (res != TIMERR_NOERROR) {
            /* Fall back to standard SetTimer */
            SetTimer(hwnd, TIMER_ID_MAIN, g_timerInterval, NULL);
            g_highPrecisionActive = FALSE;
            return TRUE;
        }
    }
    
    /* Create multimedia timer */
    g_mainTimerId = timeSetEvent(
        g_timerInterval,
        1,  /* Resolution: 1ms */
        MainTimerCallback,
        0,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );
    
    if (g_mainTimerId == 0) {
        /* Fallback to SetTimer */
        timeEndPeriod(1);
        SetTimer(hwnd, TIMER_ID_MAIN, g_timerInterval, NULL);
        g_highPrecisionActive = FALSE;
        return TRUE;
    }
    
    g_highPrecisionActive = TRUE;
    return TRUE;
}

void MainTimer_SetInterval(UINT intervalMs) {
    if (intervalMs == g_timerInterval) return;
    
    g_timerInterval = intervalMs > 0 ? intervalMs : 20;
    
    if (g_highPrecisionActive && g_mainTimerId != 0) {
        /* Kill old timer and create new one with updated interval */
        timeKillEvent(g_mainTimerId);
        
        g_mainTimerId = timeSetEvent(
            g_timerInterval,
            1,
            MainTimerCallback,
            0,
            TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
        );
        
        if (g_mainTimerId == 0) {
            /* Fallback if recreation fails */
            g_highPrecisionActive = FALSE;
            if (g_mainHwnd) {
                SetTimer(g_mainHwnd, TIMER_ID_MAIN, g_timerInterval, NULL);
            }
        }
    } else if (g_mainHwnd) {
        /* Using SetTimer, just update it */
        KillTimer(g_mainHwnd, TIMER_ID_MAIN);
        SetTimer(g_mainHwnd, TIMER_ID_MAIN, g_timerInterval, NULL);
    }
}

void MainTimer_Cleanup(void) {
    if (g_mainTimerId != 0) {
        timeKillEvent(g_mainTimerId);
        g_mainTimerId = 0;
    }
    
    if (g_highPrecisionActive) {
        timeEndPeriod(1);
        g_highPrecisionActive = FALSE;
    }
    
    if (g_mainHwnd) {
        KillTimer(g_mainHwnd, TIMER_ID_MAIN);
    }
    
    g_mainHwnd = NULL;
}

BOOL MainTimer_IsHighPrecision(void) {
    return g_highPrecisionActive;
}
