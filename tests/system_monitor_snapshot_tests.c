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
    if (g_failures) {
        fprintf(stderr, "%d system monitor snapshot test(s) failed\n",
                g_failures);
        return 1;
    }
    return 0;
}
