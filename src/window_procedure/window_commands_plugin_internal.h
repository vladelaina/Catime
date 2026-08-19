/**
 * @file window_commands_plugin_internal.h
 * @brief Shared implementation details for plugin and custom-text commands.
 */

#ifndef CATIME_WINDOW_COMMANDS_PLUGIN_INTERNAL_H
#define CATIME_WINDOW_COMMANDS_PLUGIN_INTERNAL_H

#include "window_procedure/window_commands.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_procedure.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_plugin_security.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "audio_player.h"
#include "plugin/plugin_process.h"
#include "log.h"
#include "language.h"
#include "menu_preview.h"
#include "utils/string_convert.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

#if CLOCK_IDM_PLUGINS_BASE + MAX_PLUGINS > CLOCK_IDM_PLUGINS_SETTINGS_BASE
#error "Plugin menu command range overlaps plugin settings command range"
#endif

#define CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID 42431
#define CUSTOM_TEXT_DISPLAY_PREVIEW_DELAY_MS 180
#define CUSTOM_TEXT_DISPLAY_MAX_CHARS 4096
#define CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES (64u * 1024u)
#define CUSTOM_TEXT_DISPLAY_FILENAME_W L"custom_display.txt"
#define CUSTOM_TEXT_DISPLAY_EMPTY_PREVIEW_TEXT_W L"Ciallo～(∠・ω<)⌒★"
#define CUSTOM_TEXT_DISPLAY_EDIT_SUBCLASS_ID 1
#define CUSTOM_TEXT_DISPLAY_EDIT_FONT_MIN_PX 16
#define CUSTOM_TEXT_DISPLAY_EDIT_FONT_MAX_PX 28

typedef struct {
    HWND owner;
    BOOL pluginsStopped;
    BOOL initializing;
    wchar_t contentPath[MAX_PATH];
    wchar_t* originalText;
    HFONT editFont;
} CustomTextDisplayState;

wchar_t* WindowPlugin_DuplicateWideString(const wchar_t* text);
BOOL WindowPlugin_GetCustomTextDisplayPath(wchar_t* buffer, size_t bufferSize);
wchar_t* WindowPlugin_LoadCustomTextDisplayContent(const wchar_t* filePath);
BOOL WindowPlugin_GetCustomTextDisplayText(HWND hwndDlg, wchar_t** outText);
BOOL WindowPlugin_SaveCustomTextDisplayContent(const wchar_t* filePath,
                                               const wchar_t* text);
void WindowPlugin_MoveEditCaretToEnd(HWND hwndEdit);
HFONT WindowPlugin_CreateCustomTextDisplayEditFont(HWND hwndEdit);
void WindowPlugin_StopPluginsForCustomTextDisplay(CustomTextDisplayState* state);
BOOL WindowPlugin_ApplyCustomTextDisplayPreview(HWND hwnd,
                                                const wchar_t* text,
                                                const wchar_t* sourcePath,
                                                BOOL preserveDialogFocus);
BOOL WindowPlugin_QueueCustomTextDisplayPreview(HWND hwndDlg);
BOOL WindowPlugin_ApplyCustomTextDisplayText(HWND hwndDlg,
                                             CustomTextDisplayState* state,
                                             BOOL preserveDialogFocus);
BOOL WindowPlugin_HandleCustomTextDisplay(HWND hwnd);

#endif /* CATIME_WINDOW_COMMANDS_PLUGIN_INTERNAL_H */
