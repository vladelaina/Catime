/**
 * @file tray_icon_lifetime.h
 * @brief Lifetime management for icons submitted to Explorer
 */

#ifndef TRAY_ICON_LIFETIME_H
#define TRAY_ICON_LIFETIME_H

#include <windows.h>

/** Transfer ownership of a successfully submitted icon to the lifetime store. */
void TrayIconLifetime_Retain(HICON icon);

/** Release all icons retained after the tray item has been deleted. */
void TrayIconLifetime_ReleaseAll(void);

#endif /* TRAY_ICON_LIFETIME_H */
