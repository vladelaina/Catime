/**
 * @file window_message_handlers.c
 * @brief Windows message handlers implementation
 */

#include "window_procedure/window_message_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_menus.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
#include "tray/tray_events.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "config/config_watcher.h"
#include "update/update_internal.h"
#include "dialog/dialog_plugin_security.h"
#include "plugin/plugin_process.h"
#include "audio_player.h"
#include "timer/timer.h"
#include "drag_scale.h"
#include "window_procedure/window_procedure.h"
#include "cli.h"
#include "window.h"
#include "config.h"
#include "color/color.h"
#include "drawing.h"
#include "menu_preview.h"
#include "text_effect.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "../resource/resource.h"
#include "window_procedure/window_drop_target.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "color/gradient.h"
#include "markdown/markdown_interactive.h"
#include "pomodoro.h"
#include "log.h"
#include <stdio.h>
#include <windowsx.h>

/* 50ms menu debounce prevents accidental double-clicks during menu interaction */
#define MENU_DEBOUNCE_DELAY_MS 50
#define ANIMATION_PREVIEW_DELAY_MS 30
#define BUFFER_SIZE_CLI_INPUT 256
#define BUFFER_SIZE_MENU_ITEM 100
#define IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD 42427
#define EDIT_EXIT_RIGHT_CLICK_SHIELD_MS 250u

UINT GetPendingAnimationPreviewItem(void);
static void ClearPendingMenuPreview(HWND hwnd);
static void DispatchPendingMenuPreview(HWND hwnd);
static void ResetMenuPreviewTracking(HWND hwnd);
static void StartMenuDebounceTimer(HWND hwnd);
static void StartAnimationPreviewDelayTimer(HWND hwnd);

extern UINT WM_TASKBARCREATED;
extern BOOL CLOCK_EDIT_MODE;

static BOOL g_pendingEditExitRightClick = FALSE;
static DWORD g_suppressContextMenuUntilTick = 0;
static DWORD g_editExitRightClickShieldUntilTick = 0;

static BOOL IsTickActive(DWORD untilTick) {
    if (untilTick == 0) {
        return FALSE;
    }
    return (LONG)(GetTickCount() - untilTick) < 0;
}

static void SuppressContextMenuBriefly(void) {
    DWORD until = GetTickCount() + 500u;
    g_suppressContextMenuUntilTick = until ? until : 1u;
}

static BOOL IsContextMenuSuppressed(void) {
    if (g_pendingEditExitRightClick) {
        return TRUE;
    }

    if (g_suppressContextMenuUntilTick == 0) {
        return FALSE;
    }

    if (IsTickActive(g_suppressContextMenuUntilTick)) {
        return TRUE;
    }

    g_suppressContextMenuUntilTick = 0;
    return FALSE;
}

BOOL IsEditExitRightClickShieldActive(void) {
    if (g_pendingEditExitRightClick) {
        return TRUE;
    }

    if (IsTickActive(g_editExitRightClickShieldUntilTick)) {
        return TRUE;
    }

    g_editExitRightClickShieldUntilTick = 0;
    return FALSE;
}

static void StartEditExitRightClickShield(HWND hwnd) {
    DWORD until = GetTickCount() + EDIT_EXIT_RIGHT_CLICK_SHIELD_MS;
    g_editExitRightClickShieldUntilTick = until ? until : 1u;
    SetClickThrough(hwnd, FALSE);
    if (!SetTimer(hwnd,
                  IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD,
                  EDIT_EXIT_RIGHT_CLICK_SHIELD_MS,
                  NULL)) {
        g_editExitRightClickShieldUntilTick = 0;
        if (!CLOCK_EDIT_MODE) {
            SetClickThrough(hwnd, TRUE);
        }
        LOG_WARNING("Failed to start edit-exit right-click shield timer (error=%lu)",
                    GetLastError());
    }
}

static void StopEditExitRightClickShield(HWND hwnd) {
    KillTimer(hwnd, IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD);
    g_editExitRightClickShieldUntilTick = 0;
    if (!CLOCK_EDIT_MODE) {
        SetClickThrough(hwnd, TRUE);
    }
}

