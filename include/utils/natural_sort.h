/**
 * @file natural_sort.h
 * @brief Natural/numeric-aware string comparison
 * 
 * Handles numeric sequences correctly (e.g., "img2" < "img10" < "img100")
 * Supports both UTF-16 (wchar_t) and UTF-8 (char) strings
 */

#ifndef UTILS_NATURAL_SORT_H
#define UTILS_NATURAL_SORT_H

#include <wchar.h>
#include <stddef.h>

/**
 * @brief Natural comparison for wide strings
 * @param a First string
 * @param b Second string
 * @return -1 if a < b, 0 if equal, 1 if a > b
 * 
 * @details
 * - Case-insensitive
 * - Numeric sequences compared by value, not lexicographically
 * - Leading zeros preserved but don't affect order
 */
int NaturalCompareW(const wchar_t* a, const wchar_t* b);

/**
 * @brief Natural comparison for narrow strings
 * @param a First string (UTF-8 or ASCII)
 * @param b Second string (UTF-8 or ASCII)
 * @return -1 if a < b, 0 if equal, 1 if a > b
 */
int NaturalCompareA(const char* a, const char* b);

/**
 * @brief Natural path comparison with files before folders (wide char)
 * @details At each directory level, files are sorted before subdirectories
 */
int NaturalPathCompareW(const wchar_t* a, const wchar_t* b);

/**
 * @brief Natural path comparison with files before folders (narrow char)
 */
int NaturalPathCompareA(const char* a, const char* b);

#endif /* UTILS_NATURAL_SORT_H */

