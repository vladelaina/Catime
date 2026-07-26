#include "main/main_initialization.h"
#include "main_initialization_internal.h"
#include "log.h"
#include "timer/timer_events.h"
#include "window/window_desktop_integration.h"
#include "../../resource/resource.h"

#include <stdlib.h>
#include <wchar.h>

#define STARTUP_WINDOW_RECOVERY_DELAY_MS 2000

static int s_ciSmokeExitCode = 0;

static BOOL ContainsFlag(const wchar_t* commandLine, const wchar_t* flag) {
    if (!commandLine || !flag || !*flag) return FALSE;
    const wchar_t* position = wcsstr(commandLine, flag);
    while (position) {
        wchar_t before = position == commandLine ? L' ' : position[-1];
        wchar_t after = position[wcslen(flag)];
        BOOL validBefore = before == L' ' || before == L'\t' || before == L'"';
        BOOL validAfter = after == L'\0' || after == L' ' || after == L'\t' ||
                          after == L'"' || after == L'=';
        if (validBefore && validAfter) return TRUE;
        position = wcsstr(position + 1, flag);
    }
    return FALSE;
}

BOOL IsCiSmokeMode(void) {
    return ContainsFlag(GetCommandLineW(), L"--ci-smoke");
}

UINT GetCiExitTimeoutMs(void) {
    const wchar_t* marker = wcsstr(GetCommandLineW(), L"--ci-exit-ms=");
    if (!marker) return 3000;
    marker += wcslen(L"--ci-exit-ms=");
    wchar_t* end = NULL;
    unsigned long value = wcstoul(marker, &end, 10);
    return end != marker && value >= 250 && value <= 60000
        ? (UINT)value : 3000;
}

static VOID CALLBACK SmokeExitTimer(HWND hwnd, UINT message,
                                    UINT_PTR timerId, DWORD time) {
    UNREFERENCED_PARAMETER(time);
    if (message != WM_TIMER || timerId != TIMER_ID_CI_EXIT) return;
    KillTimer(hwnd, timerId);
    if (!Timer_HasMainWindowPainted()) {
        s_ciSmokeExitCode = 2;
        if (Timer_HasPresentedMainWindowFrame()) {
            LOG_ERROR("CI smoke presented a frame with empty timer text");
        } else {
            LOG_ERROR("CI smoke did not present a timer frame");
        }
    }
    LOG_INFO("CI smoke timeout reached, closing application");
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void Main_ScheduleCiSmokeExit(HWND hwnd, UINT delayMs) {
    if (!SetTimer(hwnd, TIMER_ID_CI_EXIT, delayMs, SmokeExitTimer)) {
        LOG_WARNING("CI smoke exit timer failed (error=%lu)", GetLastError());
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
}

int Main_GetCiSmokeExitCode(void) {
    return s_ciSmokeExitCode;
}

void Main_ScheduleStartupWindowRecovery(HWND hwnd, BOOL topmost) {
    UINT timerId = topmost ? TIMER_ID_TOPMOST_RETRY : TIMER_ID_VISIBILITY_RETRY;
    if (!SetTimer(hwnd, timerId, STARTUP_WINDOW_RECOVERY_DELAY_MS, NULL)) {
        LOG_WARNING("Startup recovery timer %u failed (error=%lu)",
                    timerId, GetLastError());
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
}