static void ResetEditExitRightClickState(HWND hwnd) {
    KillTimer(hwnd, IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD);
    g_pendingEditExitRightClick = FALSE;
    g_suppressContextMenuUntilTick = 0;
    g_editExitRightClickShieldUntilTick = 0;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static void ClearPendingEditExitRightClick(HWND hwnd) {
    g_pendingEditExitRightClick = FALSE;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static int HexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static BOOL TryParseHexColorRef(const char* color, COLORREF* out) {
    if (!color || !out || color[0] != '#' || strlen(color) != 7) {
        return FALSE;
    }

    int channels[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        int hi = HexDigitValue(color[1 + i * 2]);
        int lo = HexDigitValue(color[2 + i * 2]);
        if (hi < 0 || lo < 0) {
            return FALSE;
        }
        channels[i] = hi * 16 + lo;
    }

    *out = RGB(channels[0], channels[1], channels[2]);
    return TRUE;
}

/* ============================================================================
 * Message Handler Implementations
 * ============================================================================ */

LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    HandleWindowCreate(hwnd);
    ConfigWatcher_Start(hwnd);
    return 0;
}

LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;

    /* In non-edit mode, show hand cursor for clickable regions */
    if (!CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT && HasClickableRegions()) {
        POINT pt;
        GetCursorPos(&pt);

        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);

        if (IsClickableRegionAt(pt)) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
    }

    if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    if (LOWORD(lp) == HTCLIENT || lp == CLOCK_WM_TRAYICON) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    return DefWindowProc(hwnd, WM_SETCURSOR, wp, lp);
}

LRESULT HandleLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;

    /* In non-edit mode, check for clickable region clicks */
    if (!CLOCK_EDIT_MODE && HasClickableRegions()) {
        POINT pt;
        GetCursorPos(&pt);

        /* Update region positions */
        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);

        if (HandleRegionClickAt(pt, hwnd)) {
            return 0;
        }
    }

    StartDragWindow(hwnd);
    return 0;
}

LRESULT HandleLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    EndDragWindow(hwnd);
    return 0;
}

LRESULT HandleMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    HandleScaleWindow(hwnd, delta);
    return 0;
}

LRESULT HandleMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    TryStartDragWindowFromMouseMove(hwnd);
    if (HandleDragWindow(hwnd)) return 0;
    return DefWindowProc(hwnd, WM_MOUSEMOVE, wp, lp);
}

LRESULT HandlePaint(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    HandleWindowPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT HandleEraseBkgnd(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)wp;
    (void)lp;
    return 1;
}

LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == TIMER_ID_DISPLAY_RESTORE) {
        KillTimer(hwnd, TIMER_ID_DISPLAY_RESTORE);
        RestoreWindowPositionAfterSystemChange(hwnd);
        return 0;
    }
    if (wp == IDT_MENU_DEBOUNCE) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
        return 0;
    }
    if (wp == IDT_ANIMATION_PREVIEW_DELAY) {
        DispatchPendingMenuPreview(hwnd);
        return 0;
    }
    if (wp == IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD) {
        StopEditExitRightClickShield(hwnd);
        return 0;
    }
    /* Handle click-through timer for dynamic WS_EX_TRANSPARENT switching */
    if (wp == GetClickThroughTimerId()) {
        UpdateClickThroughState(hwnd);
        return 0;
    }
    HandleTimerEvent(hwnd, wp);
    return 0;
}

/**
 * @brief Handle high-precision multimedia timer tick
 * Called from worker thread via PostMessage for smooth milliseconds display
 */
LRESULT HandleMainTimerTick(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    LONG generation = (LONG)wp;
    if (MainTimer_ShouldHandleTick(generation)) {
        /* Delegate to main timer event handler */
        HandleTimerEvent(hwnd, TIMER_ID_MAIN);
    }
    /* Release coalescing gate for the next high-precision tick message. */
    MainTimer_NotifyTickHandled(generation);
    return 0;
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ResetEditExitRightClickState(hwnd);
    StopMenuPreviewTrackingForCommand(hwnd);
    CancelPreview(hwnd);
    UnregisterGlobalHotkeys(hwnd);
    HandleWindowDestroy(hwnd);
    ConfigWatcher_Stop();

    return 0;
}

LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
    return 0;
}

LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    const WINDOWPOS* pwp = (const WINDOWPOS*)lp;
    HandleTopmostVisibilityChange(hwnd, pwp);
    if (!(pwp->flags & SWP_NOSIZE)) {
        if (CLOCK_EDIT_MODE) {
            // Region update logic removed - relying on UpdateLayeredWindow alpha channel
        }
    }
    return 0;
}

LRESULT HandleShowWindow(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp) {
        HandleTopmostShownEvent(hwnd);
    } else {
        HandleTopmostHiddenEvent(hwnd);
    }
    return DefWindowProc(hwnd, WM_SHOWWINDOW, wp, lp);
}

LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (!BeginSystemPositionChangeGuard(hwnd)) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
}

LRESULT HandleDpiChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    BOOL restoreScheduled = BeginSystemPositionChangeGuard(hwnd);

    RECT* suggested = (RECT*)lp;
    if (suggested) {
        SetWindowPos(hwnd, NULL,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (!restoreScheduled) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (g_pendingEditExitRightClick) {
        ClearPendingEditExitRightClick(hwnd);
        if (CLOCK_EDIT_MODE) {
            EndEditMode(hwnd);
        }
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONUP, wp, lp);
}

LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) {
        g_pendingEditExitRightClick = TRUE;
        SetCapture(hwnd);
        return 0;
    }

    ClearPendingEditExitRightClick(hwnd);

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        ToggleEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONDOWN, wp, lp);
}

LRESULT HandleContextMenu(HWND hwnd, WPARAM wp, LPARAM lp) {
    BOOL suppressed = CLOCK_EDIT_MODE || IsContextMenuSuppressed();
    if (suppressed) {
        return 0;
    }
    return DefWindowProc(hwnd, WM_CONTEXTMENU, wp, lp);
}

LRESULT HandleCaptureChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    if ((HWND)lp != hwnd && g_pendingEditExitRightClick) {
        g_pendingEditExitRightClick = FALSE;
    }
    return DefWindowProc(hwnd, WM_CAPTURECHANGED, wp, lp);
}

LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ResetMenuPreviewTracking(hwnd);
    StartMenuDebounceTimer(hwnd);
    return 0;
}

LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    SaveWindowSettings(hwnd);
    HideWindowIntentionally(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

LRESULT HandleSysCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (HandleTopmostMinimizeCommand(hwnd, (UINT)wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SYSCOMMAND, wp, lp);
}

LRESULT HandleSize(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (HandleTopmostSizeEvent(hwnd, wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SIZE, wp, lp);
}

LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (!CLOCK_EDIT_MODE) {
        StartEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_LBUTTONDBLCLK, wp, lp);
}

LRESULT HandleKeyDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (CLOCK_EDIT_MODE) {
        /* Only process arrow keys in edit mode */
        if (wp != VK_UP && wp != VK_DOWN && wp != VK_LEFT && wp != VK_RIGHT) {
            return DefWindowProc(hwnd, WM_KEYDOWN, wp, lp);
        }

        int step = g_AppConfig.display.move_step_small;
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            step = g_AppConfig.display.move_step_large;
        }

        /* Check all arrow keys to support diagonal movement */
        int dx = 0;
        int dy = 0;

        if (GetKeyState(VK_UP) & 0x8000) {
            dy -= step;
        }
        if (GetKeyState(VK_DOWN) & 0x8000) {
            dy += step;
        }
        if (GetKeyState(VK_LEFT) & 0x8000) {
            dx -= step;
        }
        if (GetKeyState(VK_RIGHT) & 0x8000) {
            dx += step;
        }

        if (dx != 0 || dy != 0) {
            RECT rect;
            GetWindowRect(hwnd, &rect);
            SetWindowPos(hwnd, NULL, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            CLOCK_WINDOW_POS_X = rect.left + dx;
            CLOCK_WINDOW_POS_Y = rect.top + dy;
            ScheduleConfigSave(hwnd);
            return 0;
        }
    }
    return DefWindowProc(hwnd, WM_KEYDOWN, wp, lp);
}

LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (DispatchHotkey(hwnd, (int)wp)) return 0;
    return DefWindowProc(hwnd, WM_HOTKEY, wp, lp);
}

LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;

    if (!pcds || !pcds->lpData || pcds->cbData == 0) {
        return DefWindowProc(hwnd, WM_COPYDATA, wp, lp);
    }

    // Handle CLI text
    if (pcds->dwData == COPYDATA_ID_CLI_TEXT) {
        const size_t maxLen = BUFFER_SIZE_CLI_INPUT - 1;
        char buf[BUFFER_SIZE_CLI_INPUT];
        size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
        memcpy(buf, pcds->lpData, n);
        buf[maxLen] = '\0';
        buf[n] = '\0';
        HandleCliArguments(hwnd, buf);
        return TRUE;
    }

    return DefWindowProc(hwnd, WM_COPYDATA, wp, lp);
}

LRESULT HandleQuickCountdownIndex(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    int idx = (int)lp;
    if (idx >= 1) {
        StartQuickCountdownByIndex(hwnd, idx);
    } else {
        StartDefaultCountDown(hwnd);
    }
    return 0;
}

LRESULT HandleShowCliHelp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowCliHelpDialog(hwnd);
    return 0;
}

LRESULT HandleTrayUpdateIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    if (TrayAnimation_HandleUpdateMessage(hwnd)) return 0;
    return DefWindowProc(hwnd, WM_USER + 100, wp, lp);
}

LRESULT HandleAppReregisterHotkeys(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

LRESULT HandleAnimationPreviewLoaded(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    /* Update tray icon after preview animation is loaded */
    TrayAnimation_HandleUpdateMessage(hwnd);
    return 0;
}

LRESULT HandleMeasureItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp;
    LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
    if (lpmis->CtlType == ODT_MENU) {
        lpmis->itemHeight = 25;
        lpmis->itemWidth = BUFFER_SIZE_MENU_ITEM;
        return TRUE;
    }
    return FALSE;
}

LRESULT HandleDrawItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp;
    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
    if (lpdis->CtlType != ODT_MENU) return FALSE;

    char hexColor[COLOR_HEX_BUFFER];
    if (!GetColorMenuColorFromId(lpdis->itemID, hexColor, sizeof(hexColor))) {
        return FALSE;
    }

    GradientInfoSnapshot gradientSnapshot;
    GradientType gradientType = GetGradientInfoSnapshotByName(hexColor, &gradientSnapshot);

    /* Draw color/gradient with space for sequence number */
    RECT colorRect = lpdis->rcItem;
    colorRect.left += 28;  /* Leave space for number */

    if (gradientType != GRADIENT_NONE) {
        DrawGradientRect(lpdis->hDC, &colorRect, &gradientSnapshot.info);
    } else {
        COLORREF color = RGB(255, 255, 255);
        TryParseHexColorRef(hexColor, &color);

        HGDIOBJ hBrush = GetStockObject(DC_BRUSH);
        HGDIOBJ hPen = GetStockObject(DC_PEN);
        if (!hBrush || !hPen) return FALSE;

        COLORREF oldBrushColor = SetDCBrushColor(lpdis->hDC, color);
        COLORREF oldPenColor = SetDCPenColor(lpdis->hDC, RGB(200, 200, 200));
        HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
        HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);

        Rectangle(lpdis->hDC, colorRect.left, colorRect.top,
                 colorRect.right, colorRect.bottom);

        if (oldPen) SelectObject(lpdis->hDC, oldPen);
        if (oldBrush) SelectObject(lpdis->hDC, oldBrush);
        if (oldPenColor != CLR_INVALID) SetDCPenColor(lpdis->hDC, oldPenColor);
        if (oldBrushColor != CLR_INVALID) SetDCBrushColor(lpdis->hDC, oldBrushColor);
    }

    /* Draw sequence number on left side */
    wchar_t numStr[8];
    _snwprintf_s(numStr, 8, _TRUNCATE, L"%u",
                 (unsigned int)(lpdis->itemID - CMD_COLOR_OPTIONS_BASE + 1));
    RECT numRect = lpdis->rcItem;
    numRect.right = numRect.left + 26;
    int oldBkMode = SetBkMode(lpdis->hDC, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(lpdis->hDC, GetSysColor(COLOR_MENUTEXT));
    DrawTextW(lpdis->hDC, numStr, -1, &numRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (lpdis->itemState & ODS_SELECTED) {
        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
    }

    if (oldTextColor != CLR_INVALID) {
        SetTextColor(lpdis->hDC, oldTextColor);
    }
    if (oldBkMode != 0) {
        SetBkMode(lpdis->hDC, oldBkMode);
    }

    return TRUE;
}

static UINT g_pendingAnimationPreviewItem = 0;
static UINT g_lastPreviewMenuItem = 0;
static BOOL g_lastWasColorOrFontPreview = FALSE;
static BOOL g_lastWasAnimationPreview = FALSE;

UINT GetPendingAnimationPreviewItem(void) {
    return g_pendingAnimationPreviewItem;
}

static void ClearPendingMenuPreview(HWND hwnd) {
    KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
    g_pendingAnimationPreviewItem = 0;
}

static void DispatchPendingMenuPreview(HWND hwnd) {
    UINT menuItem = GetPendingAnimationPreviewItem();
    ClearPendingMenuPreview(hwnd);
    if (menuItem != 0) {
        DispatchMenuPreview(hwnd, menuItem);
    }
}

static void ResetMenuPreviewTracking(HWND hwnd) {
    ClearPendingMenuPreview(hwnd);
    g_lastPreviewMenuItem = 0;
    g_lastWasColorOrFontPreview = FALSE;
    g_lastWasAnimationPreview = FALSE;
}

void StopMenuPreviewTrackingForCommand(HWND hwnd) {
    ResetMenuPreviewTracking(hwnd);
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
}

static void StartMenuDebounceTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start debounce timer (error=%lu)",
                    GetLastError());
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
    }
}

static void StartAnimationPreviewDelayTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY,
                  ANIMATION_PREVIEW_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start preview delay timer (error=%lu)",
                    GetLastError());
        DispatchPendingMenuPreview(hwnd);
    }
}

static BOOL IsPreviewMenuItem(UINT menuItem, BOOL* isColorOrFontPreview,
                              BOOL* isAnimationPreview) {
    BOOL colorOrFont = FALSE;
    BOOL animation = FALSE;

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

    if (isColorOrFontPreview) {
        *isColorOrFontPreview = colorOrFont;
    }
    if (isAnimationPreview) {
        *isAnimationPreview = animation;
    }
    return colorOrFont || animation;
}

/* ============================================================================
 * Modeless Dialog Result Handlers
 * ============================================================================ */

/**
 * @brief Handle countdown dialog result
 * @param wp Countdown time in seconds (0 = cancelled)
 * @param lp Reserved
 */
LRESULT HandleDialogCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    int seconds = (int)wp;
    if (seconds > 0) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, seconds);
    }
    return 0;
}

/**
 * @brief Handle shortcut time dialog result
 * @param wp Reserved
 * @param lp Reserved
 * @note Config is saved by dialog, just need to acknowledge
 */
LRESULT HandleDialogShortcut(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    /* Shortcut time options already saved by dialog */
    return 0;
}

/**
 * @brief Handle color dialog result
 * @param wp 0 = cancelled, 1 = color applied
 * @param lp Reserved
 */
LRESULT HandleDialogColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == 0) {
        /* Color cancelled - restore preview if needed */
        CancelPreview(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

/**
 * @brief Handle update dialog result
 * @param wp IDYES = update now, IDNO = later
 * @param lp Reserved
 */
LRESULT HandleDialogUpdate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDYES) {
        /* User chose to update - trigger download and exit */
        TriggerUpdateDownload(hwnd);
    }
    return 0;
}

/**
 * @brief Handle update check result from background thread
 * @param wp 1 = update available, 0 = no update
 * @param lp Reserved
 */
LRESULT HandleUpdateCheckResult(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (wp == 1) {
        ShowStoredUpdateDialog(hwnd);
    } else {
        if (lp == 0) {
            ShowStoredNoUpdateDialog(hwnd);
        }
    }
    return 0;
}

/**
 * @brief Handle font license dialog result
 * @param wp IDOK = accepted, IDCANCEL = rejected
 * @param lp Reserved
 */
