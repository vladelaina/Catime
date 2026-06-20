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
#include "utils/natural_sort.h"
#include "color/color_state.h"
#include "config.h"
#include "text_effect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Large limit for menu display to accommodate folder-based animations with many frames */
#define MAX_SCAN_ENTRIES 4096

#include "../resource/resource.h" // Ensure resource constants are available

/* ============================================================================
 * Preview Dispatch
 * ============================================================================ */

BOOL DispatchMenuPreview(HWND hwnd, UINT menuId) {
    /* Handle all animations (builtin + custom) via unified lookup */
    char animName[MAX_PATH];
    if (GetAnimationNameFromMenuId(menuId, animName, sizeof(animName))) {
        StartPreview(PREVIEW_TYPE_ANIMATION, animName, hwnd);
        return TRUE;
    }

    if (menuId >= CMD_FONT_SELECTION_BASE &&
        menuId < CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES) {
        char fontPath[MAX_PATH];
        if (GetFontPathFromMenuId(menuId, fontPath, sizeof(fontPath))) {
            StartPreview(PREVIEW_TYPE_FONT, fontPath, hwnd);
            return TRUE;
        }
        return FALSE;
    }

    char color[COLOR_HEX_BUFFER];
    if (GetColorMenuColorFromId(menuId, color, sizeof(color))) {
        StartPreview(PREVIEW_TYPE_COLOR, color, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_DEFAULT) {
        TimeFormatType format = TIME_FORMAT_DEFAULT;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) {
        TimeFormatType format = TIME_FORMAT_ZERO_PADDED;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) {
        TimeFormatType format = TIME_FORMAT_FULL_PADDED;
        StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, hwnd);
        return TRUE;
    }

    if (menuId == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
        BOOL showMilliseconds = !g_AppConfig.display.time_format.show_milliseconds;
        StartPreview(PREVIEW_TYPE_MILLISECONDS, &showMilliseconds, hwnd);
        return TRUE;
    }

    EffectType effect = TextEffect_FromMenuId(menuId);
    if (effect != TEXT_EFFECT_NONE) {
        StartPreview(PREVIEW_TYPE_EFFECT, &effect, hwnd);
        return TRUE;
    }

    return FALSE;
}

