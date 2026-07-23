#ifndef WINDOW_COMMANDS_INTERNAL_H
#define WINDOW_COMMANDS_INTERNAL_H

#include "window_procedure/window_commands.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_message_handlers.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
#include "tray/tray_events.h"
#include "window_procedure/window_events.h"
#include "drag_scale.h"
#include "timer/timer.h"
#include "window.h"
#include "config/config_applier.h"
#include "log.h"
#include "language.h"
#include "startup.h"
#include "notification.h"
#include "font.h"
#include "font/font_path_manager.h"
#include "color/color.h"
#include "pomodoro.h"
#include "tray/tray.h"
#include "drawing/drawing_effect.h"
#include "text_effect.h"
#include "utils/package_identity.h"
#include "utils/finite_double.h"
#include "dialog/dialog_procedure.h"
#include "hotkey.h"
#include "update_checker.h"
#include "async_update_checker.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_menus.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_speed_input.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "menu_preview.h"
#include "main/main_initialization.h"
#include "preview_display.h"
#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_message.h"
#include "../resource/resource.h"
#include "color/color_parser.h"

#include <math.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

extern TextEffectType CLOCK_TEXT_EFFECT;
extern wchar_t inputText[256];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

float ParseDefaultScaleOrFallback(const char* value, float fallback);
void ToggleTextEffect(HWND hwnd, TextEffectType effect);
LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdAbout(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdToggleTopmost(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdEditMode(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdToggleVisibility(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCustomizeColor(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdFontLicense(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdFontAdvanced(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdSystemFontPicker(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdAutoStart(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdColorDialog(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdColorPanel(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdAnimationSpeed(HWND hwnd, AnimationSpeedMetric metric);
LRESULT CmdAnimationFixedSpeed(HWND hwnd);
LRESULT CmdOpenWebsite(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdNotificationContent(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdNotificationDisplay(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdNotificationSettings(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdCheckUpdate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdHotkeySettings(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdHelp(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdSupport(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdVlaina(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdFeedback(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdBrowseFile(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdResetPosition(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp);
BOOL HandleColorSelection(HWND hwnd, UINT cmd, int index);
BOOL HandleRecentFile(HWND hwnd, UINT cmd, int index);
BOOL HandleFontSelection(HWND hwnd, UINT cmd, int index);

#endif
