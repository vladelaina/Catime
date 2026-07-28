#include "system_monitor.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

static BOOL PercentIsValid(float value) {
    return value >= 0.0f && value <= 100.0f;
}

static void ExerciseNetworkLifecycle(void) {
    DWORD handlesBefore = 0;
    DWORD handlesAfter = 0;
    BOOL canCountHandles = GetProcessHandleCount(
        GetCurrentProcess(), &handlesBefore);

    for (int cycle = 0; cycle < 16; ++cycle) {
        SystemMonitor_Init();
        SystemMonitor_Init();
        Expect(SystemMonitor_IsInitialized(),
               "monitor failed to initialize during lifecycle stress");

        SystemMonitorSnapshot snapshot = {0};
        Expect(SystemMonitor_GetSnapshot(
                   SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY |
                       SYSTEM_MONITOR_SNAPSHOT_NETWORK,
                   &snapshot),
               "network snapshot failed during lifecycle stress");

        SystemMonitor_Shutdown();
        Expect(!SystemMonitor_IsInitialized(),
               "monitor remained initialized after lifecycle stress shutdown");
        SystemMonitor_Shutdown();
    }

    if (canCountHandles && GetProcessHandleCount(
                               GetCurrentProcess(), &handlesAfter)) {
        Expect(handlesAfter <= handlesBefore + 2,
               "network lifecycle stress leaked process handles");
    }
}

int main(void) {
    SystemMonitor_Init();
    SystemMonitor_SetUpdateIntervalMs(500);

    SystemMonitorSnapshot before = {0};
    Expect(SystemMonitor_GetSnapshot(
               SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY, &before),
           "initial snapshot was unavailable");

    Sleep(600);

    SystemMonitorSnapshot sampled = {0};
    Expect(SystemMonitor_GetSnapshot(
               SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY, &sampled),
           "sampled snapshot was unavailable");
    Expect(sampled.revision != before.revision,
           "snapshot revision did not advance after the refresh interval");
    Expect(sampled.cpuAvailable,
           "CPU sample was unavailable after a complete interval");
    Expect(sampled.memoryAvailable,
           "memory sample was unavailable after a complete interval");
    Expect(PercentIsValid(sampled.cpuPercent),
           "CPU sample was outside 0-100 percent");
    Expect(PercentIsValid(sampled.memoryPercent),
           "memory sample was outside 0-100 percent");

    float cpu = -1.0f;
    float memory = -1.0f;
    Expect(SystemMonitor_GetCpuUsage(&cpu),
           "legacy CPU query failed");
    Expect(SystemMonitor_GetMemoryUsage(&memory),
           "legacy memory query failed");
    Expect(cpu == sampled.cpuPercent,
           "CPU query did not reuse the sampled snapshot");
    Expect(memory == sampled.memoryPercent,
           "memory query did not reuse the sampled snapshot");

    SystemMonitorSnapshot repeated = {0};
    Expect(SystemMonitor_GetSnapshot(
               SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY, &repeated),
           "repeated snapshot was unavailable");
    Expect(repeated.revision == sampled.revision,
           "snapshot refreshed twice inside one interval");
    Expect(repeated.basicSampleTick == sampled.basicSampleTick,
           "CPU and memory snapshot timestamp was not stable");
    Expect(repeated.cpuPercent == sampled.cpuPercent &&
               repeated.memoryPercent == sampled.memoryPercent,
           "repeated snapshot values changed inside one interval");

    SystemMonitor_Shutdown();
    ExerciseNetworkLifecycle();
    if (g_failures) {
        fprintf(stderr, "%d system monitor snapshot test(s) failed\n",
                g_failures);
        return 1;
    }
    return 0;
}
