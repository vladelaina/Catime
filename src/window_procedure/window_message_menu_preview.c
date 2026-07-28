/**
 * @file window_message_menu_preview.c
 * @brief Tracks and dispatches menu item previews.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "color/color.h"
#include "log.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "text_effect.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "window_procedure/window_menus.h"
#include "../resource/resource.h"

#define MENU_DEBOUNCE_DELAY_MS 50
#define ANIMATION_PREVIEW_DELAY_MS 30

static UINT g_pendingAnimationPreviewItem = 0;
static UINT g_lastPreviewMenuItem = 0;
static BOOL g_lastWasColorOrFontPreview = FALSE;
static BOOL g_lastWasAnimationPreview = FALSE;
static BOOL g_lastWasTaskbarMonitorPreview = FALSE;
static BOOL g_menuPreviewActive = FALSE;
static BOOL g_menuPreviewCancellationPending = FALSE;

static UINT GetPendingAnimationPreviewItem(void) {
    return g_pendingAnimationPreviewItem;
}

static void ClearPendingMenuPreview(HWND hwnd) {
    KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
    g_pendingAnimationPreviewItem = 0;
}

void WindowMessageInternal_DispatchPendingMenuPreview(HWND hwnd) {
    UINT menuItem = GetPendingAnimationPreviewItem();
    ClearPendingMenuPreview(hwnd);
    if (menuItem != 0 && DispatchMenuPreview(hwnd, menuItem)) {
        g_menuPreviewActive = TRUE;
    }
}

static void ResetMenuPreviewTracking(HWND hwnd) {
    ClearPendingMenuPreview(hwnd);
    g_lastPreviewMenuItem = 0;
    g_lastWasColorOrFontPreview = FALSE;
    g_lastWasAnimationPreview = FALSE;
    g_lastWasTaskbarMonitorPreview = FALSE;
}

static void ScheduleMenuPreviewCancellation(HWND hwnd) {
    ClearPendingMenuPreview(hwnd);
    if (g_menuPreviewActive) {
        g_menuPreviewCancellationPending = TRUE;
    } else {
        ResetMenuPreviewTracking(hwnd);
    }
}

static void CancelActiveMenuPreview(HWND hwnd) {
    if (!g_menuPreviewActive) return;
    g_menuPreviewActive = FALSE;
    BOOL restoreWindowVisibility =
        GetActivePreviewType() != PREVIEW_TYPE_TASKBAR_MONITOR;
    CancelPreview(hwnd);
    if (restoreWindowVisibility) RestoreWindowVisibility(hwnd);
}

void WindowMessageInternal_CancelTrackedMenuPreview(HWND hwnd) {
    if (!g_menuPreviewCancellationPending) return;
    g_menuPreviewCancellationPending = FALSE;
    CancelActiveMenuPreview(hwnd);
    ResetMenuPreviewTracking(hwnd);
}

void StopMenuPreviewTrackingForCommand(HWND hwnd) {
    ResetMenuPreviewTracking(hwnd);
    g_menuPreviewActive = FALSE;
    g_menuPreviewCancellationPending = FALSE;
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
}

static void StartMenuDebounceTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start debounce timer (error=%lu)",
                    GetLastError());
        WindowMessageInternal_CancelTrackedMenuPreview(hwnd);
    }
}

static void StartAnimationPreviewDelayTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY,
                  ANIMATION_PREVIEW_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start preview delay timer (error=%lu)",
                    GetLastError());
        WindowMessageInternal_DispatchPendingMenuPreview(hwnd);
    }
}

static BOOL IsSelectableCommandMenuItem(HMENU hMenu, UINT menuItem) {
    if (!hMenu) {
        return FALSE;
    }

    MENUITEMINFOW itemInfo = {0};
    itemInfo.cbSize = sizeof(itemInfo);
    itemInfo.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
    if (!GetMenuItemInfoW(hMenu, menuItem, FALSE, &itemInfo)) {
        return FALSE;
    }

    if (itemInfo.hSubMenu ||
        (itemInfo.fType & MFT_SEPARATOR) ||
        (itemInfo.fState & (MFS_DISABLED | MFS_GRAYED))) {
        return FALSE;
    }

    return TRUE;
}

static BOOL IsPreviewMenuItem(UINT menuItem, BOOL* isColorOrFontPreview,
                              BOOL* isAnimationPreview,
                              BOOL* isTaskbarMonitorPreview) {
    BOOL colorOrFont = FALSE;
    BOOL animation = FALSE;
    BOOL taskbarMonitor = FALSE;

    if (GetColorMenuColorFromId(menuItem, NULL, 0)) {
        colorOrFont = TRUE;
    }

    if (menuItem >= CMD_FONT_SELECTION_BASE &&
        menuItem < CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES) {
        colorOrFont = TRUE;
    }

    if (menuItem == CLOCK_IDM_TIME_FORMAT_DEFAULT ||
        menuItem == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED ||
        menuItem == CLOCK_IDM_TIME_FORMAT_FULL_PADDED ||
        menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS ||
        menuItem == CLOCK_IDM_SHOW_SECONDS ||
        menuItem == CLOCK_IDM_24HOUR_FORMAT ||
        TextEffect_IsMenuId(menuItem)) {
        colorOrFont = TRUE;
    }

    if (menuItem == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_MEM ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_BATTERY ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_NONE ||
        (menuItem >= CLOCK_IDM_ANIMATIONS_BASE && menuItem < CLOCK_IDM_ANIMATIONS_END)) {
        animation = TRUE;
    }

    if (menuItem == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY ||
        menuItem == CLOCK_IDM_TASKBAR_MONITOR_NETWORK) {
        taskbarMonitor = TRUE;
    }

    if (isColorOrFontPreview) {
        *isColorOrFontPreview = colorOrFont;
    }
    if (isAnimationPreview) {
        *isAnimationPreview = animation;
    }
    if (isTaskbarMonitorPreview) {
        *isTaskbarMonitorPreview = taskbarMonitor;
    }
    return colorOrFont || animation || taskbarMonitor;
}

LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ScheduleMenuPreviewCancellation(hwnd);
    StartMenuDebounceTimer(hwnd);
    return 0;
}

LRESULT HandleInitMenuPopup(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;

    if (HIWORD(lp)) {
        return 0;
    }

    UpdateHelpSubmenuSupportFace((HMENU)wp);
    return 0;
}

LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    UINT menuItem = LOWORD(wp);
    UINT flags = HIWORD(wp);
    HMENU hMenu = (HMENU)lp;

    /* Mouse moved outside any menu item - set debounce timer to cancel preview */
    if (menuItem == 0xFFFF) {
        ScheduleMenuPreviewCancellation(hwnd);
        StartMenuDebounceTimer(hwnd);
        return 0;
    } else {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        g_menuPreviewCancellationPending = FALSE;
    }

    if (hMenu == NULL) return 0;

    if (!(flags & MF_POPUP) && IsSelectableCommandMenuItem(hMenu, menuItem)) {
        BOOL isColorOrFontPreview = FALSE;
        BOOL isAnimationPreview = FALSE;
        BOOL isTaskbarMonitorPreview = FALSE;
        BOOL isPreviewItem = IsPreviewMenuItem(menuItem, &isColorOrFontPreview,
                                               &isAnimationPreview,
                                               &isTaskbarMonitorPreview);

        if (isAnimationPreview != g_lastWasAnimationPreview) {
            if (g_lastWasAnimationPreview && !isAnimationPreview) {
                CancelActiveMenuPreview(hwnd);
            }
        }

        if (isTaskbarMonitorPreview != g_lastWasTaskbarMonitorPreview) {
            if (g_lastWasTaskbarMonitorPreview &&
                !isTaskbarMonitorPreview) {
                CancelActiveMenuPreview(hwnd);
            }
        }

        if (isColorOrFontPreview != g_lastWasColorOrFontPreview) {
            if (g_lastWasColorOrFontPreview && !isColorOrFontPreview) {
                CancelActiveMenuPreview(hwnd);
            }
        }

        if (isColorOrFontPreview) {
            ShowWindowForPreview(hwnd);
        }

        g_lastWasColorOrFontPreview = isColorOrFontPreview;
        g_lastWasAnimationPreview = isAnimationPreview;
        g_lastWasTaskbarMonitorPreview = isTaskbarMonitorPreview;

        if (isPreviewItem && menuItem != g_lastPreviewMenuItem) {
            g_lastPreviewMenuItem = menuItem;

            ClearPendingMenuPreview(hwnd);
            g_pendingAnimationPreviewItem = menuItem;
            StartAnimationPreviewDelayTimer(hwnd);
        } else if (!isPreviewItem) {
            ClearPendingMenuPreview(hwnd);
            g_lastPreviewMenuItem = menuItem;
        }
    } else {
        ClearPendingMenuPreview(hwnd);
        if (g_lastWasColorOrFontPreview) {
            CancelActiveMenuPreview(hwnd);
            g_lastWasColorOrFontPreview = FALSE;
        }
        if (g_lastWasAnimationPreview) {
            CancelActiveMenuPreview(hwnd);
            g_lastWasAnimationPreview = FALSE;
        }
        if (g_lastWasTaskbarMonitorPreview) {
            CancelActiveMenuPreview(hwnd);
            g_lastWasTaskbarMonitorPreview = FALSE;
        }
        g_lastPreviewMenuItem = 0;
    }

    return 0;
}
