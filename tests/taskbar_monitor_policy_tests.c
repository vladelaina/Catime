#include "taskbar_monitor/taskbar_monitor_internal.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

static void CheckSnapshotFields(void) {
    Expect(TaskbarMonitor_GetSnapshotFields(FALSE, FALSE) == 0,
           "disabled taskbar preview requested a system snapshot");
    Expect(TaskbarMonitor_GetSnapshotFields(TRUE, FALSE) ==
               SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY,
           "CPU and memory snapshot fields were incorrect");
    Expect(TaskbarMonitor_GetSnapshotFields(FALSE, TRUE) ==
               SYSTEM_MONITOR_SNAPSHOT_NETWORK,
           "network snapshot fields were incorrect");
    Expect(TaskbarMonitor_GetSnapshotFields(TRUE, TRUE) ==
               (SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY |
                SYSTEM_MONITOR_SNAPSHOT_NETWORK),
           "combined snapshot fields were incorrect");
}

static void CheckSystemMonitorRetention(void) {
    Expect(!TaskbarMonitor_ShouldKeepSystemMonitorActive(
               FALSE, FALSE, FALSE, FALSE, FALSE),
           "idle taskbar monitor retained system sampling");
    Expect(TaskbarMonitor_ShouldKeepSystemMonitorActive(
               TRUE, FALSE, FALSE, FALSE, FALSE),
           "enabled taskbar monitor released system sampling");
    Expect(TaskbarMonitor_ShouldKeepSystemMonitorActive(
               FALSE, FALSE, TRUE, TRUE, FALSE),
           "removal preview released the original sampler");
    Expect(TaskbarMonitor_ShouldKeepSystemMonitorActive(
               FALSE, FALSE, TRUE, FALSE, FALSE),
           "menu preview did not prefetch system metrics");
}

static void CheckSnapshotEquality(void) {
    SystemMonitorSnapshot first = {0};
    SystemMonitorSnapshot second = {0};
    Expect(TaskbarMonitor_SnapshotsEqual(&first, &second),
           "identical empty snapshots were not equal");
    first.revision = second.revision = 7;
    first.basicSampleTick = second.basicSampleTick = 100;
    first.cpuPercent = second.cpuPercent = 42.0f;
    first.cpuAvailable = second.cpuAvailable = TRUE;
    Expect(TaskbarMonitor_SnapshotsEqual(&first, &second),
           "identical populated snapshots were not equal");
    second.basicSampleTick = 200;
    Expect(!TaskbarMonitor_SnapshotsEqual(&first, &second),
           "revision reuse hid a restarted monitor snapshot");
    second = first;
    second.cpuPercent = 43.0f;
    Expect(!TaskbarMonitor_SnapshotsEqual(&first, &second),
           "changed snapshot values were treated as duplicates");
    Expect(!TaskbarMonitor_SnapshotsEqual(NULL, &second),
           "null snapshot was treated as equal");
}

int main(void) {
    CheckSnapshotFields();
    CheckSystemMonitorRetention();
    CheckSnapshotEquality();
    return g_failures ? 1 : 0;
}