LRESULT HandleDialogFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDOK) {
        SetFontLicenseAccepted(TRUE);
        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

/**
 * @brief Handle plugin security dialog result
 * @param wp IDYES = trust and run, IDOK = run once, IDCANCEL = cancelled
 * @param lp Reserved
 */
LRESULT HandleDialogPluginSecurity(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;

    int pluginIndex = GetPendingPluginIndex();
    const char* pendingPluginPath = GetPendingPluginPath();

    if (wp == IDCANCEL) {
        /* User cancelled - just clear pending info, don't change display state */
        ClearPendingPluginInfo();
        return 0;
    }
    if ((wp != IDYES && wp != IDOK) ||
        pluginIndex < 0 ||
        !pendingPluginPath ||
        pendingPluginPath[0] == '\0') {
        LOG_WARNING("Ignoring stale plugin security dialog result");
        ClearPendingPluginInfo();
        return 0;
    }

    /* User confirmed - now change state and start plugin */

    /* Stop notification sound */
    StopNotificationSound();

    /* Prevent countdown completion notification from triggering */
    countdown_message_shown = true;

    /* Reset timer flags */
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;

    /* Stop internal timer */
    MainTimer_Stop();

    /* Reset Pomodoro if active */
    current_pomodoro_phase = POMODORO_PHASE_IDLE;

    /* Reset timer values */
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    /* Show loading message */
    PluginInfo pluginInfo;
    if (PluginManager_CopyPlugin(pluginIndex, &pluginInfo)) {
        wchar_t loadingText[256];
        PluginData_SetOutputDirectoryFromPluginPath(pluginInfo.path);
        _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", pluginInfo.displayName);
        PluginData_SetText(loadingText);
        PluginData_SetActive(TRUE);
    }

    /* IDYES = Trust & Run, IDOK = Run Once */
    BOOL trustPlugin = (wp == IDYES);
    BOOL startResult = PluginManager_StartPluginAfterSecurityCheck(pluginIndex, trustPlugin);

    if (!startResult) {
        /* Failed to start after security check - show specific error */
        const wchar_t* errorMsg = PluginProcess_GetLastError();
        if (errorMsg && errorMsg[0] != L'\0') {
            PluginData_SetStatusText(errorMsg);
        } else {
            PluginData_SetStatusText(L"FAIL");
        }
    }

    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);  /* 15 FPS for smooth animation */
    }

    /* Re-apply visibility/topmost policy to recover from any z-order drift */
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);

    ClearPendingPluginInfo();

    return 0;
}

/**
 * @brief Handle plugin hot-reload request from background thread
 * @param wp Hot-reload request generation
 * @param lp Reserved
 */
LRESULT HandlePluginHotReload(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)lp;

    PluginManager_RestartPendingHotReload((LONG)(LONG_PTR)wp);

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
        ResetMenuPreviewTracking(hwnd);
        StartMenuDebounceTimer(hwnd);
        return 0;
    } else {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    }

    if (hMenu == NULL) return 0;

    if (!(flags & MF_POPUP)) {
        BOOL isColorOrFontPreview = FALSE;
        BOOL isAnimationPreview = FALSE;
        BOOL isPreviewItem = IsPreviewMenuItem(menuItem, &isColorOrFontPreview,
                                               &isAnimationPreview);

        if (isAnimationPreview != g_lastWasAnimationPreview) {
            if (g_lastWasAnimationPreview && !isAnimationPreview) {
                CancelPreview(hwnd);
            }
        }

        if (isColorOrFontPreview != g_lastWasColorOrFontPreview) {
            if (g_lastWasColorOrFontPreview && !isColorOrFontPreview) {
                CancelPreview(hwnd);
                RestoreWindowVisibility(hwnd);
            }
        }

        if (isColorOrFontPreview) {
            ShowWindowForPreview(hwnd);
        }

        g_lastWasColorOrFontPreview = isColorOrFontPreview;
        g_lastWasAnimationPreview = isAnimationPreview;

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
            CancelPreview(hwnd);
            RestoreWindowVisibility(hwnd);
            g_lastWasColorOrFontPreview = FALSE;
        }
        if (g_lastWasAnimationPreview) {
            CancelPreview(hwnd);
            g_lastWasAnimationPreview = FALSE;
        }
        g_lastPreviewMenuItem = 0;
    }

    return 0;
}
