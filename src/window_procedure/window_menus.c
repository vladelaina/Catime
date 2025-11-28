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
#include "utils/natural_sort.h"
#include "color/color_state.h"
#include "config.h"
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
    if (menuId == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        StartAnimationPreview("__logo__");
        return TRUE;
    }

    if (menuId == CLOCK_IDM_ANIMATIONS_USE_CPU) {
        StartAnimationPreview("__cpu__");
        return TRUE;
    }

    if (menuId == CLOCK_IDM_ANIMATIONS_USE_MEM) {
        StartAnimationPreview("__mem__");
        return TRUE;
    }

    if (menuId == CLOCK_IDM_ANIMATIONS_USE_NONE) {
        StartAnimationPreview("__none__");
        return TRUE;
    }

    if (menuId >= CLOCK_IDM_ANIMATIONS_BASE && menuId < CLOCK_IDM_ANIMATIONS_END) {
        char animName[MAX_PATH];
        if (GetAnimationNameFromMenuId(menuId, animName, sizeof(animName))) {
            StartAnimationPreview(animName);
            return TRUE;
        }
        return FALSE;
    }

    if (menuId >= CMD_FONT_SELECTION_BASE && menuId < CMD_FONT_SELECTION_END) {
        char fontPath[MAX_PATH];
        if (GetFontPathFromMenuId(menuId, fontPath, sizeof(fontPath))) {
            StartPreview(PREVIEW_TYPE_FONT, fontPath, hwnd);
            return TRUE;
        }
        return FALSE;
    }

    int colorIndex = menuId - CMD_COLOR_OPTIONS_BASE;
    if (colorIndex >= 0 && colorIndex < (int)COLOR_OPTIONS_COUNT) {
        StartPreview(PREVIEW_TYPE_COLOR, COLOR_OPTIONS[colorIndex].hexColor, hwnd);
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

    return FALSE;
}

