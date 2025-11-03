/**
 * @file font_path_manager.h
 * @brief Font path resolution and auto-recovery system
 * 
 * Manages font paths with automatic fixing when users reorganize
 * their fonts folder. Recursively searches for moved fonts.
 */

#ifndef FONT_PATH_MANAGER_H
#define FONT_PATH_MANAGER_H

#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Font folder prefix in config (for relative paths) */
#define FONT_FOLDER_PREFIX "%LOCALAPPDATA%\\Catime\\resources\\fonts\\"

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Complete font path information
 * @note Provides all path variants for validation and recovery
 */
typedef struct {
    char absolutePath[MAX_PATH];    /**< Full filesystem path */
    char relativePath[MAX_PATH];    /**< Relative to fonts folder */
    char configPath[MAX_PATH];      /**< With %LOCALAPPDATA% prefix */
    char fileName[MAX_PATH];        /**< Just the filename */
    BOOL isValid;                   /**< All paths consistent */
} FontPathInfo;

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

/**
 * @brief Get fonts folder path (with auto-creation)
 * @param outW Output buffer (wide character)
 * @param size Buffer size in wide characters
 * @param ensureCreate TRUE to create directory if missing
 * @return TRUE on success
 * 
 * @details Returns %LOCALAPPDATA%\Catime\resources\fonts
 */
BOOL GetFontsFolderW(wchar_t* outW, size_t size, BOOL ensureCreate);

/**
 * @brief Build full absolute path from relative font path
 * @param relativePath Font path relative to fonts folder
 * @param outAbsolutePath Output buffer (UTF-8)
 * @param bufferSize Buffer size
 * @return TRUE on success
 * 
 * @example "subfolder/font.ttf" → "C:\...\fonts\subfolder\font.ttf"
 */
BOOL BuildFullFontPath(const char* relativePath, char* outAbsolutePath, size_t bufferSize);

/**
 * @brief Build config-style path with %LOCALAPPDATA% prefix
 * @param relativePath Font path relative to fonts folder
 * @param outBuffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if path fits buffer
 * 
 * @example "font.ttf" → "%LOCALAPPDATA%\...\fonts\font.ttf"
 */
BOOL BuildFontConfigPath(const char* relativePath, char* outBuffer, size_t bufferSize);

/**
 * @brief Calculate relative path from absolute path
 * @param absolutePath Full filesystem path
 * @param outRelativePath Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if path is within fonts folder, FALSE otherwise
 * 
 * @details Strips fonts folder prefix to get relative path
 */
BOOL CalculateRelativePath(const char* absolutePath, char* outRelativePath, size_t bufferSize);

/* ============================================================================
 * Path Validation
 * ============================================================================ */

/**
 * @brief Check if path uses fonts folder prefix
 * @param path Path to check
 * @return TRUE if starts with %LOCALAPPDATA%\...\fonts\
 */
BOOL IsFontsFolderPath(const char* path);

/**
 * @brief Extract relative portion from config path
 * @param fullConfigPath Full path with prefix
 * @return Pointer to relative portion within string, or NULL
 * 
 * @note Result points into original string, no allocation
 */
const char* ExtractRelativePath(const char* fullConfigPath);

/* ============================================================================
 * Font Search
 * ============================================================================ */

/**
 * @brief Recursively search for font file in fonts folder
 * @param fontFileName Filename to search for
 * @param foundPath Output buffer for full path
 * @param foundPathSize Buffer size
 * @return TRUE if found
 * 
 * @details Case-insensitive search through all subdirectories.
 *          Stops at first match (assumes unique filenames).
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize);

/* ============================================================================
 * Auto-Recovery
 * ============================================================================ */

/**
 * @brief Auto-recover font path after user reorganization
 * @param fontFileName Original filename
 * @param pathInfo Output: all path variants
 * @return TRUE if font found and paths resolved
 * 
 * @details Searches recursively when direct path fails.
 *          Updates pathInfo with all resolved path variants.
 */
BOOL AutoFixFontPath(const char* fontFileName, FontPathInfo* pathInfo);

/**
 * @brief Check and auto-fix font path if file not found
 * @return TRUE if path was fixed, FALSE if no fix needed or failed
 * 
 * @details Validates current FONT_FILE_NAME, searches if missing,
 *          updates config automatically if found.
 * 
 * @note Call periodically (via timer) to handle user file moves
 */
BOOL CheckAndFixFontPath(void);

#endif /* FONT_PATH_MANAGER_H */

