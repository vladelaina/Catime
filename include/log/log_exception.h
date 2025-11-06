/**
 * @file log_exception.h
 * @brief Exception handling and error descriptions
 */

#ifndef LOG_EXCEPTION_H
#define LOG_EXCEPTION_H

#include <windows.h>

/**
 * Setup exception signal handlers
 */
void SetupExceptionHandler(void);

/**
 * Get Windows error description from error code
 * @param errorCode Windows error code
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 */
void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

#endif /* LOG_EXCEPTION_H */

