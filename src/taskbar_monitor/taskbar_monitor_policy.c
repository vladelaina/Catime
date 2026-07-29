/**
 * @file taskbar_monitor_policy.c
 * @brief Pure taskbar-monitor sampling and snapshot policies.
 */

#include "taskbar_monitor_internal.h"

DWORD TaskbarMonitor_GetSnapshotFields(
    BOOL cpuMemoryEnabled, BOOL networkEnabled) {
    DWORD fields = 0;
    if (cpuMemoryEnabled) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY;
    }
    if (networkEnabled) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_NETWORK;
    }
    return fields;
}

BOOL TaskbarMonitor_ShouldKeepSystemMonitorActive(
    BOOL cpuMemoryEnabled, BOOL networkEnabled,
    BOOL menuPreviewSessionActive,
    BOOL originalCpuMemoryEnabled,
    BOOL originalNetworkEnabled) {
    if (cpuMemoryEnabled || networkEnabled) return TRUE;
    return menuPreviewSessionActive &&
           (originalCpuMemoryEnabled || originalNetworkEnabled);
}

BOOL TaskbarMonitor_SnapshotsEqual(
    const SystemMonitorSnapshot* first,
    const SystemMonitorSnapshot* second) {
    if (!first || !second) return FALSE;
    return first->cpuPercent == second->cpuPercent &&
           first->memoryPercent == second->memoryPercent &&
           first->uploadBytesPerSecond ==
               second->uploadBytesPerSecond &&
           first->downloadBytesPerSecond ==
               second->downloadBytesPerSecond &&
           first->revision == second->revision &&
           first->basicSampleTick == second->basicSampleTick &&
           first->networkSampleTick == second->networkSampleTick &&
           first->cpuAvailable == second->cpuAvailable &&
           first->memoryAvailable == second->memoryAvailable &&
           first->networkAvailable == second->networkAvailable;
}
