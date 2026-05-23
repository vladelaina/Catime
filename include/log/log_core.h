/**
 * @file log_core.h
 * @brief Core logging functionality (write, rotate, init)
 */

#ifndef LOG_CORE_H
#define LOG_CORE_H

#include <windows.h>

/**
 * Get log file handle (for crash handler)
 * @return HANDLE or INVALID_HANDLE_VALUE
 */
HANDLE GetLogFileHandle(void);

/**
 * Get log critical section (for crash handler)
 * @return Pointer to critical section
 */
CRITICAL_SECTION* GetLogCriticalSection(void);

#endif /* LOG_CORE_H */

