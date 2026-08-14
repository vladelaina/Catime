/**
 * @file tray_menu_submenus_internal.h
 * @brief Shared dependencies for tray submenu builder modules.
 */

#ifndef CATIME_TRAY_MENU_SUBMENUS_INTERNAL_H
#define CATIME_TRAY_MENU_SUBMENUS_INTERNAL_H

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "language.h"
#include "tray/tray_menu.h"
#include "tray/tray_menu_submenus.h"
#include "font.h"
#include "color/color.h"
#include "window.h"
#include "drag_scale.h"
#include "pomodoro.h"
#include "timer/timer.h"
#include "config.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "../../resource/resource.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_menu.h"
#include "startup.h"
#include "update_checker.h"
#include "utils/string_convert.h"
#include "utils/string_format.h"
#include "utils/package_identity.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "text_effect.h"

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];
extern int current_pomodoro_time_index;
extern POMODORO_PHASE current_pomodoro_phase;
void GetConfigPath(char* path, size_t size);

HBITMAP TraySubmenu_GetUpdateDotBitmap(void);
HBITMAP TraySubmenu_GetSupportHeartBitmap(void);
HBITMAP TraySubmenu_GetVlainaCheckBitmap(void);

#endif /* CATIME_TRAY_MENU_SUBMENUS_INTERNAL_H */
