/**
 * @file system_monitor.h
 * @brief Lightweight system performance monitoring with unified state management
 * 
 * Provides real-time monitoring of system resources including CPU usage,
 * memory usage, and network throughput with automatic caching and refresh control.
 * 
 * Features:
 * - Thread-safe initialization with atomic operations
 * - Configurable refresh intervals to reduce system overhead
 * - Automatic caching with stale detection
 * - Network counter overflow handling for long-running sessions
 * - Unified state management for improved maintainability
 * 
 * @version 2.0 - Refactored for improved modularity and reduced code duplication
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <windows.h>

/**
 * @brief Initialize the system monitor module with default settings.
 * 
 * Sets up internal state, initializes CPU baseline sampling, and prepares
 * network interface monitoring. This function uses atomic operations to
 * ensure thread-safe initialization.
 * 
 * @note Safe to call multiple times; subsequent calls are ignored.
 * @note Default refresh interval is 1000ms (1 second).
 * 
 * @see SystemMonitor_Shutdown
 */
void SystemMonitor_Init(void);

/**
 * @brief Shutdown the system monitor and release all resources.
 * 
 * Resets internal state and releases any allocated resources. After calling
 * this function, SystemMonitor_Init must be called again before using any
 * other functions.
 * 
 * @note Safe to call even if module was never initialized.
 * 
 * @see SystemMonitor_Init
 */
void SystemMonitor_Shutdown(void);

/**
 * @brief Configure the minimum interval between cache refreshes.
 * 
 * Controls how frequently the monitor updates cached values. Lower values
 * provide more up-to-date metrics but increase system overhead. Higher
 * values reduce overhead but may show stale data.
 * 
 * @param intervalMs Minimum milliseconds between refreshes (0 = use default 1000ms)
 * 
 * @note Recommended range: 500ms - 5000ms depending on use case
 * @note Real-time displays should use 500-1000ms
 * @note Background monitoring can use 2000-5000ms
 */
void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs);

/**
 * @brief Force immediate refresh of all cached metrics.
 * 
 * Bypasses the normal refresh interval and updates all cached values
 * (CPU, memory, network) immediately. Useful for getting fresh data
 * after application startup or when real-time accuracy is critical.
 * 
 * @note This may cause a brief spike in system resource usage
 * 
 * @see SystemMonitor_SetUpdateIntervalMs
 */
void SystemMonitor_ForceRefresh(void);

/**
 * @brief Get current CPU usage percentage.
 * 
 * Returns the system-wide CPU usage calculated from the difference
 * between two samples of kernel and user time. Automatically refreshes
 * if cached value is stale.
 * 
 * @param outPercent Output pointer to store CPU usage (0.0f - 100.0f)
 * @return TRUE on success, FALSE if outPercent is NULL
 * 
 * @note First call after initialization may return 0.0f until baseline is established
 * @note Automatically initializes module if not already initialized
 */
BOOL SystemMonitor_GetCpuUsage(float* outPercent);

/**
 * @brief Get current memory usage percentage.
 * 
 * Returns the percentage of physical memory currently in use.
 * Automatically refreshes if cached value is stale.
 * 
 * @param outPercent Output pointer to store memory usage (0.0f - 100.0f)
 * @return TRUE on success, FALSE if outPercent is NULL
 * 
 * @note Automatically initializes module if not already initialized
 */
BOOL SystemMonitor_GetMemoryUsage(float* outPercent);

/**
 * @brief Get both CPU and memory usage in a single call.
 * 
 * Convenience function that retrieves both CPU and memory metrics
 * with a single cache refresh check. More efficient than calling
 * GetCpuUsage and GetMemoryUsage separately.
 * 
 * @param outCpuPercent Output pointer for CPU usage (0.0f - 100.0f)
 * @param outMemPercent Output pointer for memory usage (0.0f - 100.0f)
 * @return TRUE on success, FALSE if any pointer is NULL
 * 
 * @note Automatically initializes module if not already initialized
 */
BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent);

/**
 * @brief Get aggregated network traffic speed across all active interfaces.
 * 
 * Calculates upload and download speeds in bytes per second by sampling
 * network interface counters and computing the delta over time. Excludes
 * loopback interfaces. Handles 32-bit counter overflow gracefully.
 * 
 * @param outUpBytesPerSec Output pointer for upload speed (bytes/second)
 * @param outDownBytesPerSec Output pointer for download speed (bytes/second)
 * @return TRUE on success, FALSE if any pointer is NULL
 * 
 * @note First call establishes baseline; returns 0.0f for both values
 * @note Subsequent calls return speed calculated from previous sample
 * @note Automatically initializes module if not already initialized
 */
BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec);

#endif


