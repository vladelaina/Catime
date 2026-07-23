#include "plugin_exit_internal.h"
#include "plugin/plugin_exit.h"
#include "../resource/resource.h"
#include "log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

HWND g_notifyWnd = NULL;
CRITICAL_SECTION* g_dataCS = NULL;
extern wchar_t* g_pluginDisplayText;
extern size_t g_pluginDisplayTextLen;
extern BOOL g_hasPluginData;

static BOOL IsValidPluginNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    wchar_t className[64] = {0};
    return GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
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

DWORD WINAPI ExitCountdownThread(LPVOID parameter) {
    int seconds = (int)(intptr_t)parameter;
    while (seconds > 0 && IsExitInProgress()) {
        wchar_t countdownNum[16];
        _snwprintf_s(countdownNum, _countof(countdownNum), _TRUNCATE,
                     L"%d", seconds);
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
                LOG_WARNING("PluginExit: Failed to allocate countdown text buffer");
                ReleaseSRWLockShared(&g_exitLock);
                InterlockedExchange(&g_exitInProgress, FALSE);
                return 0;
            }
            displayText = heapDisplay;
        }
        displayText[0] = L'\0';
        if (g_exitPrefix) wcsncat_s(displayText, totalLen, g_exitPrefix, prefixLen);
        wcsncat_s(displayText, totalLen, countdownNum, numLen);
        if (g_exitSuffix) wcsncat_s(displayText, totalLen, g_exitSuffix, suffixLen);
        displayLen = totalLen;
        displayReady = TRUE;
        ReleaseSRWLockShared(&g_exitLock);
        if (displayReady && dataCS) {
            EnterCriticalSection(dataCS);
            if (!g_pluginDisplayText || g_pluginDisplayTextLen < displayLen) {
                wchar_t* newBuffer = (wchar_t*)realloc(
                    g_pluginDisplayText, displayLen * sizeof(wchar_t));
                if (newBuffer) {
                    g_pluginDisplayText = newBuffer;
                    g_pluginDisplayTextLen = displayLen;
                }
            }
            if (g_pluginDisplayText && g_pluginDisplayTextLen >= displayLen) {
                memcpy(g_pluginDisplayText, displayText,
                       displayLen * sizeof(wchar_t));
                g_hasPluginData = TRUE;
            }
            LeaveCriticalSection(dataCS);
        }
        free(heapDisplay);
        if (IsValidPluginNotifyWindow(notifyWnd))
            InvalidateRect(notifyWnd, NULL, FALSE);
        if (stopEvent && WaitForSingleObject(stopEvent, 1000) == WAIT_OBJECT_0)
            break;
        seconds--;
    }
    if (IsExitInProgress()) {
        ExitCountdownSnapshot snapshot = {0};
        SnapshotExitCountdownState(&snapshot);
        if (IsValidPluginNotifyWindow(snapshot.notifyWnd))
            PostMessage(snapshot.notifyWnd, CLOCK_WM_PLUGIN_EXIT, 0, 0);
    }
    InterlockedExchange(&g_exitInProgress, FALSE);
    return 0;
}
