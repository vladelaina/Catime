/**
 * @file window_menus.c
 * @brief Menu construction and preview dispatch implementation
 */

#include "window_procedure/window_menus.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "menu_preview.h"
#include "font.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "taskbar_monitor.h"
#include "utils/natural_sort.h"
#include "color/color_state.h"
#include "config.h"
#include "text_effect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Large limit for menu display to accommodate folder-based animations with many frames */
#define MAX_SCAN_ENTRIES 4096

extern TextEffectType CLOCK_TEXT_EFFECT;

#include "../resource/resource.h" // Ensure resource constants are available

/* ============================================================================
 * Preview Dispatch
 * ============================================================================ */

BOOL DispatchMenuPreview(HWND hwnd, UINT menuId) {
    if (menuId == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY ||
        menuId == CLOCK_IDM_TASKBAR_MONITOR_NETWORK) {
        TaskbarMonitorOption option =
            menuId == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY
                ? TASKBAR_MONITOR_OPTION_CPU_MEMORY
                : TASKBAR_MONITOR_OPTION_NETWORK;
        return StartPreview(PREVIEW_TYPE_TASKBAR_MONITOR, &option, hwnd);
    }

    /* Handle all animations (builtin + custom) via unified lookup */
    char animName[MAX_PATH];
    if (GetAnimationNameFromMenuId(menuId, animName, sizeof(animName))) {
        return StartPreview(PREVIEW_TYPE_ANIMATION, animName, hwnd);
    }

    if (menuId >= CMD_FONT_SELECTION_BASE &&
        menuId < CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES) {
        char fontPath[MAX_PATH];
        if (GetFontPathFromMenuId(menuId, fontPath, sizeof(fontPath))) {
            return StartPreview(PREVIEW_TYPE_FONT, fontPath, hwnd);
        }
        return FALSE;
    }

    char color[COLOR_HEX_BUFFER];
    if (GetColorMenuColorFromId(menuId, color, sizeof(color))) {
        return StartPreview(PREVIEW_TYPE_COLOR, color, hwnd);
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_DEFAULT) {
        TimeFormatType format = TIME_FORMAT_DEFAULT;
        return StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) {
        TimeFormatType format = TIME_FORMAT_ZERO_PADDED;
        return StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) {
        TimeFormatType format = TIME_FORMAT_FULL_PADDED;
        return StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
        BOOL showMilliseconds = !g_AppConfig.display.time_format.show_milliseconds;
        return StartPreview(PREVIEW_TYPE_MILLISECONDS, &showMilliseconds, hwnd);
    }

    if (menuId == CLOCK_IDM_SHOW_SECONDS) {
        BOOL showSeconds = !CLOCK_SHOW_SECONDS;
        return StartPreview(PREVIEW_TYPE_SECONDS, &showSeconds, hwnd);
    }

    if (menuId == CLOCK_IDM_24HOUR_FORMAT) {
        BOOL use24Hour = !CLOCK_USE_24HOUR;
        return StartPreview(PREVIEW_TYPE_24HOUR, &use24Hour, hwnd);
    }

    EffectType effect = TextEffect_FromMenuId(menuId);
    if (effect != TEXT_EFFECT_NONE) {
        EffectType previewEffect =
            (effect == CLOCK_TEXT_EFFECT) ? TEXT_EFFECT_NONE : effect;
        return StartPreview(PREVIEW_TYPE_EFFECT, &previewEffect, hwnd);
    }

    return FALSE;
}
