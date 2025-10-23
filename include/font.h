/**
 * @file font.h
 * @brief Modular font management system with path resolution and TTF parsing
 * 
 * Refactored architecture with separated concerns:
 * - Path resolution and encoding conversion
 * - Font auto-fix and validation
 * - TTF/OTF font name extraction
 * - Font loading and resource management
 * - Preview and configuration management
 */

#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Font folder prefix constant to eliminate magic string repetition */
#define FONT_FOLDER_PREFIX "%LOCALAPPDATA%\\Catime\\resources\\fonts\\"

/** @brief Maximum font name length */
#define MAX_FONT_NAME_LEN 256

/** @brief TTF name table tag (in little-endian: 'name') */
#define TTF_NAME_TABLE_TAG 0x656D616E

/** @brief Name ID for font family name in TTF name table */
#define TTF_NAME_ID_FAMILY 1

/** @brief Safety limit for TTF string data */
#define TTF_STRING_SAFETY_LIMIT 1024

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Font resource information for embedded font extraction
 */
typedef struct {
    int resourceId;        /**< Resource identifier */
    const char* fontName;  /**< Font file name */
} FontResource;

/**
 * @brief Font path information structure
 * 
 * Encapsulates all path-related information to reduce parameter passing
 * and improve code clarity in font path resolution operations
 */
typedef struct {
    char absolutePath[MAX_PATH];  /**< Full absolute path to font file */
    char relativePath[MAX_PATH];  /**< Relative path from fonts folder */
    char configPath[MAX_PATH];    /**< Config format path with %LOCALAPPDATA% prefix */
    char fileName[MAX_PATH];      /**< File name only (without directory) */
    BOOL isValid;                 /**< TRUE if paths are valid and consistent */
} FontPathInfo;

/* ============================================================================
 * Global State
 * ============================================================================ */

/** @brief Array of available font resources */
extern FontResource fontResources[];

/** @brief Count of available font resources */
extern const int FONT_RESOURCES_COUNT;

/** @brief Current font file name */
extern char FONT_FILE_NAME[100];

/** @brief Current font internal name */
extern char FONT_INTERNAL_NAME[100];

/** @brief Preview font file name */
extern char PREVIEW_FONT_NAME[100];

/** @brief Preview font internal name */
extern char PREVIEW_INTERNAL_NAME[100];

/** @brief Font preview active state */
extern BOOL IS_PREVIEWING;

/* ============================================================================
 * Public API - Core Font Operations
 * ============================================================================ */

/**
 * @brief Unload currently loaded font resource
 * @return TRUE if font unloaded successfully or no font was loaded
 */
BOOL UnloadCurrentFontResource(void);

/**
 * @brief Load font from file path with resource management
 * @param fontFilePath Full path to font file
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontFromFile(const char* fontFilePath);

/**
 * @brief Find font file in fonts folder recursively
 * @param fontFileName Font filename to search for
 * @param foundPath Buffer to store found font path
 * @param foundPathSize Size of foundPath buffer
 * @return TRUE if font file found
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize);

/**
 * @brief Load font by file name with auto-fix capability
 * @param hInstance Application instance handle
 * @param fontName Font file name to load
 * @return TRUE on success
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Load font and extract real font name from TTF/OTF file
 * @param hInstance Application instance handle
 * @param fontFileName Font filename to load
 * @param realFontName Buffer to store extracted font name
 * @param realFontNameSize Size of realFontName buffer
 * @return TRUE if font loaded and name extracted
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize);

/* ============================================================================
 * Public API - TTF Font Parsing
 * ============================================================================ */

/**
 * @brief Extract font family name from TTF/OTF font file
 * @param fontFilePath Path to font file
 * @param fontName Buffer to store extracted font name
 * @param fontNameSize Size of fontName buffer
 * @return TRUE if font name extracted successfully
 */
BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize);

/* ============================================================================
 * Public API - Configuration Management
 * ============================================================================ */

/**
 * @brief Write font configuration to settings
 * @param fontFileName Font file name to save
 * @param shouldReload TRUE to reload config after writing
 */
void WriteConfigFont(const char* fontFileName, BOOL shouldReload);

/* ============================================================================
 * Public API - Font Enumeration
 * ============================================================================ */

/**
 * @brief List all available system fonts
 */
void ListAvailableFonts(void);

/**
 * @brief Font enumeration callback procedure
 * @param lpelfe Font enumeration data
 * @param lpntme Text metrics data
 * @param FontType Font type flags
 * @param lParam User-defined parameter
 * @return Callback processing result
 */
int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, 
                               DWORD FontType, LPARAM lParam);

/* ============================================================================
 * Public API - Font Preview System
 * ============================================================================ */

/**
 * @brief Start font preview mode
 * @param hInstance Application instance handle
 * @param fontName Font name to preview
 * @return TRUE on success
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Cancel current font preview and restore original font
 */
void CancelFontPreview(void);

/**
 * @brief Apply current preview font as active font
 */
void ApplyFontPreview(void);

/**
 * @brief Switch to different font permanently
 * @param hInstance Application instance handle
 * @param fontName Font name to switch to
 * @return TRUE on success
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

/* ============================================================================
 * Public API - Embedded Font Resources
 * ============================================================================ */

/**
 * @brief Extract embedded font resource to file
 * @param hInstance Application instance handle
 * @param resourceId Resource ID of the font to extract
 * @param outputPath Full path where to save the font file
 * @return TRUE if font extracted successfully
 */
BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath);

/**
 * @brief Extract all embedded fonts to the fonts directory
 * @param hInstance Application instance handle
 * @return TRUE if all fonts extracted successfully
 */
BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* ============================================================================
 * Public API - Path Validation and Auto-Fix
 * ============================================================================ */

/**
 * @brief Check current font path validity and auto-fix if needed
 * @return TRUE if font path was fixed
 */
BOOL CheckAndFixFontPath(void);

#endif
