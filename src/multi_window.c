/** @file multi_window.c @brief Process coordination for independent timer windows. */
#include "multi_window.h"
#include <wchar.h>

#define CATIME_MULTI_WINDOW_MUTEX L"Local\\Vladelaina.Catime.MultiWindowCreate"
#define CATIME_MAIN_WINDOW_CLASS L"CatimeWindowClass"
#define CATIME_MAIN_WINDOW_TITLE L"Catime"

static HANDLE g_creationMutex = NULL;
static int g_initialWindowIndex = 0;

BOOL MultiWindow_IsSecondary(void) {
    return wcsstr(GetCommandLineW(), L"--new-window") != NULL;
}

static int CountMainWindows(void) {
    int count = 0;
    HWND window = NULL;
    while ((window = FindWindowExW(NULL, window, CATIME_MAIN_WINDOW_CLASS,
                                  CATIME_MAIN_WINDOW_TITLE)) != NULL) count++;
    return count;
}

BOOL MultiWindow_BeginMainWindowCreation(void) {
    g_creationMutex = CreateMutexW(NULL, FALSE, CATIME_MULTI_WINDOW_MUTEX);
    if (!g_creationMutex || WaitForSingleObject(g_creationMutex, INFINITE) !=
                              WAIT_OBJECT_0) {
        if (g_creationMutex) CloseHandle(g_creationMutex);
        g_creationMutex = NULL;
        return FALSE;
    }
    g_initialWindowIndex = CountMainWindows();
    if (g_initialWindowIndex < CATIME_MAX_TIMER_WINDOWS) return TRUE;
    MessageBoxW(NULL, L"At most 20 Catime timer windows can run at once.",
                L"Catime", MB_OK | MB_ICONINFORMATION);
    MultiWindow_EndMainWindowCreation();
    return FALSE;
}

void MultiWindow_EndMainWindowCreation(void) {
    if (!g_creationMutex) return;
    ReleaseMutex(g_creationMutex);
    CloseHandle(g_creationMutex);
    g_creationMutex = NULL;
}

void MultiWindow_OffsetInitialPosition(int* x, int* y) {
    if (!MultiWindow_IsSecondary() || !x || !y) return;
    int offset = MultiWindow_GetOffsetForWindowIndex(g_initialWindowIndex);
    *x += offset;
    *y += offset;
}

int MultiWindow_GetOffsetForWindowIndex(int index) {
    if (index < 0) index = 0;
    return (index % 8 + 1) * 28;
}

BOOL MultiWindow_LaunchNewTimerWindow(HWND hwnd) {
    wchar_t executable[MAX_PATH] = {0};
    if (!GetModuleFileNameW(NULL, executable, _countof(executable))) return FALSE;
    wchar_t commandLine[MAX_PATH + 32] = {0};
    _snwprintf_s(commandLine, _countof(commandLine), _TRUNCATE,
                 L"\"%ls\" --new-window", executable);
    STARTUPINFOW startupInfo = {sizeof(startupInfo)};
    PROCESS_INFORMATION processInfo = {0};
    BOOL started = CreateProcessW(executable, commandLine, NULL, NULL, FALSE,
                                  0, NULL, NULL, &startupInfo, &processInfo);
    if (started) {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
    } else if (hwnd) {
        MessageBoxW(hwnd, L"Failed to create a new Catime timer window.",
                    L"Catime", MB_OK | MB_ICONERROR);
    }
    return started;
}
