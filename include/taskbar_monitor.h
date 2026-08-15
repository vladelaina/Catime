/**
 * @file taskbar_monitor.h
 * @brief Compact CPU, memory, and network display hosted by the taskbar.
 */

#ifndef CATIME_TASKBAR_MONITOR_H
#define CATIME_TASKBAR_MONITOR_H

#include <windows.h>
#include "system_monitor.h"

typedef enum {
    TASKBAR_MONITOR_OPTION_CPU_MEMORY = 0,
    TASKBAR_MONITOR_OPTION_NETWORK
} TaskbarMonitorOption;

/** Apply configured preferences without writing them back to disk. */
void TaskbarMonitor_ApplyConfig(BOOL enabled, BOOL cpuMemoryEnabled,
                                BOOL networkEnabled);

/**
 * Prepare a stable, non-interactive taskbar host before opening the tray menu.
 * Hover previews can then update their contents without changing Explorer's
 * layout while its popup-menu loop is active.
 */
BOOL TaskbarMonitor_BeginMenuPreviewSession(void);

/** Restore normal taskbar sizing and interaction after the tray menu closes. */
void TaskbarMonitor_EndMenuPreviewSession(void);

/**
 * Initialize the shared taskbar monitor after the main window is available.
 * Only one Catime process displays it; standby instances take over as needed.
 */
BOOL TaskbarMonitor_Initialize(HINSTANCE instance, HWND owner);

/** Restore any reserved shell space and release taskbar monitor resources. */
void TaskbarMonitor_Shutdown(void);

/** Return the user preference, including while Explorer is restarting. */
BOOL TaskbarMonitor_IsEnabled(void);

/** Return whether one metric group is selected. */
BOOL TaskbarMonitor_IsOptionEnabled(TaskbarMonitorOption option);

/** Return metric fields needed by the active display or menu prefetch. */
DWORD TaskbarMonitor_GetRequiredSnapshotFields(void);

/** Keep sampling alive across a temporary menu preview transition. */
BOOL TaskbarMonitor_NeedsSystemMonitor(void);

/** Persist and apply one metric-group preference. */
BOOL TaskbarMonitor_SetOptionEnabled(TaskbarMonitorOption option,
                                     BOOL enabled);

/** Recreate the monitor after Explorer broadcasts TaskbarCreated. */
void TaskbarMonitor_OnTaskbarCreated(void);

/** Re-evaluate taskbar geometry, DPI, and colors. */
void TaskbarMonitor_Refresh(void);

/** Repaint immediately, then confirm the final Windows taskbar theme. */
void TaskbarMonitor_RefreshAppearance(void);

/** Apply the same sampled metrics used by the tray icon and tooltip. */
void TaskbarMonitor_UpdateSnapshot(
    const SystemMonitorSnapshot* snapshot);

#endif /* CATIME_TASKBAR_MONITOR_H */
