/**
 * @file font_manager.h
 * @brief Font loading, preview, and resource management
 * 
 * Core font system providing loading, GDI resource management,
 * and risk-free preview functionality.
 */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <windows.h>

/* ============================================================================
 * Global State (exposed for drawing system)
 * ============================================================================ */

/** @brief Current active font filename (config-style path) */
extern char FONT_FILE_NAME[100];

/** @brief Current active font internal name (from TTF) */
extern char FONT_INTERNAL_NAME[100];

/** @brief Preview font filename */
extern char PREVIEW_FONT_NAME[100];

/** @brief Preview font internal name */
extern char PREVIEW_INTERNAL_NAME[100];

/** @brief Preview mode active flag */
extern BOOL IS_PREVIEWING;

/* ============================================================================
 * Font Resource Management
 * ============================================================================ */

/**
 * @brief Load font file into GDI
 * @param fontFilePath Full absolute path to font file (UTF-8)
 * @return TRUE on success
 * 
 * @details Uses AddFontResourceExW with FR_PRIVATE flag.
 *          Automatically unloads previous font if different.
 *          Skips reload if same font already loaded.
 */
BOOL LoadFontFromFile(const char* fontFilePath);

/**
 * @brief Unload current font resource
 * @return TRUE if unloaded or nothing loaded
 * 
 * @details Call before exit or when switching fonts.
 *          Uses RemoveFontResourceExW.
 */
BOOL UnloadCurrentFontResource(void);

/* ============================================================================
 * High-Level Font Loading
 * ============================================================================ */

/**
 * @brief Load font by filename with auto-recovery
 * @param hInstance Application instance (unused, kept for compatibility)
 * @param fontName Font filename (relative to fonts folder)
 * @return TRUE on success
 * 
 * @details Attempts direct load, then auto-searches if file moved.
 *          Updates config automatically if path fixed.
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Load font and extract internal TTF name
 * @param hInstance Application instance
 * @param fontFileName Font filename (relative)
 * @param realFontName Output buffer for TTF internal name
 * @param realFontNameSize Buffer size
 * @return TRUE on success
 * 
 * @details Parses TTF 'name' table to get family name.
 *          Falls back to filename without extension on parse failure.
 *          Auto-recovers if file moved.
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize);

/**
 * @brief Switch to different font permanently
 * @param hInstance Application instance
 * @param fontName Font filename to switch to
 * @return TRUE on success
 * 
 * @details Updates FONT_FILE_NAME, loads font, extracts internal name,
 *          writes to config (without reload).
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

/* ============================================================================
 * Preview System
 * ============================================================================ */

/**
 * @brief Start font preview (non-destructive)
 * @param hInstance Application instance
 * @param fontName Font to preview
 * @return TRUE on success
 * 
 * @details Loads preview font without affecting active font.
 *          Sets IS_PREVIEWING flag.
 *          Can be cancelled to restore original.
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Cancel preview and restore original font
 * 
 * @details Reloads active font from FONT_FILE_NAME.
 *          Clears preview state.
 */
void CancelFontPreview(void);

/**
 * @brief Apply preview as active font
 * 
 * @details Commits preview font to FONT_FILE_NAME.
 *          Writes to config.
 *          Clears preview state.
 */
void ApplyFontPreview(void);

/* ============================================================================
 * Embedded Font Resources
 * ============================================================================ */

/**
 * @brief Font resource metadata
 */
typedef struct {
    int resourceId;         /**< Resource ID in .rc file */
    const char* fontName;   /**< Filename to extract to */
} FontResource;

/** @brief Embedded font resources list */
extern FontResource fontResources[];

/** @brief Count of embedded fonts */
extern const int FONT_RESOURCES_COUNT;

/**
 * @brief Extract embedded font to file
 * @param hInstance Application instance
 * @param resourceId Resource ID
 * @param outputPath Output file path (UTF-8)
 * @return TRUE on success
 */
BOOL ExtractFontResourceToFile(HINSTANCE hInstance, int resourceId, const char* outputPath);

/**
 * @brief Extract all embedded fonts to fonts folder
 * @param hInstance Application instance
 * @return TRUE if all extracted successfully
 * 
 * @details Extracts to %LOCALAPPDATA%\Catime\resources\fonts
 *          Called on first run.
 */
BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief List all system fonts (debug helper)
 * @note Legacy function, may be removed
 */
void ListAvailableFonts(void);

/**
 * @brief Font enumeration callback
 */
int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, 
                               DWORD FontType, LPARAM lParam);

#endif /* FONT_MANAGER_H */

