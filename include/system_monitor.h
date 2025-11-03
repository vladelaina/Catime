/**
 * @file system_monitor.h
 * @brief Lightweight system monitor with caching
 * 
 * Configurable refresh intervals balance accuracy vs overhead (500ms for real-time,
 * 2-5s for background). Atomic initialization ensures thread safety.
 * Network counter overflow handled for long-running sessions (32-bit wraparound).
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <windows.h>

/**
 * @brief Initialize monitor (thread-safe, default 1000ms interval)
 * 
 * @details
 * Sets up CPU baseline and network interface monitoring.
 * Atomic operations ensure safe multi-call.
 * 
 * @note Subsequent calls ignored
 */
void SystemMonitor_Init(void);

/**
 * @brief Shutdown and release resources
 * 
 * @note Safe to call if never initialized
 */
void SystemMonitor_Shutdown(void);

/**
 * @brief Set minimum refresh interval
 * @param intervalMs Interval (0 = default 1000ms)
 * 
 * @details
 * Lower = fresher data but more overhead.
 * Recommended: 500-1000ms (real-time), 2000-5000ms (background).
 */
void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs);

/**
 * @brief Force immediate refresh (bypasses interval)
 * 
 * @details
 * Updates all cached values immediately. Use after startup or when
 * real-time accuracy critical.
 * 
 * @note Brief resource usage spike
 */
void SystemMonitor_ForceRefresh(void);

/**
 * @brief Get CPU usage (auto-refreshes if stale)
 * @param outPercent Output (0.0-100.0)
 * @return TRUE on success
 * 
 * @details
 * Calculated from kernel+user time delta between samples.
 * Auto-initializes if needed.
 * 
 * @note First call may return 0.0 until baseline established
 */
BOOL SystemMonitor_GetCpuUsage(float* outPercent);

/**
 * @brief Get memory usage (auto-refreshes if stale)
 * @param outPercent Output (0.0-100.0)
 * @return TRUE on success
 * 
 * @note Auto-initializes if needed
 */
BOOL SystemMonitor_GetMemoryUsage(float* outPercent);

/**
 * @brief Get CPU and memory in single call (more efficient)
 * @param outCpuPercent CPU output (0.0-100.0)
 * @param outMemPercent Memory output (0.0-100.0)
 * @return TRUE on success
 * 
 * @details Single cache refresh check for both metrics
 */
BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent);

/**
 * @brief Get network speed (all interfaces, excludes loopback)
 * @param outUpBytesPerSec Upload speed output (bytes/sec)
 * @param outDownBytesPerSec Download speed output (bytes/sec)
 * @return TRUE on success
 * 
 * @details
 * Calculates speed from counter delta. Handles 32-bit overflow.
 * First call establishes baseline (returns 0.0).
 * 
 * @note Auto-initializes if needed
 */
BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec);

#endif


