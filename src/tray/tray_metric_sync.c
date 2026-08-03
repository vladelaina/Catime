/**
 * @file tray_metric_sync.c
 * @brief One-snapshot dispatch for taskbar, tray icon, and tooltip metrics.
 */

#include "tray_internal.h"

#include "taskbar_monitor.h"
#include "tray/tray_animation_core.h"

#define TRAY_METRIC_STALE_GRACE_MS 5000ULL

static SystemMonitorSnapshot g_lastAvailableSnapshot = {0};

static BOOL IsRecentSample(ULONGLONG now, ULONGLONG sampleTick) {
    return sampleTick != 0 && now >= sampleTick &&
           now - sampleTick <= TRAY_METRIC_STALE_GRACE_MS;
}

static void ReuseRecentSamples(SystemMonitorSnapshot* snapshot) {
    if (!snapshot) return;
    ULONGLONG now = GetTickCount64();
    if (!snapshot->cpuAvailable &&
        g_lastAvailableSnapshot.cpuAvailable &&
        IsRecentSample(now, g_lastAvailableSnapshot.basicSampleTick)) {
        snapshot->cpuPercent = g_lastAvailableSnapshot.cpuPercent;
        snapshot->cpuAvailable = TRUE;
        snapshot->basicSampleTick =
            g_lastAvailableSnapshot.basicSampleTick;
    }
    if (!snapshot->memoryAvailable &&
        g_lastAvailableSnapshot.memoryAvailable &&
        IsRecentSample(now, g_lastAvailableSnapshot.basicSampleTick)) {
        snapshot->memoryPercent = g_lastAvailableSnapshot.memoryPercent;
        snapshot->memoryAvailable = TRUE;
        snapshot->basicSampleTick =
            g_lastAvailableSnapshot.basicSampleTick;
    }
    if (!snapshot->networkAvailable &&
        g_lastAvailableSnapshot.networkAvailable &&
        IsRecentSample(now, g_lastAvailableSnapshot.networkSampleTick)) {
        snapshot->uploadBytesPerSecond =
            g_lastAvailableSnapshot.uploadBytesPerSecond;
        snapshot->downloadBytesPerSecond =
            g_lastAvailableSnapshot.downloadBytesPerSecond;
        snapshot->networkAvailable = TRUE;
        snapshot->networkSampleTick =
            g_lastAvailableSnapshot.networkSampleTick;
    }
    if (snapshot->cpuAvailable) {
        g_lastAvailableSnapshot.cpuPercent = snapshot->cpuPercent;
        g_lastAvailableSnapshot.cpuAvailable = TRUE;
        g_lastAvailableSnapshot.basicSampleTick = snapshot->basicSampleTick;
    }
    if (snapshot->memoryAvailable) {
        g_lastAvailableSnapshot.memoryPercent = snapshot->memoryPercent;
        g_lastAvailableSnapshot.memoryAvailable = TRUE;
        g_lastAvailableSnapshot.basicSampleTick = snapshot->basicSampleTick;
    }
    if (snapshot->networkAvailable) {
        g_lastAvailableSnapshot.uploadBytesPerSecond =
            snapshot->uploadBytesPerSecond;
        g_lastAvailableSnapshot.downloadBytesPerSecond =
            snapshot->downloadBytesPerSecond;
        g_lastAvailableSnapshot.networkAvailable = TRUE;
        g_lastAvailableSnapshot.networkSampleTick = snapshot->networkSampleTick;
    }
}

static DWORD GetRequiredSnapshotFields(
    BOOL tooltipActive, BOOL iconNeedsSystemMonitor) {
    DWORD fields = TaskbarMonitor_GetRequiredSnapshotFields();
    if (tooltipActive || iconNeedsSystemMonitor) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY;
    }
    if (tooltipActive) {
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
    ReuseRecentSamples(snapshot);
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
