#ifndef TRAY_MENU_FONT_H
#define TRAY_MENU_FONT_H

#include <windows.h>

#define FONT_MENU_MAX_ENTRIES 200

/**
 * @brief Build font submenu from cached recursive folder scan
 * @param hMenu Parent menu handle
 */
void BuildFontSubmenu(HMENU hMenu);

/**
 * @brief Reset shutdown state before using the font menu cache
 */
void FontMenu_Initialize(void);

/**
 * @brief Request a background refresh of the font menu cache
 */
void FontMenu_RequestScanAsync(void);

/**
 * @brief Stop background font menu scanning and clear cached state
 */
void FontMenu_Shutdown(void);

/**
 * @brief Get font path from menu ID
 * @param id Menu command ID
 * @param outPath Output buffer for font relative path
 * @param outPathSize Buffer size
 * @return TRUE if found, FALSE otherwise
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize);

#endif // TRAY_MENU_FONT_H
