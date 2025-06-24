/**
 * @file log.h
 * @brief Logging functionality header file
 * 
 * Provides logging functionality to record application errors and critical event information
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <windows.h>
#include <signal.h>

// Log level enumeration
typedef enum {
    LOG_LEVEL_DEBUG,   // Debug information
    LOG_LEVEL_INFO,    // General information
    LOG_LEVEL_WARNING, // Warning information
    LOG_LEVEL_ERROR,   // Error information
    LOG_LEVEL_FATAL    // Fatal error
} LogLevel;

/**
 * @brief Initialize the logging system
 * 
 * Sets up the log file path, automatically creates a log directory based on the application configuration file location
 * 
 * @return BOOL Whether initialization was successful
 */
BOOL InitializeLogSystem(void);

/**
 * @brief Write to log
 * 
 * Write log information according to the specified log level
 * 
 * @param level Log level
 * @param format Format string
 * @param ... Variable argument list
 */
void WriteLog(LogLevel level, const char* format, ...);

/**
 * @brief Get description of the last Windows error
 * 
 * @param errorCode Windows error code
 * @param buffer Buffer to store the error description
 * @param bufferSize Buffer size
 */
void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

/**
 * @brief Set up global exception handler
 * 
 * Capture standard C signals (SIGFPE, SIGILL, SIGSEGV, etc.) and handle them
 */
void SetupExceptionHandler(void);

/**
 * @brief Clean up logging system resources
 * 
 * Close log files and release related resources when the program exits normally
 */
void CleanupLogSystem(void);

// Macro definitions for convenience
#define LOG_DEBUG(format, ...) WriteLog(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) WriteLog(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) WriteLog(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) WriteLog(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) WriteLog(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

// Log macro with error code
#define LOG_WINDOWS_ERROR(format, ...) do { \
    DWORD errorCode = GetLastError(); \
    char errorMessage[256] = {0}; \
    GetLastErrorDescription(errorCode, errorMessage, sizeof(errorMessage)); \
    WriteLog(LOG_LEVEL_ERROR, format " (Error code: %lu, Description: %s)", ##__VA_ARGS__, errorCode, errorMessage); \
} while(0)

#endif // LOG_H
