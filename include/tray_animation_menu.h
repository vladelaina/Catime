/**
 * @file tray_animation_menu.h
 * @brief Animation menu building and command handling
 * 
 * Recursively scans animations folder and builds hierarchical menu.
 * Maps menu IDs to animation paths for command routing.
 */

#ifndef TRAY_ANIMATION_MENU_H
#define TRAY_ANIMATION_MENU_H

#include <windows.h>

/* Menu ID ranges from tray_menu.h */
#define CLOCK_IDM_ANIMATIONS_MENU 2200
#define CLOCK_IDM_ANIMATIONS_OPEN_DIR 2201
#define CLOCK_IDM_ANIMATIONS_USE_LOGO 2202
#define CLOCK_IDM_ANIMATIONS_USE_CPU 2203
#define CLOCK_IDM_ANIMATIONS_USE_MEM 2204
#define CLOCK_IDM_ANIMATIONS_BASE 3000

/**
 * @brief Build animation submenu
 * @param hMenu Parent menu handle
 * @param currentAnimationName Current animation for checkmark
 * 
 * @details
 * Recursively scans animations folder.
 * Folders first, then files (natural sorted).
 * Leaf folders and files become clickable items.
 * Branch folders become submenus.
 */
void BuildAnimationMenu(HMENU hMenu, const char* currentAnimationName);

/**
 * @brief Handle animation menu command
 * @param hwnd Window handle
 * @param id Menu command ID
 * @return TRUE if handled, FALSE if not animation command
 * 
 * @details
 * Routes menu ID to animation name and activates it.
 * Supports builtin (__logo__, __cpu__, __mem__) and custom animations.
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id);

/**
 * @brief Open animations folder in Explorer
 */
void OpenAnimationsFolder(void);

/**
 * @brief Get animation name from menu ID
 * @param id Menu command ID
 * @param outPath Output buffer for animation name
 * @param outPathSize Buffer size
 * @return TRUE if found, FALSE otherwise
 */
BOOL GetAnimationNameFromMenuId(UINT id, char* outPath, size_t outPathSize);

#endif /* TRAY_ANIMATION_MENU_H */

