/**
 * @file plugin_process_launch_helpers.c
 * @brief Plugin launcher reference counting and interpreter selection.
 */

#include "plugin_process_internal.h"

void ClosePluginLaunchJobHandle(PluginLauncherArgs* args) {
    if (!args || !args->hJob) return;
    CloseHandle(args->hJob);
    args->hJob = NULL;
}

BOOL SignalPluginLaunchReady(PluginLauncherArgs* args) {
    if (!args) return FALSE;
    HANDLE event = args->hReadySignalEvent;
    LONG previous = InterlockedCompareExchange(
        &args->readyState, PLUGIN_LAUNCH_READY_SIGNALED,
        PLUGIN_LAUNCH_READY_PENDING);
    if (previous != PLUGIN_LAUNCH_READY_PENDING) return FALSE;
    if (event) {
        if (!SetEvent(event)) {
            LOG_WARNING("[Process] Failed to signal plugin launch ready event: %lu",
                        GetLastError());
        }
        CloseHandle(event);
        args->hReadySignalEvent = NULL;
    }
    return TRUE;
}

void ReleaseAbandonedPluginLaunchArgs(PluginLauncherArgs* args) {
    if (!args || InterlockedDecrement(&args->refCount) != 0) return;
    ClosePluginLaunchJobHandle(args);
    if (args->hReadySignalEvent) CloseHandle(args->hReadySignalEvent);
    free(args);
}

DWORD FinishPluginLaunchFailure(PluginLauncherArgs* args) {
    ClosePluginLaunchJobHandle(args);
    SignalPluginLaunchReady(args);
    ReleaseAbandonedPluginLaunchArgs(args);
    return 0;
}

const wchar_t* PluginProcess_GetInterpreter(const wchar_t* path) {
    const wchar_t* extension = path ? wcsrchr(path, L'.') : NULL;
    if (!extension) return NULL;
    if (_wcsicmp(extension, L".py") == 0 ||
        _wcsicmp(extension, L".pyw") == 0) return L"pythonw.exe";
    if (_wcsicmp(extension, L".ps1") == 0) {
        return L"powershell.exe -NoProfile -ExecutionPolicy Bypass "
               L"-WindowStyle Hidden -NonInteractive -File";
    }
    if (_wcsicmp(extension, L".bat") == 0 ||
        _wcsicmp(extension, L".cmd") == 0) return L"cmd.exe /c";
    if (_wcsicmp(extension, L".vbs") == 0 ||
        _wcsicmp(extension, L".vbe") == 0) return L"wscript.exe //nologo //B";
    if (_wcsicmp(extension, L".js") == 0 ||
        _wcsicmp(extension, L".mjs") == 0 ||
        _wcsicmp(extension, L".cjs") == 0) return L"node.exe";
    if (_wcsicmp(extension, L".lua") == 0) return L"lua.exe";
    if (_wcsicmp(extension, L".rbw") == 0) return L"rubyw.exe";
    if (_wcsicmp(extension, L".rb") == 0) return L"ruby.exe";
    if (_wcsicmp(extension, L".pl") == 0 ||
        _wcsicmp(extension, L".pm") == 0) return L"perl.exe";
    if (_wcsicmp(extension, L".php") == 0) return L"php.exe";
    if (_wcsicmp(extension, L".sh") == 0) return L"bash.exe";
    return NULL;
}

const wchar_t* PluginProcess_GetInterpreterName(const wchar_t* path) {
    const wchar_t* interpreter = PluginProcess_GetInterpreter(path);
    if (!interpreter) return NULL;
    static wchar_t name[32];
    wcsncpy(name, interpreter, _countof(name) - 1);
    name[_countof(name) - 1] = L'\0';
    wchar_t* space = wcschr(name, L' ');
    if (space) *space = L'\0';
    return name;
}
