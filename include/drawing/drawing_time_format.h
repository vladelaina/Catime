/**
 * @file drawing_time_format.h
 * @brief Time component retrieval and formatting
 */

#ifndef DRAWING_TIME_FORMAT_H
#define DRAWING_TIME_FORMAT_H

#include <windows.h>
#include "../config.h"

/**
 * @brief Time components (simplifies function signatures)
 */
typedef struct {
    int hours;
    int minutes;
    int seconds;
    int centiseconds;
} TimeComponents;

/**
 * Get current system time components
 * @param use24Hour TRUE for 24-hour format, FALSE for 12-hour
 * @return Time components structure
 */
TimeComponents GetCurrentTimeComponents(BOOL use24Hour);

/**
 * Get count-up timer components
 * @return Time components structure
 */
TimeComponents GetCountUpComponents(void);

/**
 * Get countdown timer components (remaining time)
 * @return Time components structure (clamped to zero)
 */
TimeComponents GetCountDownComponents(void);

/**
 * Format time components with adaptive zero-padding for display
 * @param tc Time components to format
 * @param format Zero-padding strategy
 * @param showMilliseconds TRUE to append centiseconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size in characters
 * @note Internal function for drawing module
 */
void FormatTimeComponentsForDisplay(
    const TimeComponents* tc,
    TimeFormatType format,
    BOOL showMilliseconds,
    wchar_t* buffer,
    size_t bufferSize
);

/**
 * Generate display text for current timer mode
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @note Uses preview settings if active, otherwise config values
 */
void GetTimeText(wchar_t* buffer, size_t bufferSize);

#endif /* DRAWING_TIME_FORMAT_H */

