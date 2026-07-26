/**
 * @file taskbar_monitor.h
 * @brief Compact CPU, memory, and network display hosted by the taskbar.
 */

#ifndef CATIME_TASKBAR_MONITOR_H
#define CATIME_TASKBAR_MONITOR_H

#include <windows.h>

typedef enum {
    TASKBAR_MONITOR_OPTION_CPU_MEMORY = 0,
    TASKBAR_MONITOR_OPTION_NETWORK
} TaskbarMonitorOption;

/** Apply configured preferences without writing them back to disk. */
void TaskbarMonitor_ApplyConfig(BOOL enabled, BOOL cpuMemoryEnabled,
                                BOOL networkEnabled);

/** Initialize the taskbar monitor after the main window is available. */
BOOL TaskbarMonitor_Initialize(HINSTANCE instance, HWND owner);

/** Restore any reserved shell space and release taskbar monitor resources. */
void TaskbarMonitor_Shutdown(void);

/** Return the user preference, including while Explorer is restarting. */
BOOL TaskbarMonitor_IsEnabled(void);

/** Return whether one metric group is selected. */
BOOL TaskbarMonitor_IsOptionEnabled(TaskbarMonitorOption option);

/** Persist and apply one metric-group preference. */
BOOL TaskbarMonitor_SetOptionEnabled(TaskbarMonitorOption option,
                                     BOOL enabled);

/** Recreate the monitor after Explorer broadcasts TaskbarCreated. */
void TaskbarMonitor_OnTaskbarCreated(void);

/** Re-evaluate taskbar geometry, DPI, and colors. */
void TaskbarMonitor_Refresh(void);

#endif /* CATIME_TASKBAR_MONITOR_H */
