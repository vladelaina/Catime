/**
 * @file string_safe.h
 * @brief Safe string manipulation functions
 * 
 * Provides bounds-checked string operations to prevent buffer overflows.
 */

#ifndef STRING_SAFE_H
#define STRING_SAFE_H

#include <stddef.h>

/**
 * @brief Safe string copy with guaranteed null termination
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Total size of destination buffer (including null terminator)
 * @return Length of copied string on success, -1 if truncated or error
 * 
 * @details Always null-terminates destination. Returns -1 if source was truncated.
 */
int safe_strncpy(char* dest, const char* src, size_t dest_size);

/**
 * @brief Safe string concatenation with guaranteed null termination
 * @param dest Destination buffer (must contain valid null-terminated string)
 * @param src Source string to append
 * @param dest_size Total size of destination buffer (including null terminator)
 * @return New length of string on success, -1 if truncated or error
 * 
 * @details Always null-terminates destination. Returns -1 if source was truncated.
 */
int safe_strncat(char* dest, const char* src, size_t dest_size);

#endif /* STRING_SAFE_H */

