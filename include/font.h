/**
 * @file font.h
 * @brief Font management module header file
 * 
 * This file defines the application's font management related interfaces, including font loading, setting, and management functionality.
 */

#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

// Forward declaration for font enumeration callback
int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

/// Font resource structure, used to manage font resources
typedef struct {
    int menuId;
    int resourceId;
    const char* fontName;
} FontResource;

/// Font resource array, stores all available font resources
extern FontResource fontResources[];

/// Size of the font resource array
extern const int FONT_RESOURCES_COUNT;

/// Font file name
extern char FONT_FILE_NAME[100];

/// Font internal name
extern char FONT_INTERNAL_NAME[100];

/// Preview font name
extern char PREVIEW_FONT_NAME[100];

/// Preview font internal name
extern char PREVIEW_INTERNAL_NAME[100];

/// Whether font is being previewed
extern BOOL IS_PREVIEWING;

/**
 * @brief Load font from resource
 * @param hInstance Application instance handle
 * @param resourceId Font resource ID
 * @return Whether loading was successful
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId);

/**
 * @brief Load font by name
 * @param hInstance Application instance handle
 * @param fontName Font name
 * @return Whether loading was successful
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Write font configuration to configuration file
 * @param font_file_name Font file name
 */
void WriteConfigFont(const char* font_file_name);

/**
 * @brief List available fonts in the system
 */
void ListAvailableFonts(void);

/**
 * @brief Preview font
 * @param hInstance Application instance handle
 * @param fontName Font name to preview
 * @return Whether preview was successful
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief Cancel font preview
 */
void CancelFontPreview(void);

/**
 * @brief Apply font preview
 */
void ApplyFontPreview(void);

/**
 * @brief Switch font
 * @param hInstance Application instance handle
 * @param fontName Font name to switch to
 * @return Whether switch was successful
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

#endif /* FONT_H */