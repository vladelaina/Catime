/**
 * @file system_monitor.h
 * @brief Lightweight system performance monitoring (CPU and memory usage)
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <windows.h>

/**
 * @brief Initialize the system monitor module.
 *        Safe to call multiple times; only the first call takes effect.
 */
void SystemMonitor_Init(void);

/**
 * @brief Shutdown the system monitor module and release resources.
 */
void SystemMonitor_Shutdown(void);

/**
 * @brief Set update interval for refreshing cached usage values.
 * @param intervalMs Minimum milliseconds between consecutive refreshes.
 */
void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs);

/**
 * @brief Force refresh of cached CPU and memory usage immediately.
 */
void SystemMonitor_ForceRefresh(void);

/**
 * @brief Get current CPU usage percentage from cache (auto-refresh if stale).
 * @param outPercent Output pointer (0.0f - 100.0f)
 * @return TRUE on success, FALSE if value is not yet available.
 */
BOOL SystemMonitor_GetCpuUsage(float* outPercent);

/**
 * @brief Get current memory usage percentage from cache (auto-refresh if stale).
 * @param outPercent Output pointer (0.0f - 100.0f)
 * @return TRUE on success, FALSE on failure.
 */
BOOL SystemMonitor_GetMemoryUsage(float* outPercent);

/**
 * @brief Convenience API to get both CPU and memory usage (auto-refresh if stale).
 * @param outCpuPercent Output pointer for CPU (0.0f - 100.0f)
 * @param outMemPercent Output pointer for memory (0.0f - 100.0f)
 * @return TRUE if both values are available, FALSE otherwise.
 */
BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent);

/**
 * @brief Get aggregated network speed across active interfaces.
 * @param outUpBytesPerSec Upstream bytes per second (sum of interfaces)
 * @param outDownBytesPerSec Downstream bytes per second (sum of interfaces)
 * @return TRUE on success, FALSE if values not available yet.
 */
BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec);

#endif


