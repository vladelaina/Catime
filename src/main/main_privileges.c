#include "main_initialization_internal.h"
#include "log.h"

#include <stdlib.h>
#include <windows.h>

static BOOL IsElevated(void) {
    BOOL elevated = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION information;
        DWORD size = sizeof(information);
        if (GetTokenInformation(token, TokenElevation, &information,
                                 sizeof(information), &size)) {
            elevated = information.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return elevated;
}

static BOOL RelaunchAsStandardUser(void) {
    HWND shellWindow = GetShellWindow();
    if (!shellWindow) return FALSE;
    DWORD shellPid = 0;
    GetWindowThreadProcessId(shellWindow, &shellPid);
    if (!shellPid) return FALSE;

    HANDLE shellProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION, FALSE, shellPid);
    if (!shellProcess) return FALSE;
    HANDLE shellToken = NULL;
    if (!OpenProcessToken(shellProcess, TOKEN_DUPLICATE, &shellToken)) {
        CloseHandle(shellProcess);
        return FALSE;
    }
    HANDLE newToken = NULL;
    BOOL duplicated = DuplicateTokenEx(
        shellToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation,
        TokenPrimary, &newToken);
    if (!duplicated) {
        CloseHandle(shellToken);
        CloseHandle(shellProcess);
        return FALSE;
    }

    const wchar_t* commandLine = GetCommandLineW();
    size_t commandLength = wcslen(commandLine) + 1;
    wchar_t* commandCopy =
        (wchar_t*)malloc(commandLength * sizeof(wchar_t));
    if (!commandCopy) {
        CloseHandle(newToken);
        CloseHandle(shellToken);
        CloseHandle(shellProcess);
        return FALSE;
    }
    wcscpy_s(commandCopy, commandLength, commandLine);
    STARTUPINFOW startup = {0};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {0};
    BOOL created = CreateProcessWithTokenW(
        newToken, LOGON_WITH_PROFILE, NULL, commandCopy, 0,
        NULL, NULL, &startup, &process);
    free(commandCopy);

    if (created) {
        DWORD exitCode = 0;
        if (WaitForSingleObject(process.hProcess, 100) != WAIT_TIMEOUT &&
            GetExitCodeProcess(process.hProcess, &exitCode) &&
            exitCode != STILL_ACTIVE) {
            LOG_WARNING("Relaunched process exited immediately: %lu", exitCode);
            created = FALSE;
        }
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    } else {
        LOG_WARNING("CreateProcessWithTokenW failed: %lu", GetLastError());
    }
    CloseHandle(newToken);
    CloseHandle(shellToken);
    CloseHandle(shellProcess);
    return created;
}

static BOOL IsUacEnabled(void) {
    HKEY key = NULL;
    DWORD value = 0;
    DWORD size = sizeof(value);
    LSTATUS status = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) return FALSE;
    status = RegQueryValueExW(
        key, L"EnableLUA", NULL, NULL, (LPBYTE)&value, &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && value != 0;
}

static BOOL IsSecondaryLogonRunning(void) {
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager) return FALSE;
    SC_HANDLE service = OpenServiceW(
        manager, L"seclogon", SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(manager);
        return FALSE;
    }
    SERVICE_STATUS status;
    BOOL running = QueryServiceStatus(service, &status) &&
                   status.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return running;
}

static BOOL IsWindowsServer(void) {
    OSVERSIONINFOEXW version = {0};
    version.dwOSVersionInfoSize = sizeof(version);
    DWORDLONG condition = 0;
    VER_SET_CONDITION(condition, VER_PRODUCT_TYPE, VER_EQUAL);
    version.wProductType = VER_NT_WORKSTATION;
    return !VerifyVersionInfoW(
        &version, VER_PRODUCT_TYPE, condition);
}

static BOOL IsShellElevated(void) {
    HWND shellWindow = GetShellWindow();
    if (!shellWindow) return TRUE;
    DWORD shellPid = 0;
    GetWindowThreadProcessId(shellWindow, &shellPid);
    HANDLE process = shellPid
        ? OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, shellPid) : NULL;
    if (!process) return TRUE;
    HANDLE token = NULL;
    TOKEN_ELEVATION information;
    DWORD size = sizeof(information);
    BOOL elevated = TRUE;
    if (OpenProcessToken(process, TOKEN_QUERY, &token) &&
        GetTokenInformation(token, TokenElevation, &information,
                            sizeof(information), &size)) {
        elevated = information.TokenIsElevated;
    }
    if (token) CloseHandle(token);
    CloseHandle(process);
    return elevated;
}

void Main_DropPrivileges(void) {
    if (!IsElevated() || IsWindowsServer() || !IsUacEnabled() ||
        !IsSecondaryLogonRunning() || IsShellElevated()) {
        return;
    }
    LOG_INFO("Elevated process meeting drop-privilege preconditions");
    if (RelaunchAsStandardUser()) {
        ExitProcess(0);
    }
    LOG_WARNING("Failed to switch to standard user; continuing elevated");
}
