/**
 * @file tray_metric_sync.c
 * @brief One-snapshot dispatch for taskbar, tray icon, and tooltip metrics.
 */

#include "tray_internal.h"

#include "taskbar_monitor.h"
#include "tray/tray_animation_core.h"

static DWORD GetRequiredSnapshotFields(
    BOOL tooltipActive, BOOL iconNeedsSystemMonitor) {
    DWORD fields = 0;
    if (tooltipActive || iconNeedsSystemMonitor ||
        TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_CPU_MEMORY)) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY;
    }
    if (tooltipActive || TaskbarMonitor_IsOptionEnabled(
            TASKBAR_MONITOR_OPTION_NETWORK)) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_NETWORK;
    }
    return fields;
}

const SystemMonitorSnapshot* TrayMetricSync_GetSnapshot(
    BOOL tooltipActive, BOOL iconNeedsSystemMonitor,
    SystemMonitorSnapshot* snapshot) {
    DWORD fields = GetRequiredSnapshotFields(
        tooltipActive, iconNeedsSystemMonitor);
    if (!fields || !GetSystemMetricsSnapshot(fields, snapshot)) {
        return NULL;
    }
    if (TaskbarMonitor_IsEnabled()) {
        TaskbarMonitor_UpdateSnapshot(snapshot);
    }
    return snapshot;
}

void TrayMetricSync_UpdateIcon(
    BOOL dynamicIcon, BOOL iconNeedsSystemMonitor,
    const SystemMonitorSnapshot* snapshot) {
    if (!dynamicIcon || (iconNeedsSystemMonitor && !snapshot)) return;
    (void)TrayAnimation_UpdatePercentIconWithSnapshot(
        iconNeedsSystemMonitor ? snapshot : NULL, NULL);
}

BOOL TrayMetricSync_UpdateIconAndTooltip(
    BOOL dynamicIcon, BOOL iconNeedsSystemMonitor,
    const SystemMonitorSnapshot* snapshot, const wchar_t* tip) {
    if (!dynamicIcon || (iconNeedsSystemMonitor && !snapshot)) {
        return FALSE;
    }
    return TrayAnimation_UpdatePercentIconWithSnapshot(
        iconNeedsSystemMonitor ? snapshot : NULL, tip);
}
