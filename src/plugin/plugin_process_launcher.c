/**
 * @file plugin_process_launcher.c
 * @brief Background plugin process creation and monitoring thread.
 */

#include "plugin_process_internal.h"

DWORD WINAPI PluginLauncherThread(LPVOID parameter) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)parameter;
    PluginInfo* plugin = &args->pluginSnapshot;
    args->errorMsg[0] = L'\0';

    wchar_t workDir[MAX_PATH];
    wcsncpy(workDir, plugin->path, MAX_PATH - 1);
    workDir[MAX_PATH - 1] = L'\0';
    wchar_t* separator = wcsrchr(workDir, L'\\');
    if (separator) *separator = L'\0';

    HANDLE process = NULL;
    HANDLE monitor = NULL;
    DWORD processId = 0;
    const wchar_t* interpreter = PluginProcess_GetInterpreter(plugin->path);

    if (interpreter) {
        wchar_t commandLine[MAX_PATH * 2 + 256];
        if (_snwprintf_s(commandLine, _countof(commandLine), _TRUNCATE,
                        L"%s \"%s\"", interpreter, plugin->path) < 0) {
            _snwprintf_s(args->errorMsg, _countof(args->errorMsg),
                         _TRUNCATE, L"Path too long");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
        STARTUPINFOW startup = {0};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION processInfo = {0};
        if (!CreateProcessW(NULL, commandLine, NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, workDir,
                            &startup, &processInfo)) {
            LOG_ERROR("[Thread] CreateProcess FAILED! Error: %lu",
                      GetLastError());
            const wchar_t* name = PluginProcess_GetInterpreterName(plugin->path);
            _snwprintf_s(args->errorMsg, _countof(args->errorMsg),
                         _TRUNCATE, L"%s not found", name ? name : L"Launch");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
        process = processInfo.hProcess;
        processId = processInfo.dwProcessId;
        CloseHandle(processInfo.hThread);
    } else {
        SHELLEXECUTEINFOW execute = {0};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        execute.lpVerb = L"open";
        execute.lpFile = plugin->path;
        execute.lpDirectory = workDir;
        execute.nShow = SW_HIDE;
        if (!ShellExecuteExW(&execute) || !execute.hProcess) {
            _snwprintf_s(args->errorMsg, _countof(args->errorMsg),
                         _TRUNCATE, L"Launch failed");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
        process = execute.hProcess;
        processId = GetProcessId(process);
        if (!processId) {
            CloseHandle(process);
            _snwprintf_s(args->errorMsg, _countof(args->errorMsg),
                         _TRUNCATE, L"Launch failed");
            args->success = FALSE;
            return FinishPluginLaunchFailure(args);
        }
    }

    if (!DuplicateHandle(GetCurrentProcess(), process,
                         GetCurrentProcess(), &monitor, SYNCHRONIZE,
                         FALSE, 0)) {
        PluginProcess_TerminateTree(processId, 0);
        CloseHandle(process);
        _snwprintf_s(args->errorMsg, _countof(args->errorMsg),
                     _TRUNCATE, L"Launch failed");
        args->success = FALSE;
        return FinishPluginLaunchFailure(args);
    }
    if (args->hJob && !AssignProcessToJobObject(args->hJob, process)) {
        DWORD error = GetLastError();
        if (error != ERROR_ACCESS_DENIED) {
            LOG_WARNING("[Thread] Failed to assign to Job: %lu", error);
        }
    }
    ClosePluginLaunchJobHandle(args);

    plugin->pi.hProcess = process;
    plugin->pi.hThread = NULL;
    plugin->pi.dwProcessId = processId;
    plugin->pi.dwThreadId = 0;
    plugin->isRunning = TRUE;
    args->success = TRUE;
    if (!SignalPluginLaunchReady(args)) {
        PluginProcess_TerminateTree(processId, 0);
        if (monitor) CloseHandle(monitor);
        if (process) CloseHandle(process);
        ReleaseAbandonedPluginLaunchArgs(args);
        return 0;
    }
    ReleaseAbandonedPluginLaunchArgs(args);

    if (monitor) {
        DWORD waitResult = WaitForSingleObject(monitor, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            LOG_WARNING("[Thread] Wait failed (result=%lu, error=%lu)",
                        waitResult, GetLastError());
        }
        PluginProcess_TerminateTree(processId, 0);
        PluginManager_HandleProcessExit(processId);
        CloseHandle(monitor);
    }
    return 0;
}
