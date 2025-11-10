/**
 * @file time_parser.h
 * @brief Unified time parsing and formatting utilities
 *
 * @details
 * This module provides a centralized time parsing system used across
 * all input dialogs and configuration handling. It supports multiple
 * time input formats and provides consistent formatting.
 */

#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#include <windows.h>
#include <stddef.h>

/* ============================================================================
 * Time Unit Constants
 * ============================================================================ */

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400

/* ============================================================================
 * Core Parsing Functions
 * ============================================================================ */

/**
 * @brief Parse basic time input with units
 *
 * @param input Time string (UTF-8)
 * @param seconds Output seconds
 * @return TRUE on success, FALSE on invalid input
 *
 * @details Supported formats:
 * - With units: "2h3m", "25m", "1h30m15s", "2h 3m", "90s"
 * - No unit: "25" (defaults to minutes)
 * - Units can be: h (hours), m (minutes), s (seconds)
 * - Whitespace is allowed between number and unit
 *
 * @note This is the primary parsing function for dialog inputs
 */
BOOL TimeParser_ParseBasic(const char* input, int* seconds);

/**
 * @brief Parse advanced time input with multiple formats
 *
 * @param input Time string (UTF-8)
 * @param seconds Output seconds
 * @return TRUE on success, FALSE on invalid input
 *
 * @details Supported formats include all basic formats plus:
 * - Absolute time: "14 30t" (countdown to 14:30)
 * - Numeric shorthand: "25" (25 minutes), "130 20" (130 minutes 20 seconds), "1 30 15" (1 hour 30 minutes 15 seconds)
 *
 * @note Used for main countdown dialog with extended features
 */
BOOL TimeParser_ParseAdvanced(const char* input, int* seconds);

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

/**
 * @brief Validate if input string can be parsed
 *
 * @param input Time string to validate (UTF-8)
 * @return TRUE if valid format, FALSE otherwise
 *
 * @details Checks for:
 * - Contains at least one digit
 * - Only contains digits, spaces, tabs, and valid units (h/m/s/t)
 * - No invalid characters
 */
BOOL TimeParser_Validate(const char* input);

/**
 * @brief Check if input contains time units
 *
 * @param input Time string (UTF-8)
 * @return TRUE if contains h/m/s/t units, FALSE otherwise
 */
BOOL TimeParser_HasUnits(const char* input);

/* ============================================================================
 * Formatting Functions
 * ============================================================================ */

/**
 * @brief Format seconds to compact string representation
 *
 * @param seconds Total seconds to format
 * @param buffer Output buffer
 * @param bufferSize Buffer size in bytes
 *
 * @details Output format examples:
 * - 7200 → "2h"
 * - 7260 → "2h1m"
 * - 7265 → "2h1m5s"
 * - 60 → "1m"
 * - 5 → "5s"
 *
 * @note Omits zero components for compact representation
 */
void TimeParser_FormatToString(int seconds, char* buffer, size_t bufferSize);

/**
 * @brief Convert seconds to hours, minutes, seconds components
 *
 * @param seconds Total seconds
 * @param hours Output hours (can be NULL)
 * @param mins Output minutes (can be NULL)
 * @param secs Output seconds (can be NULL)
 *
 * @details Example: 3661 seconds → 1h, 1m, 1s
 */
void TimeParser_FormatToHMS(int seconds, int* hours, int* mins, int* secs);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get multiplier for time unit
 *
 * @param unit Time unit character (h/m/s)
 * @return Multiplier in seconds (3600 for 'h', 60 for 'm', 1 for 's')
 *
 * @details
 * - 'h' or 'H' → 3600 (SECONDS_PER_HOUR)
 * - 'm' or 'M' → 60 (SECONDS_PER_MINUTE)
 * - 's' or 'S' → 1
 * - Other → 0 (invalid)
 */
int TimeParser_GetUnitMultiplier(char unit);

#endif /* TIME_PARSER_H */
