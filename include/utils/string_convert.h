/**
 * @file string_convert.h
 * @brief Unified UTF-8 ↔ UTF-16 conversion utilities
 * 
 * Centralizes all character encoding conversions to eliminate duplication
 * across 6+ files. Provides both fixed-buffer and dynamic allocation variants.
 */

#ifndef UTILS_STRING_CONVERT_H
#define UTILS_STRING_CONVERT_H

#include <windows.h>
#include <stdbool.h>

/* ============================================================================
 * Fixed-buffer conversions (stack-safe, no allocation)
 * ============================================================================ */

/**
 * @brief Convert UTF-8 to UTF-16 (fixed buffer)
 * @param utf8 Source UTF-8 string
 * @param wide Output buffer
 * @param wideSize Buffer size in wide characters
 * @return TRUE on success, FALSE on error or buffer too small
 */
BOOL Utf8ToWide(const char* utf8, wchar_t* wide, size_t wideSize);

/**
 * @brief Convert UTF-16 to UTF-8 (fixed buffer)
 * @param wide Source UTF-16 string
 * @param utf8 Output buffer
 * @param utf8Size Buffer size in bytes
 * @return TRUE on success, FALSE on error or buffer too small
 */
BOOL WideToUtf8(const wchar_t* wide, char* utf8, size_t utf8Size);

/* ============================================================================
 * Dynamic allocation conversions (caller must free)
 * ============================================================================ */

/**
 * @brief Convert UTF-8 to UTF-16 (allocated)
 * @param utf8 Source UTF-8 string
 * @return Allocated wide string or NULL on error
 * @note Caller must free() the returned pointer
 */
wchar_t* Utf8ToWideAlloc(const char* utf8);

/**
 * @brief Convert UTF-16 to UTF-8 (allocated)
 * @param wide Source UTF-16 string
 * @return Allocated UTF-8 string or NULL on error
 * @note Caller must free() the returned pointer
 */
char* WideToUtf8Alloc(const wchar_t* wide);

/* ============================================================================
 * Convenience wrappers
 * ============================================================================ */

/**
 * @brief Get required buffer size for UTF-8 to UTF-16 conversion
 * @param utf8 Source string
 * @return Required size in wide characters (including null terminator)
 */
size_t Utf8ToWideSize(const char* utf8);

/**
 * @brief Get required buffer size for UTF-16 to UTF-8 conversion
 * @param wide Source string
 * @return Required size in bytes (including null terminator)
 */
size_t WideToUtf8Size(const wchar_t* wide);

#endif /* UTILS_STRING_CONVERT_H */

