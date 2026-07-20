/**
 * @file localized_duration.h
 * @brief Locale-aware human-readable duration formatting.
 */

#ifndef UTILS_LOCALIZED_DURATION_H
#define UTILS_LOCALIZED_DURATION_H

#include <windows.h>
#include <stddef.h>

/**
 * @brief Format seconds using localized hour, minute, and second units.
 *
 * Zero-valued components are omitted. For example, 13920 seconds becomes
 * "3 hours 52 minutes" in English and "3小时52分钟" in Simplified Chinese.
 *
 * @param totalSeconds Duration in seconds; negative values are treated as zero.
 * @param buffer Output buffer.
 * @param bufferCount Output buffer capacity in wchar_t units.
 * @return TRUE when the complete text was written, otherwise FALSE.
 */
BOOL LocalizedDuration_Format(int totalSeconds, wchar_t* buffer,
                              size_t bufferCount);

#endif /* UTILS_LOCALIZED_DURATION_H */
