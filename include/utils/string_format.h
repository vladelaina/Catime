/**
 * @file string_format.h
 * @brief String formatting utilities for menu display
 * 
 * Provides common string formatting functions used across menu modules.
 */

#ifndef UTILS_STRING_FORMAT_H
#define UTILS_STRING_FORMAT_H

#include <windows.h>
#include <stddef.h>

/**
 * @brief Truncate long filenames for menu display
 * @param fileName Original filename
 * @param truncated Output buffer
 * @param maxLen Maximum display length
 * 
 * @details
 * Uses middle truncation for very long names: "start...end.ext"
 * Preserves file extension for readability.
 * 
 * @note maxLen should be at least 8 characters for meaningful truncation
 */
void TruncateFileName(const wchar_t* fileName, wchar_t* truncated, size_t maxLen);

/**
 * @brief Format time duration for Pomodoro menu display
 * @param seconds Duration in seconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size in wchar_t units
 * 
 * @details
 * Format varies by duration:
 * - Hours present: "h:mm:ss" (e.g., "1:30:00")
 * - No seconds: "mm" (e.g., "25")
 * - Has seconds: "mm:ss" (e.g., "5:30")
 */
void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize);

#endif /* UTILS_STRING_FORMAT_H */

