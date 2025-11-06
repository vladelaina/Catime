/**
 * @file log_system_info.h
 * @brief System information collection and logging
 */

#ifndef LOG_SYSTEM_INFO_H
#define LOG_SYSTEM_INFO_H

#include <windows.h>

/**
 * Log operating system version
 */
void LogOSVersion(void);

/**
 * Log CPU architecture
 */
void LogCPUArchitecture(void);

/**
 * Log memory information
 */
void LogMemoryInfo(void);

/**
 * Log UAC status
 */
void LogUACStatus(void);

/**
 * Log administrator privileges
 */
void LogAdminPrivileges(void);

/**
 * Format byte size to human-readable string
 * @param bytes Size in bytes
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 */
void FormatBytes(ULONGLONG bytes, char* buffer, size_t bufferSize);

#endif /* LOG_SYSTEM_INFO_H */

