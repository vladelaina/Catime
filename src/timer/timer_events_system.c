/**
 * @file timer_events_system.c
 * @brief One-shot timeout actions that affect the operating system.
 */

#include "timer_events_internal.h"
#include <powrprof.h>

BOOL TimerEvents_IsSystemTimeoutAction(TimeoutActionType action) {
    return action == TIMEOUT_ACTION_SHUTDOWN ||
           action == TIMEOUT_ACTION_RESTART ||
           action == TIMEOUT_ACTION_SLEEP;
}

void Timer_ClearTimeoutSystemActionArm(void) {
    g_armedTimeoutSystemAction = TIMEOUT_ACTION_MESSAGE;
}

void Timer_ArmTimeoutSystemAction(TimeoutActionType action) {
    if (TimerEvents_IsSystemTimeoutAction(action)) {
        g_armedTimeoutSystemAction = action;
    } else {
        Timer_ClearTimeoutSystemActionArm();
    }
}

BOOL TimerEvents_IsSystemTimeoutActionArmed(TimeoutActionType action) {
    return TimerEvents_IsSystemTimeoutAction(action) &&
           g_armedTimeoutSystemAction == action;
}

BOOL TimerEvents_IsSystemTimeoutExecutionContextSafe(void) {
    if (!MainTimer_IsRunning() ||
        CLOCK_SHOW_CURRENT_TIME ||
        CLOCK_COUNT_UP ||
        CLOCK_IS_PAUSED ||
        CLOCK_TOTAL_TIME <= 0 ||
        countdown_elapsed_time < CLOCK_TOTAL_TIME ||
        g_target_end_time <= 0) {
        return FALSE;
    }

    return GetAbsoluteTimeMs() >= g_target_end_time;
}

void TimerEvents_ConsumeBlockedSystemTimeoutAction(HWND hwnd) {
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    Timer_ClearTimeoutSystemActionArm();
    TimerEvents_ResetTimerState(0);
    MainTimer_Stop();
    TimerEvents_ForceWindowRedraw(hwnd);
}

const char* TimerEvents_GetSystemActionName(TimeoutActionType action) {
    switch (action) {
        case TIMEOUT_ACTION_SHUTDOWN: return "shutdown";
        case TIMEOUT_ACTION_RESTART: return "restart";
        case TIMEOUT_ACTION_SLEEP: return "sleep";
        default: return "unknown";
    }
}

static BOOL EnableShutdownPrivilege(void) {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &token)) {
        LOG_WARNING("Failed to open process token for system action (error: %lu)",
                    GetLastError());
        return FALSE;
    }

    TOKEN_PRIVILEGES privileges;
    ZeroMemory(&privileges, sizeof(privileges));
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(NULL,
                               SE_SHUTDOWN_NAME,
                               &privileges.Privileges[0].Luid)) {
        DWORD error = GetLastError();
        CloseHandle(token);
        LOG_WARNING("Failed to look up shutdown privilege (error: %lu)", error);
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(token, FALSE, &privileges, 0, NULL, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(token);
        LOG_WARNING("Failed to enable shutdown privilege (error: %lu)", error);
        return FALSE;
    }

    DWORD adjustError = GetLastError();
    CloseHandle(token);
    if (adjustError == ERROR_NOT_ALL_ASSIGNED) {
        LOG_WARNING("Shutdown privilege is not assigned to this process token");
        return FALSE;
    }

    return TRUE;
}

static BOOL ExecuteSystemPowerAction(TimeoutActionType action) {
    switch (action) {
        case TIMEOUT_ACTION_SHUTDOWN:
            if (!EnableShutdownPrivilege()) return FALSE;
            return ExitWindowsEx(EWX_POWEROFF | EWX_FORCEIFHUNG,
                                 SHTDN_REASON_MAJOR_APPLICATION |
                                 SHTDN_REASON_MINOR_MAINTENANCE |
                                 SHTDN_REASON_FLAG_PLANNED);

        case TIMEOUT_ACTION_RESTART:
            if (!EnableShutdownPrivilege()) return FALSE;
            return ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
                                 SHTDN_REASON_MAJOR_APPLICATION |
                                 SHTDN_REASON_MINOR_MAINTENANCE |
                                 SHTDN_REASON_FLAG_PLANNED);

        case TIMEOUT_ACTION_SLEEP:
            EnableShutdownPrivilege();
            return SetSuspendState(FALSE, FALSE, FALSE);

        default:
            return FALSE;
    }
}

BOOL TimerEvents_ExecuteSystemAction(HWND hwnd, TimeoutActionType action) {
    if (!TimerEvents_IsSystemTimeoutAction(action)) {
        return FALSE;
    }

    if (!TimerEvents_IsSystemTimeoutActionArmed(action)) {
        LOG_WARNING("Blocked unarmed timeout system action: %s",
                    TimerEvents_GetSystemActionName(action));
        TimerEvents_ConsumeBlockedSystemTimeoutAction(hwnd);
        return TRUE;
    }

    if (!TimerEvents_IsSystemTimeoutExecutionContextSafe()) {
        LOG_WARNING("Blocked timeout system action outside completed countdown: %s",
                    TimerEvents_GetSystemActionName(action));
        TimerEvents_ConsumeBlockedSystemTimeoutAction(hwnd);
        return TRUE;
    }

    TimerEvents_ResetTimerState(0);
    MainTimer_Stop();
    TimerEvents_ForceWindowRedraw(hwnd);
    Timer_ClearTimeoutSystemActionArm();

    if (!ExecuteSystemPowerAction(action)) {
        LOG_WARNING("Timeout system action failed: %s (error: %lu)",
                    TimerEvents_GetSystemActionName(action), GetLastError());
    }

    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    return TRUE;
}
