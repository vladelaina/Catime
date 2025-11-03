/**
 * @file path_utils.h
 * @brief Cross-platform path manipulation utilities
 * 
 * Provides unified path handling for both UTF-8 and UTF-16 strings.
 * Eliminates duplication across font.c, config_watcher.c, window_procedure.c, etc.
 */

#ifndef UTILS_PATH_UTILS_H
#define UTILS_PATH_UTILS_H

#include <windows.h>
#include <stdbool.h>

/* ============================================================================
 * Filename extraction
 * ============================================================================ */

/**
 * @brief Get filename from path (UTF-8)
 * @param path Full path
 * @return Pointer to filename portion within path, or path if no separator
 * @note Result points into original string, no allocation
 */
const char* GetFileNameU8(const char* path);

/**
 * @brief Get filename from path (UTF-16)
 * @param path Full path
 * @return Pointer to filename portion within path, or path if no separator
 * @note Result points into original string, no allocation
 */
const wchar_t* GetFileNameW(const wchar_t* path);

/**
 * @brief Extract filename to separate buffer (UTF-8)
 * @param path Source path
 * @param name Output buffer
 * @param nameSize Buffer size
 * @return TRUE on success
 */
BOOL ExtractFileNameU8(const char* path, char* name, size_t nameSize);

/**
 * @brief Extract filename to separate buffer (UTF-16)
 * @param path Source path
 * @param name Output buffer
 * @param nameSize Buffer size in wide characters
 * @return TRUE on success
 */
BOOL ExtractFileNameW(const wchar_t* path, wchar_t* name, size_t nameSize);

/* ============================================================================
 * Directory path extraction
 * ============================================================================ */

/**
 * @brief Extract directory path (UTF-8)
 * @param path Full path
 * @param dir Output buffer
 * @param dirSize Buffer size
 * @return TRUE on success
 * @note Result excludes trailing separator
 */
BOOL ExtractDirectoryU8(const char* path, char* dir, size_t dirSize);

/**
 * @brief Extract directory path (UTF-16)
 * @param path Full path
 * @param dir Output buffer
 * @param dirSize Buffer size in wide characters
 * @return TRUE on success
 */
BOOL ExtractDirectoryW(const wchar_t* path, wchar_t* dir, size_t dirSize);

/* ============================================================================
 * Path joining
 * ============================================================================ */

/**
 * @brief Join path components (UTF-8)
 * @param base Base path (modified in-place)
 * @param baseSize Buffer size
 * @param component Component to append
 * @return TRUE on success, FALSE if buffer too small
 * @note Automatically adds separator if needed
 */
BOOL PathJoinU8(char* base, size_t baseSize, const char* component);

/**
 * @brief Join path components (UTF-16)
 * @param base Base path (modified in-place)
 * @param baseSize Buffer size in wide characters
 * @param component Component to append
 * @return TRUE on success
 */
BOOL PathJoinW(wchar_t* base, size_t baseSize, const wchar_t* component);

/* ============================================================================
 * Relative path calculation
 * ============================================================================ */

/**
 * @brief Calculate relative path (UTF-8)
 * @param root Root directory (prefix to strip)
 * @param target Full path
 * @param relative Output buffer
 * @param relativeSize Buffer size
 * @return TRUE if target is under root, FALSE otherwise
 */
BOOL GetRelativePathU8(const char* root, const char* target, 
                       char* relative, size_t relativeSize);

/**
 * @brief Calculate relative path (UTF-16)
 * @param root Root directory
 * @param target Full path
 * @param relative Output buffer
 * @param relativeSize Buffer size in wide characters
 * @return TRUE if target is under root
 */
BOOL GetRelativePathW(const wchar_t* root, const wchar_t* target,
                      wchar_t* relative, size_t relativeSize);

/* ============================================================================
 * Path normalization
 * ============================================================================ */

/**
 * @brief Normalize path separators to backslash (UTF-8)
 * @param path Path to normalize (modified in-place)
 */
void NormalizePathSeparatorsU8(char* path);

/**
 * @brief Normalize path separators to backslash (UTF-16)
 * @param path Path to normalize (modified in-place)
 */
void NormalizePathSeparatorsW(wchar_t* path);

/**
 * @brief Remove trailing separator if present (UTF-8)
 * @param path Path to modify (in-place)
 */
void RemoveTrailingSeparatorU8(char* path);

/**
 * @brief Remove trailing separator if present (UTF-16)
 * @param path Path to modify (in-place)
 */
void RemoveTrailingSeparatorW(wchar_t* path);

/* ============================================================================
 * Path validation
 * ============================================================================ */

/**
 * @brief Check if path starts with given prefix (case-insensitive, UTF-8)
 * @param path Path to check
 * @param prefix Prefix to match
 * @return TRUE if path starts with prefix
 */
BOOL PathStartsWith(const char* path, const char* prefix);

#endif /* UTILS_PATH_UTILS_H */

