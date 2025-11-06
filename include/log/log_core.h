/**
 * @file log_core.h
 * @brief Core logging functionality (write, rotate, init)
 */

#ifndef LOG_CORE_H
#define LOG_CORE_H

#include <windows.h>
#include "../log.h"

/**
 * Get log file path (shared with other log modules)
 * @param logPath Output buffer
 * @param size Buffer size
 */
void GetLogFilePath(wchar_t* logPath, size_t size);

/**
 * Get log file handle (for crash handler)
 * @return File pointer or NULL
 */
FILE* GetLogFileHandle(void);

/**
 * Get log critical section (for crash handler)
 * @return Pointer to critical section
 */
CRITICAL_SECTION* GetLogCriticalSection(void);

#endif /* LOG_CORE_H */

