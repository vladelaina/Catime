/**
 * @file font.h
 * @brief Font management with TTF parsing and auto-fix capability
 * 
 * TTF parsing extracts real font names to avoid registration mismatches.
 * Auto-fix searches fonts folder when config path is invalid (handles moved configs).
 * Preview system enables risk-free font testing before applying changes.
 */

#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Font folder prefix (centralized to avoid magic string repetition) */
#define FONT_FOLDER_PREFIX "%LOCALAPPDATA%\\Catime\\resources\\fonts\\"

#define MAX_FONT_NAME_LEN 256

/** @brief TTF 'name' table tag (little-endian) */
#define TTF_NAME_TABLE_TAG 0x656D616E

/** @brief Font family name ID in TTF name table */
#define TTF_NAME_ID_FAMILY 1

/** @brief TTF string safety limit (prevents buffer overflows) */
#define TTF_STRING_SAFETY_LIMIT 1024

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Embedded font resource info
 */
typedef struct {
    int resourceId;
    const char* fontName;
} FontResource;

/**
 * @brief Font path resolution result (reduces parameter passing)
 */
typedef struct {
    char absolutePath[MAX_PATH];
    char relativePath[MAX_PATH];
    char configPath[MAX_PATH];    /**< %LOCALAPPDATA% prefix format */
    char fileName[MAX_PATH];
    BOOL isValid;                 /**< All paths consistent */
} FontPathInfo;

/* ============================================================================
 * Global State
 * ============================================================================ */

extern FontResource fontResources[];
extern const int FONT_RESOURCES_COUNT;

extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];

/** @brief Preview state (separate from active font for cancellation) */
extern char PREVIEW_FONT_NAME[100];
extern char PREVIEW_INTERNAL_NAME[100];
extern BOOL IS_PREVIEWING;

/* ============================================================================
 * Public API - Core Font Operations
 * ============================================================================ */

/**
 * @brief Unload current font resource
 * @return TRUE if unloaded or nothing loaded
 */
BOOL UnloadCurrentFontResource(void);

/**
 * @brief Load font from file path
 * @param fontFilePath Full path
 * @return TRUE on success
 */
BOOL LoadFontFromFile(const char* fontFilePath);

/**
 * @brief Find font file recursively in fonts folder
 * @param fontFileName Filename to search
 * @param foundPath Output buffer
 * @param foundPathSize Buffer size
 * @return TRUE if found
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize);

/**
 * @brief Load font with auto-fix (searches folder if path invalid)
 * @param hInstance App instance
 * @param fontName Font filename
 * @return TRUE on success
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Load font and extract real name from TTF
 * @param hInstance App instance
 * @param fontFileName Font filename
 * @param realFontName Output buffer
 * @param realFontNameSize Buffer size
 * @return TRUE if loaded and name extracted
 * 
 * @details Extracts name from TTF to avoid registration mismatches
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize);

/* ============================================================================
 * Public API - TTF Font Parsing
 * ============================================================================ */

/**
 * @brief Extract font family name from TTF/OTF
 * @param fontFilePath Font path
 * @param fontName Output buffer
 * @param fontNameSize Buffer size
 * @return TRUE on success
 */
BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize);

/* ============================================================================
 * Public API - Configuration Management
 * ============================================================================ */

/**
 * @brief Write font to config
 * @param fontFileName Font filename
 * @param shouldReload TRUE to reload after write
 */
void WriteConfigFont(const char* fontFileName, BOOL shouldReload);

/* ============================================================================
 * Public API - Font Enumeration
 * ============================================================================ */

/**
 * @brief List all system fonts
 */
void ListAvailableFonts(void);

/**
 * @brief Font enumeration callback
 */
int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, 
                               DWORD FontType, LPARAM lParam);

/* ============================================================================
 * Public API - Font Preview System
 * ============================================================================ */

/**
 * @brief Start preview mode (enables risk-free testing)
 * @param hInstance App instance
 * @param fontName Font to preview
 * @return TRUE on success
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Cancel preview and restore original
 */
void CancelFontPreview(void);

/**
 * @brief Apply preview as active font
 */
void ApplyFontPreview(void);

/**
 * @brief Switch font permanently
 * @param hInstance App instance
 * @param fontName Font to switch to
 * @return TRUE on success
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

/* ============================================================================
 * Public API - Embedded Font Resources
 * ============================================================================ */

/**
 * @brief Extract embedded font resource to file
 * @param hInstance App instance
 * @param resourceId Resource ID
 * @param outputPath Output path
 * @return TRUE on success
 */
BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath);

/**
 * @brief Extract all embedded fonts to fonts folder
 * @param hInstance App instance
 * @return TRUE if all extracted
 */
BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* ============================================================================
 * Public API - Path Validation and Auto-Fix
 * ============================================================================ */

/**
 * @brief Validate and auto-fix font path if needed
 * @return TRUE if fixed
 * 
 * @details Searches fonts folder when config path invalid (handles moved configs)
 */
BOOL CheckAndFixFontPath(void);

#endif
