/**
 * @file font.h
 * @brief Font management and preview system
 * 
 * Functions for loading, previewing, and managing application fonts
 */

#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

/**
 * @brief Font enumeration callback procedure
 * @param lpelfe Font enumeration data
 * @param lpntme Text metrics data
 * @param FontType Font type flags
 * @param lParam User-defined parameter
 * @return Callback processing result
 */
int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

/**
 * @brief Font resource information structure
 */
typedef struct {
    int menuId;            /**< Menu item identifier */
    int resourceId;        /**< Resource identifier */
    const char* fontName;  /**< Font file name */
} FontResource;

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

/**
 * @brief Load font from application resources
 * @param hInstance Application instance handle
 * @param resourceId Font resource identifier
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId);

/**
 * @brief Load font from file on disk
 * @param fontFilePath Full path to font file
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontFromFile(const char* fontFilePath);

/**
 * @brief Find font file in fonts folder and subfolders
 * @param fontFileName Font filename to search for
 * @param foundPath Buffer to store found font path
 * @param foundPathSize Size of foundPath buffer
 * @return TRUE if font file found, FALSE otherwise
 */
BOOL FindFontInFontsFolder(const char* fontFileName, char* foundPath, size_t foundPathSize);

/**
 * @brief Load font by file name from embedded resources or fonts folder
 * @param hInstance Application instance handle
 * @param fontName Font file name to load
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Read font family name from TTF/OTF font file
 * @param fontFilePath Path to font file
 * @param fontName Buffer to store extracted font name
 * @param fontNameSize Size of fontName buffer
 * @return TRUE if font name extracted successfully, FALSE otherwise
 */
BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize);

/**
 * @brief Load font and get real font name for fonts folder fonts
 * @param hInstance Application instance handle
 * @param fontFileName Font filename to search for
 * @param realFontName Buffer to store real font name
 * @param realFontNameSize Size of realFontName buffer
 * @return TRUE if font found, loaded, and real name extracted, FALSE otherwise
 */
BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, 
                                  char* realFontName, size_t realFontNameSize);

/**
 * @brief Write font configuration to settings
 * @param font_file_name Font file name to save
 */
void WriteConfigFont(const char* font_file_name);

/**
 * @brief List all available system fonts
 */
void ListAvailableFonts(void);

/**
 * @brief Start font preview mode
 * @param hInstance Application instance handle
 * @param fontName Font name to preview
 * @return TRUE on success, FALSE on failure
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Cancel current font preview
 */
void CancelFontPreview(void);

/**
 * @brief Apply current preview font as active font
 */
void ApplyFontPreview(void);

/**
 * @brief Switch to different font immediately
 * @param hInstance Application instance handle
 * @param fontName Font name to switch to
 * @return TRUE on success, FALSE on failure
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

#endif