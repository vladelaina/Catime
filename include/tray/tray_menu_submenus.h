#ifndef TRAY_MENU_SUBMENUS_H
#define TRAY_MENU_SUBMENUS_H

#include <windows.h>

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

#endif // TRAY_MENU_SUBMENUS_H
