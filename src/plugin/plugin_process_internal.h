/**
 * @file plugin_process_internal.h
 * @brief Shared plugin process launch, monitoring, and termination internals.
 */

#ifndef CATIME_PLUGIN_PROCESS_INTERNAL_H
#define CATIME_PLUGIN_PROCESS_INTERNAL_H

#include "plugin/plugin_process.h"
#include "plugin/plugin_data.h"
#include "log.h"

#include <shellapi.h>
#include <string.h>
#include <stdlib.h>
#include <tlhelp32.h>

#define JOB_PROCESS_STACK_CAPACITY 16
#define PROCESS_TREE_STACK_CAPACITY 256
#define PROCESS_TREE_MAX_DEPTH 32
#define PROCESS_TREE_VISITED_CAPACITY (PROCESS_TREE_MAX_DEPTH + 1)
#define PLUGIN_LAUNCH_READY_TIMEOUT_MS 5000
#define PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS 2000
#define PLUGIN_LAUNCH_START_FAILURE_COOLDOWN_MS 2000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define PLUGIN_LAUNCH_READY_PENDING 0
#define PLUGIN_LAUNCH_READY_SIGNALED 1
#define PLUGIN_LAUNCH_READY_ABANDONED 2

typedef struct {
    DWORD processId;
    DWORD parentProcessId;
} ProcessTreeEntry;

typedef struct {
    HANDLE hReadySignalEvent;
    HANDLE hJob;
    BOOL success;
    wchar_t errorMsg[128];
    volatile LONG readyState;
    volatile LONG refCount;
    PluginInfo pluginSnapshot;
} PluginLauncherArgs;

extern HANDLE g_pluginJob;
extern HWND g_pluginNotifyWindow;
extern wchar_t g_pluginLastLaunchError[128];
extern DWORD g_pluginLaunchFailureCooldownUntil;

void PluginProcess_ClearHandles(PluginInfo* plugin,
                                BOOL waitForMonitorThread);
BOOL IsValidPluginNotifyWindow(HWND window);
void ClosePluginLaunchJobHandle(PluginLauncherArgs* args);
BOOL SignalPluginLaunchReady(PluginLauncherArgs* args);
void ReleaseAbandonedPluginLaunchArgs(PluginLauncherArgs* args);
DWORD FinishPluginLaunchFailure(PluginLauncherArgs* args);
DWORD WINAPI PluginLauncherThread(LPVOID parameter);

BOOL PluginProcess_IsFileInterpreter(const wchar_t* path);
const wchar_t* PluginProcess_GetInterpreter(const wchar_t* path);
const wchar_t* PluginProcess_GetInterpreterName(const wchar_t* path);

void PluginProcess_TerminateTree(DWORD pid, int depth);
void PluginProcess_TerminateAllJobProcesses(void);

#endif /* CATIME_PLUGIN_PROCESS_INTERNAL_H */
