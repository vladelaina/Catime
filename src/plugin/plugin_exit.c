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

/* ============================================================================
 * State
 * ============================================================================ */

static volatile BOOL g_exitInProgress = FALSE;
static HANDLE g_exitThread = NULL;

/* Template for countdown display */
static wchar_t* g_exitPrefix = NULL;
static wchar_t* g_exitSuffix = NULL;

/* Shared resources from plugin_data */
static HWND g_notifyWnd = NULL;
static CRITICAL_SECTION* g_dataCS = NULL;

/* External: display text buffer from plugin_data */
extern wchar_t* g_pluginDisplayText;
extern size_t g_pluginDisplayTextLen;
extern BOOL g_hasPluginData;

/* ============================================================================
 * Exit Countdown Thread
 * ============================================================================ */

static DWORD WINAPI ExitCountdownThread(LPVOID lpParam) {
    int seconds = (int)(intptr_t)lpParam;
    
    LOG_INFO("PluginExit: Countdown started (%d seconds)", seconds);
    
    while (seconds > 0 && g_exitInProgress) {
        /* Build display: prefix + number + suffix */
        wchar_t countdownNum[16];
        _snwprintf_s(countdownNum, 16, _TRUNCATE, L"%d", seconds);
        
        if (g_dataCS) {
            EnterCriticalSection(g_dataCS);
            
            size_t prefixLen = g_exitPrefix ? wcslen(g_exitPrefix) : 0;
            size_t suffixLen = g_exitSuffix ? wcslen(g_exitSuffix) : 0;
            size_t numLen = wcslen(countdownNum);
            size_t totalLen = prefixLen + numLen + suffixLen + 1;
            
            if (g_pluginDisplayText == NULL || g_pluginDisplayTextLen < totalLen) {
                wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, totalLen * sizeof(wchar_t));
                if (newBuf) {
                    g_pluginDisplayText = newBuf;
                    g_pluginDisplayTextLen = totalLen;
                }
            }
            
            if (g_pluginDisplayText) {
                g_pluginDisplayText[0] = L'\0';
                if (g_exitPrefix) wcscat(g_pluginDisplayText, g_exitPrefix);
                wcscat(g_pluginDisplayText, countdownNum);
                if (g_exitSuffix) wcscat(g_pluginDisplayText, g_exitSuffix);
                g_hasPluginData = TRUE;
            }
            
            LeaveCriticalSection(g_dataCS);
        }
        
        if (g_notifyWnd) {
            InvalidateRect(g_notifyWnd, NULL, FALSE);
        }
        
        Sleep(1000);
        seconds--;
    }
    
    if (g_exitInProgress) {
        LOG_INFO("PluginExit: Countdown complete, requesting exit");
        if (g_notifyWnd) {
            PostMessage(g_notifyWnd, CLOCK_WM_PLUGIN_EXIT, 0, 0);
        }
    }
    
    g_exitInProgress = FALSE;
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void PluginExit_Init(HWND hwnd, CRITICAL_SECTION* dataCS) {
    g_notifyWnd = hwnd;
    g_dataCS = dataCS;
    g_exitInProgress = FALSE;
    g_exitThread = NULL;
    g_exitPrefix = NULL;
    g_exitSuffix = NULL;
}

void PluginExit_Shutdown(void) {
    PluginExit_Cancel();
    g_notifyWnd = NULL;
    g_dataCS = NULL;
}

BOOL PluginExit_IsInProgress(void) {
    return g_exitInProgress;
}

