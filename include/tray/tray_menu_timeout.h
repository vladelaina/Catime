/**
 * @file tray_menu_timeout.h
 * @brief Timeout action menu building
 * 
 * Handles timeout action menu items including file/website opening,
 * system actions (shutdown/restart/sleep), and recent files management.
 */

#ifndef TRAY_MENU_TIMEOUT_H
#define TRAY_MENU_TIMEOUT_H

#include <windows.h>

/**
 * @brief Build timeout action submenu
 * @param hParentMenu Parent menu handle to append to
 * 
 * @details
 * Creates submenu with timeout action options:
 * - Show message
 * - Show current time
 * - Count up
 * - Lock screen
 * - Open file/software (with recent files list)
 * - Open website
 * - Shutdown/Restart/Sleep (one-time actions)
 * 
 * Checkmarks indicate currently selected timeout action.
 */
void BuildTimeoutActionMenu(HMENU hParentMenu);

#endif /* TRAY_MENU_TIMEOUT_H */

