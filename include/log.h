/**
 * @file log.h
 * @brief Modular logging system with comprehensive diagnostics and crash handling
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <windows.h>
#include <signal.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum size of log file before rotation (10MB) */
#define LOG_MAX_FILE_SIZE (10 * 1024 * 1024)

/** @brief Number of rotated log files to keep */
#define LOG_ROTATION_COUNT 3

/** @brief UTF-8 BOM for proper file encoding */
#define UTF8_BOM "\xEF\xBB\xBF"

/** @brief ISO 8601 timestamp format for log entries */
#define LOG_TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Logging severity levels with standard syslog-like hierarchy
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

/**
 * @brief OS version information structure for table-driven version detection
 */
typedef struct {
    DWORD major;
    DWORD minor;
    DWORD minBuild;
    const char* name;
} OSVersionInfo;

/**
 * @brief CPU architecture mapping structure
 */
typedef struct {
    WORD archId;
    const char* name;
} CPUArchInfo;

/**
 * @brief Signal information mapping structure
 */
typedef struct {
    int signal;
    const char* description;
} SignalInfo;

/* ============================================================================
 * Public API - Core Log System
 * ============================================================================ */

/**
 * @brief Initialize logging system with diagnostic output
 * 
 * Creates log file, writes system information header, and sets up rotation.
 * Safe to call multiple times (idempotent).
 * 
 * @return TRUE on success, FALSE if log file creation failed
 */
BOOL InitializeLogSystem(void);

/**
 * @brief Write formatted log message with timestamp and level
 * 
 * Thread-safe logging with automatic flushing for crash resilience.
 * Respects minimum log level filtering if configured.
 * 
 * @param level Log severity level
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void WriteLog(LogLevel level, const char* format, ...);

/**
 * @brief Clean shutdown of logging system with final log entry
 * 
 * Writes exit marker, flushes buffers, closes file handle, and cleans up
 * synchronization primitives. Safe to call multiple times.
 */
void CleanupLogSystem(void);

/**
 * @brief Set minimum log level for filtering
 * 
 * Messages below this level will not be written to log file.
 * Useful for reducing log volume in production.
 * 
 * @param minLevel Minimum level to log (DEBUG, INFO, WARNING, ERROR, FATAL)
 */
void SetMinimumLogLevel(LogLevel minLevel);

/**
 * @brief Get current minimum log level
 * @return Current minimum log level filter setting
 */
LogLevel GetMinimumLogLevel(void);

/* ============================================================================
 * Public API - System Diagnostics
 * ============================================================================ */

/**
 * @brief Log operating system version and edition
 * 
 * Uses RtlGetVersion for accurate version detection, bypassing compatibility
 * layer. Handles Windows 2000 through Windows 11+.
 */
void LogOSVersion(void);

/**
 * @brief Log CPU architecture information
 * 
 * Detects x86, x64, ARM, ARM64 architectures with native system info.
 */
void LogCPUArchitecture(void);

/**
 * @brief Log physical memory usage and capacity
 * 
 * Reports total, used, and available physical RAM with percentage.
 */
void LogMemoryInfo(void);

/**
 * @brief Log UAC (User Account Control) status
 * 
 * Detects whether UAC is enabled and current elevation level.
 */
void LogUACStatus(void);

/**
 * @brief Log administrator privilege status
 * 
 * Checks if current process is running with admin rights.
 */
void LogAdminPrivileges(void);

/* ============================================================================
 * Public API - Error Handling
 * ============================================================================ */

/**
 * @brief Convert Windows error code to human-readable UTF-8 description
 * 
 * Zero-allocation error formatting using pre-allocated buffer.
 * Removes trailing CRLF from system messages for cleaner output.
 * 
 * @param errorCode Windows API error code from GetLastError()
 * @param buffer Output buffer for error description
 * @param bufferSize Size of the output buffer in bytes
 */
void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

/**
 * @brief Format bytes to human-readable size string
 * 
 * Converts byte counts to KB/MB/GB with appropriate precision.
 * 
 * @param bytes Number of bytes to format
 * @param buffer Output buffer for formatted string
 * @param bufferSize Size of output buffer
 */
void FormatBytes(ULONGLONG bytes, char* buffer, size_t bufferSize);

/* ============================================================================
 * Public API - Exception Handling
 * ============================================================================ */

/**
 * @brief Setup signal handlers for crash detection and logging
 * 
 * Registers handlers for: SIGFPE, SIGILL, SIGSEGV, SIGTERM, SIGABRT, SIGINT.
 * Handlers use lock-free atomic operations to prevent deadlock.
 */
void SetupExceptionHandler(void);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/** @brief Log debug message (only if DEBUG level enabled) */
#define LOG_DEBUG(format, ...) WriteLog(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

/** @brief Log informational message */
#define LOG_INFO(format, ...) WriteLog(LOG_LEVEL_INFO, format, ##__VA_ARGS__)

/** @brief Log warning message */
#define LOG_WARNING(format, ...) WriteLog(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

/** @brief Log error message */
#define LOG_ERROR(format, ...) WriteLog(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

/** @brief Log fatal error message */
#define LOG_FATAL(format, ...) WriteLog(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

/**
 * @brief Log Windows API error with automatic error code capture
 * 
 * Automatically captures GetLastError() and formats human-readable description.
 * Includes error code and message in a single log entry.
 */
#define LOG_WINDOWS_ERROR(format, ...) do { \
    DWORD errorCode = GetLastError(); \
    char errorMessage[256] = {0}; \
    GetLastErrorDescription(errorCode, errorMessage, sizeof(errorMessage)); \
    WriteLog(LOG_LEVEL_ERROR, format " (Error code: %lu, Description: %s)", \
             ##__VA_ARGS__, errorCode, errorMessage); \
} while(0)

#endif /* LOG_H */
