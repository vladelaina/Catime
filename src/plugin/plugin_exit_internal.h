#ifndef PLUGIN_EXIT_INTERNAL_H
#define PLUGIN_EXIT_INTERNAL_H

#include <windows.h>

#define MAX_EXIT_COUNTDOWN_SECONDS 3600

extern volatile LONG g_exitInProgress;
extern SRWLOCK g_exitLock;
extern CONDITION_VARIABLE g_exitStateChanged;
extern BOOL g_exitThreadStopInProgress;
extern HANDLE g_exitThread;
extern HANDLE g_exitStopEvent;
extern wchar_t* g_exitPrefix;
extern wchar_t* g_exitSuffix;
extern DWORD g_exitStartFailureCooldownUntil;
extern HWND g_notifyWnd;
extern CRITICAL_SECTION* g_dataCS;
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

BOOL IsExitInProgress(void);
BOOL IsExitStartFailureCoolingDown(DWORD now);
void MarkExitStartFailure(DWORD now);
void CleanupCompletedExitThreadHandleLocked(void);
void FreeExitTemplatesLocked(void);
BOOL EnsureExitStopEventLocked(void);
DWORD WINAPI ExitCountdownThread(LPVOID parameter);

#endif