BOOL PluginExit_ParseTag(wchar_t* text, int* textLen, size_t maxLen) {
    if (!text || !textLen) return FALSE;
    
    /* Cancel any existing countdown first to avoid race condition */
    if (g_exitInProgress) {
        PluginExit_Cancel();
    }
    
    wchar_t* start = wcsstr(text, L"<exit>");
    wchar_t* end = wcsstr(text, L"</exit>");
    
    if (!start || !end || end <= start) {
        return FALSE;
    }
    
    /* Parse countdown seconds (default 3) */
    int seconds = 3;
    wchar_t* numStart = start + 6;
    BOOL validNumber = TRUE;
    
    if (numStart < end) {
        /* Skip whitespace */
        while (numStart < end && (*numStart == L' ' || *numStart == L'\t')) {
            numStart++;
        }
        
        if (numStart < end) {
            /* Validate numeric content */
            wchar_t* p = numStart;
            while (p < end && *p != L' ' && *p != L'\t') {
                if (*p < L'0' || *p > L'9') {
                    validNumber = FALSE;
                    break;
                }
                p++;
            }
            
            if (validNumber) {
                int parsed = _wtoi(numStart);
                if (parsed > 0) {
                    seconds = parsed;
                } else {
                    validNumber = FALSE;
                }
            }
        }
    }
    
    if (!validNumber) {
        return FALSE;
    }
    
    /* Save prefix (text before <exit>) */
    size_t prefixLen = start - text;
    if (g_exitPrefix) {
        free(g_exitPrefix);
        g_exitPrefix = NULL;
    }
    if (prefixLen > 0) {
        g_exitPrefix = (wchar_t*)malloc((prefixLen + 1) * sizeof(wchar_t));
        if (g_exitPrefix) {
            wcsncpy(g_exitPrefix, text, prefixLen);
            g_exitPrefix[prefixLen] = L'\0';
        } else {
            LOG_WARNING("PluginExit: Failed to allocate prefix buffer (%zu bytes)", (prefixLen + 1) * sizeof(wchar_t));
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
        g_exitSuffix = (wchar_t*)malloc((suffixLen + 1) * sizeof(wchar_t));
        if (g_exitSuffix) {
            wcsncpy(g_exitSuffix, suffixStart, suffixLen);
            g_exitSuffix[suffixLen] = L'\0';
        } else {
            LOG_WARNING("PluginExit: Failed to allocate suffix buffer (%zu bytes)", (suffixLen + 1) * sizeof(wchar_t));
        }
    }
    
    /* Replace <exit>N</exit> with just N */
    wchar_t countdownNum[16];
    _snwprintf_s(countdownNum, 16, _TRUNCATE, L"%d", seconds);
    
    size_t suffixLen = wcslen(suffixStart);
    size_t numLen = wcslen(countdownNum);
    size_t newLen = prefixLen + numLen + suffixLen + 1;
    
    if (newLen <= maxLen) {
        memmove(start + numLen, suffixStart, (suffixLen + 1) * sizeof(wchar_t));
        memcpy(start, countdownNum, numLen * sizeof(wchar_t));
        *textLen = (int)(prefixLen + numLen + suffixLen);
    }
    
    /* Start countdown thread */
    g_exitInProgress = TRUE;
    g_exitThread = CreateThread(NULL, 0, ExitCountdownThread, 
                                (LPVOID)(intptr_t)seconds, 0, NULL);
    if (!g_exitThread) {
        g_exitInProgress = FALSE;
        LOG_ERROR("PluginExit: Failed to create countdown thread");
        return FALSE;
    }
    
    LOG_INFO("PluginExit: Started %d second countdown", seconds);
    return TRUE;
}

void PluginExit_Cancel(void) {
    if (!g_exitInProgress) return;
    
    g_exitInProgress = FALSE;
    
    /* Wait for thread */
    if (g_exitThread) {
        DWORD result = WaitForSingleObject(g_exitThread, 3000);
        if (result != WAIT_OBJECT_0) {
            LOG_ERROR("PluginExit: Thread did not terminate cleanly");
        }
        CloseHandle(g_exitThread);
        g_exitThread = NULL;
    }
    
    /* Cleanup templates */
    if (g_exitPrefix) {
        free(g_exitPrefix);
        g_exitPrefix = NULL;
    }
    if (g_exitSuffix) {
        free(g_exitSuffix);
        g_exitSuffix = NULL;
    }
    
    LOG_INFO("PluginExit: Countdown cancelled");
}
