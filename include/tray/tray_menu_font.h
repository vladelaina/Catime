#ifndef TRAY_MENU_FONT_H
#define TRAY_MENU_FONT_H

#include <windows.h>

/**
 * @brief Build font submenu with recursive folder scanning
 * @param hMenu Parent menu handle
 */
void BuildFontSubmenu(HMENU hMenu);

/**
 * @brief Get font path from menu ID
 * @param id Menu command ID
 * @param outPath Output buffer for font relative path
 * @param outPathSize Buffer size
 * @return TRUE if found, FALSE otherwise
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize);

#endif // TRAY_MENU_FONT_H
