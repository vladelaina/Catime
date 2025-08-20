/**
 * @file log.h
 * @brief Logging system and error handling
 * 
 * Comprehensive logging with multiple levels and Windows error integration
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <windows.h>
#include <signal.h>

/**
 * @brief Logging severity levels
 */
typedef enum {
    LOG_LEVEL_DEBUG,   /**< Debug information */
    LOG_LEVEL_INFO,    /**< General information */
    LOG_LEVEL_WARNING, /**< Warning messages */
    LOG_LEVEL_ERROR,   /**< Error conditions */
    LOG_LEVEL_FATAL    /**< Fatal errors */
} LogLevel;

/**
 * @brief Initialize logging system
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeLogSystem(void);

/**
 * @brief Write log message with specified level
 * @param level Log severity level
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void WriteLog(LogLevel level, const char* format, ...);

/**
 * @brief Get human-readable description of Windows error code
 * @param errorCode Windows error code
 * @param buffer Buffer to store error description
 * @param bufferSize Size of buffer in bytes
 */
void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

/**
 * @brief Setup exception handler for crash logging
 */
void SetupExceptionHandler(void);

/**
 * @brief Clean up logging system resources
 */
void CleanupLogSystem(void);

/** @brief Convenient debug logging macro */
#define LOG_DEBUG(format, ...) WriteLog(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

/** @brief Convenient info logging macro */
#define LOG_INFO(format, ...) WriteLog(LOG_LEVEL_INFO, format, ##__VA_ARGS__)

/** @brief Convenient warning logging macro */
#define LOG_WARNING(format, ...) WriteLog(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

/** @brief Convenient error logging macro */
#define LOG_ERROR(format, ...) WriteLog(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

/** @brief Convenient fatal error logging macro */
#define LOG_FATAL(format, ...) WriteLog(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

/**
 * @brief Log Windows API error with description
 * 
 * Automatically captures GetLastError() and formats human-readable description
 */
#define LOG_WINDOWS_ERROR(format, ...) do { \
    DWORD errorCode = GetLastError(); \
    char errorMessage[256] = {0}; \
    GetLastErrorDescription(errorCode, errorMessage, sizeof(errorMessage)); \
    WriteLog(LOG_LEVEL_ERROR, format " (Error code: %lu, Description: %s)", ##__VA_ARGS__, errorCode, errorMessage); \
} while(0)

#endif