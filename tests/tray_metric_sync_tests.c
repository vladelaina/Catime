#include "system_monitor.h"
#include "taskbar_monitor.h"
#include "tray/tray_animation_core.h"
#include "tray_internal.h"

#include <stdio.h>

static int g_failures = 0;
static BOOL g_cpuMemoryEnabled = FALSE;
static BOOL g_networkEnabled = FALSE;
static DWORD g_requestedFields = 0;
static int g_sampleCalls = 0;
static int g_taskbarUpdates = 0;
static int g_iconUpdates = 0;
static BOOL g_sampleAvailable = TRUE;
static const SystemMonitorSnapshot* g_taskbarSnapshot = NULL;
static const SystemMonitorSnapshot* g_iconSnapshot = NULL;
static const wchar_t* g_iconTooltip = NULL;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

BOOL GetSystemMetricsSnapshot(DWORD fields,
                              SystemMonitorSnapshot* snapshot) {
    ++g_sampleCalls;
    g_requestedFields = fields;
    if (!snapshot) return FALSE;
    snapshot->cpuPercent = 37.25f;
    snapshot->memoryPercent = 61.5f;
    snapshot->uploadBytesPerSecond = 1024.0f;
    snapshot->downloadBytesPerSecond = 2048.0f;
    snapshot->revision = 42;
    snapshot->basicSampleTick = GetTickCount64();
    snapshot->networkSampleTick = snapshot->basicSampleTick;
    snapshot->cpuAvailable = g_sampleAvailable;
    snapshot->memoryAvailable = g_sampleAvailable;
    snapshot->networkAvailable = g_sampleAvailable;
    return TRUE;
}

BOOL TaskbarMonitor_IsEnabled(void) {
    return g_cpuMemoryEnabled || g_networkEnabled;
}

BOOL TaskbarMonitor_IsOptionEnabled(TaskbarMonitorOption option) {
    return option == TASKBAR_MONITOR_OPTION_CPU_MEMORY
        ? g_cpuMemoryEnabled : g_networkEnabled;
}

DWORD TaskbarMonitor_GetRequiredSnapshotFields(void) {
    DWORD fields = 0;
    if (g_cpuMemoryEnabled) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY;
    }
    if (g_networkEnabled) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_NETWORK;
    }
    return fields;
}

void TaskbarMonitor_UpdateSnapshot(
    const SystemMonitorSnapshot* snapshot) {
    ++g_taskbarUpdates;
    g_taskbarSnapshot = snapshot;
}

BOOL TrayAnimation_UpdatePercentIconWithSnapshot(
    const SystemMonitorSnapshot* snapshot,
    const wchar_t* synchronizedTooltip) {
    ++g_iconUpdates;
    g_iconSnapshot = snapshot;
    g_iconTooltip = synchronizedTooltip;
    return synchronizedTooltip != NULL;
}

int main(void) {
    SystemMonitorSnapshot snapshot = {0};
    const SystemMonitorSnapshot* shared = TrayMetricSync_GetSnapshot(
        FALSE, FALSE, &snapshot);
    Expect(shared == NULL && g_sampleCalls == 0,
           "idle dispatch sampled metrics unexpectedly");

    g_cpuMemoryEnabled = TRUE;
    shared = TrayMetricSync_GetSnapshot(FALSE, FALSE, &snapshot);
    Expect(shared == &snapshot,
           "dispatch did not return the caller-owned snapshot");
    Expect(g_requestedFields == SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY,
           "CPU/memory taskbar requested the wrong fields");
    Expect(g_taskbarUpdates == 1 && g_taskbarSnapshot == shared,
           "taskbar did not receive the shared snapshot");

    TrayMetricSync_UpdateIcon(TRUE, TRUE, shared);
    Expect(g_iconUpdates == 1 && g_iconSnapshot == g_taskbarSnapshot,
           "tray icon did not receive the taskbar snapshot");
    Expect(g_iconSnapshot->revision == 42,
           "snapshot revision changed during dispatch");

    const wchar_t tip[] = L"synchronized";
    Expect(TrayMetricSync_UpdateIconAndTooltip(
               TRUE, TRUE, shared, tip),
           "combined icon and tooltip update failed");
    Expect(g_iconUpdates == 2 && g_iconSnapshot == shared &&
               g_iconTooltip == tip,
           "tooltip did not receive the shared snapshot update");

    g_cpuMemoryEnabled = FALSE;
    g_networkEnabled = TRUE;
    shared = TrayMetricSync_GetSnapshot(TRUE, FALSE, &snapshot);
    Expect(shared == &snapshot,
           "tooltip dispatch did not return a snapshot");
    Expect(g_requestedFields ==
               (SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY |
                SYSTEM_MONITOR_SNAPSHOT_NETWORK),
           "tooltip did not request all displayed metrics");

    g_sampleAvailable = FALSE;
    ZeroMemory(&snapshot, sizeof(snapshot));
    shared = TrayMetricSync_GetSnapshot(TRUE, FALSE, &snapshot);
    Expect(shared && shared->cpuAvailable && shared->memoryAvailable &&
               shared->networkAvailable,
           "transient refresh discarded the last available snapshot");
    Expect(shared->cpuPercent == 37.25f &&
               shared->downloadBytesPerSecond == 2048.0f,
           "transient refresh did not reuse the cached metric values");

    int previousIconUpdates = g_iconUpdates;
    TrayMetricSync_UpdateIcon(TRUE, TRUE, NULL);
    Expect(g_iconUpdates == previousIconUpdates,
           "monitor icon updated without a valid snapshot");

    if (g_failures) {
        fprintf(stderr, "%d tray metric sync test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
