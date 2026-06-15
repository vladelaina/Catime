#ifndef TRAY_MENU_SUBMENUS_H
#define TRAY_MENU_SUBMENUS_H

#include <windows.h>
#include <stddef.h>

/**
 * @brief Build timeout action submenu
 * @param hMenu Parent menu handle
 */
void BuildTimeoutActionSubmenu(HMENU hMenu);

/**
 * @brief Build preset management submenu (time options, startup settings, notifications)
 * @param hMenu Parent menu handle
 */
void BuildPresetManagementSubmenu(HMENU hMenu);

/**
 * @brief Build format submenu (time format options)
 * @param hMenu Parent menu handle
 */
void BuildFormatSubmenu(HMENU hMenu);

/**
 * @brief Build color submenu
 * @param hMenu Parent menu handle
 */
void BuildColorSubmenu(HMENU hMenu);

/**
 * @brief Resolve the color captured when the current color submenu was built
 * @param id Menu command identifier
 * @param outColor Optional output buffer for the color string
 * @param outSize Output buffer size
 * @return TRUE when id maps to a color in the current/last built color submenu
 */
BOOL GetColorMenuColorFromId(UINT id, char* outColor, size_t outSize);

/**
 * @brief Build style/appearance submenu
 * @param hMenu Parent menu handle
 */
void BuildStyleSubmenu(HMENU hMenu);

/**
 * @brief Build animation/tray icon submenu
 * @param hMenu Parent menu handle
 */
void BuildAnimationSubmenu(HMENU hMenu);

/**
 * @brief Build plugins submenu
 * @param hMenu Parent menu handle
 */
void BuildPluginsSubmenu(HMENU hMenu);

/**
 * @brief Build help/about submenu
 * @param hMenu Parent menu handle
 */
void BuildHelpSubmenu(HMENU hMenu);

/**
 * @brief Refresh the support item face when the help submenu is about to open
 * @param hMenu Popup menu being initialized
 * @return TRUE when the menu was the help submenu and the item was updated
 */
BOOL UpdateHelpSubmenuSupportFace(HMENU hMenu);

/**
 * @brief Release cached bitmap resources used by tray submenus
 */
void CleanupTraySubmenuResources(void);

#endif // TRAY_MENU_SUBMENUS_H
