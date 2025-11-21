/**
 * @file resource_cache.c
 * @brief Resource cache coordinator implementation
 */

#include "cache/font_cache.h"
#include "cache/animation_cache.h"
#include "cache/resource_cache.h"
#include "cache/resource_watcher.h"
#include "log.h"
#include <stdio.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static HANDLE g_backgroundThread = NULL;
static volatile BOOL g_shutdownRequested = FALSE;
static volatile BOOL g_backgroundScanComplete = FALSE;
static volatile LONG g_refreshInProgress = 0;
static volatile BOOL g_initialized = FALSE;

/* ============================================================================
 * Background Thread
 * ============================================================================ */

static DWORD WINAPI BackgroundScanThread(LPVOID lpParam) {
    (void)lpParam;
    
    WriteLog(LOG_LEVEL_INFO, "Background resource scan started");
    
    // Small delay to let main window initialize
    Sleep(100);
    
    // Check if shutdown requested
    if (g_shutdownRequested) {
        WriteLog(LOG_LEVEL_INFO, "Background scan aborted (shutdown requested)");
        InterlockedExchange(&g_refreshInProgress, 0);
        return 0;
    }
    
    // Scan fonts
    if (!g_shutdownRequested) {
        FontCache_Scan();
    }
    
    // Check again before scanning animations
    if (g_shutdownRequested) {
        WriteLog(LOG_LEVEL_INFO, "Background scan aborted after fonts (shutdown requested)");
        InterlockedExchange(&g_refreshInProgress, 0);
        return 0;
    }
    
    // Scan animations
    if (!g_shutdownRequested) {
        AnimationCache_Scan();
    }
    
    g_backgroundScanComplete = TRUE;
    InterlockedExchange(&g_refreshInProgress, 0);
    
    WriteLog(LOG_LEVEL_INFO, "Background resource scan complete");
    
    return 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

BOOL ResourceCache_Initialize(BOOL startBackgroundScan) {
    if (g_initialized) {
        return TRUE;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Initializing resource cache system");
    
    // Initialize sub-systems
    if (!FontCache_Initialize()) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to initialize font cache");
        return FALSE;
    }
    
    if (!AnimationCache_Initialize()) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to initialize animation cache");
        FontCache_Shutdown();
        return FALSE;
    }
    
    g_shutdownRequested = FALSE;
    g_backgroundScanComplete = FALSE;
    InterlockedExchange(&g_refreshInProgress, 0);
    g_initialized = TRUE;
    
    if (startBackgroundScan) {
        InterlockedExchange(&g_refreshInProgress, 1);
        g_backgroundThread = CreateThread(
            NULL, 0, BackgroundScanThread, NULL, 0, NULL);
        
        if (g_backgroundThread == NULL) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create background scan thread");
            InterlockedExchange(&g_refreshInProgress, 0);
            return FALSE;
        }
        
        SetThreadPriority(g_backgroundThread, THREAD_PRIORITY_BELOW_NORMAL);
        
        // Start file system watcher (will delay internally to let initial scan complete)
        if (!ResourceWatcher_Start()) {
            WriteLog(LOG_LEVEL_WARNING, "File system watcher failed to start, auto-refresh disabled");
            // Continue anyway, this is not critical
        }
    }
    
    WriteLog(LOG_LEVEL_INFO, "Resource cache initialized (background=%d)", startBackgroundScan);
    return TRUE;
}

void ResourceCache_Shutdown(void) {
    if (!g_initialized) {
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Shutting down resource cache system");
    
    g_shutdownRequested = TRUE;
    
    // Stop file system watcher first
    ResourceWatcher_Stop();
    
    // Wait for background thread
    if (g_backgroundThread != NULL) {
        DWORD waitResult = WaitForSingleObject(g_backgroundThread, 5000);
        
        if (waitResult == WAIT_TIMEOUT) {
            WriteLog(LOG_LEVEL_ERROR, "Background thread did not exit gracefully within 5 seconds");
            WriteLog(LOG_LEVEL_ERROR, "Abandoning thread and leaking memory to avoid deadlock");
            CloseHandle(g_backgroundThread);
            g_backgroundThread = NULL;
            g_initialized = FALSE;
            return;
        }
        
        CloseHandle(g_backgroundThread);
        g_backgroundThread = NULL;
    }
    
    // Shutdown sub-systems
    FontCache_Shutdown();
    AnimationCache_Shutdown();
    
    g_initialized = FALSE;
    
    WriteLog(LOG_LEVEL_INFO, "Resource cache shutdown complete");
}

BOOL ResourceCache_IsReady(void) {
    return g_initialized;
}

BOOL ResourceCache_IsBackgroundScanComplete(void) {
    return g_backgroundScanComplete;
}

void ResourceCache_RequestRefresh(void) {
    if (!g_initialized) return;
    
    // Atomically check and set refresh flag
    if (InterlockedCompareExchange(&g_refreshInProgress, 1, 0) != 0) {
        WriteLog(LOG_LEVEL_INFO, "Resource cache refresh already in progress, skipping");
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Resource cache refresh requested (async)");
    
    // Invalidate caches
    FontCache_Invalidate();
    AnimationCache_Invalidate();
    
    // Start background rescan
    HANDLE hThread = CreateThread(NULL, 0, BackgroundScanThread, NULL, 0, NULL);
    if (hThread) {
        SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(hThread);
    } else {
        InterlockedExchange(&g_refreshInProgress, 0);
        WriteLog(LOG_LEVEL_ERROR, "Failed to create refresh thread");
    }
}

void ResourceCache_InvalidateAll(void) {
    if (!g_initialized) return;
    
    WriteLog(LOG_LEVEL_INFO, "Invalidating all resource caches");
    
    FontCache_Invalidate();
    AnimationCache_Invalidate();
}

void ResourceCache_GetStatistics(int* outFontCount, int* outAnimCount,
                                  time_t* outFontScanTime, time_t* outAnimScanTime) {
    if (outFontCount || outFontScanTime) {
        FontCache_GetStatistics(outFontCount, outFontScanTime);
    }
    
    if (outAnimCount || outAnimScanTime) {
        AnimationCache_GetStatistics(outAnimCount, outAnimScanTime);
    }
}

BOOL ResourceCache_IsWatcherActive(void) {
    return ResourceWatcher_IsRunning();
}
