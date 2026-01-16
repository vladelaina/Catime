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
#include "tray/tray_events.h"
#include "tray/tray_animation_core.h"
#include "drag_scale.h"
#include "window_procedure/window_procedure.h"
#include "cli.h"
#include "window.h"
#include "config.h"
#include "color/color.h"
#include "drawing.h"
#include "menu_preview.h"
#include "window/window_visual_effects.h"
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
#define BUFFER_SIZE_CLI_INPUT 256
#define BUFFER_SIZE_MENU_ITEM 100

extern UINT WM_TASKBARCREATED;
extern BOOL CLOCK_EDIT_MODE;
extern size_t COLOR_OPTIONS_COUNT;
extern PredefinedColor* COLOR_OPTIONS;

/* ============================================================================
 * Message Handler Implementations
 * ============================================================================ */

LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    HandleWindowCreate(hwnd);
    extern void ConfigWatcher_Start(HWND hwnd);
    ConfigWatcher_Start(hwnd);
    return 0;
}

LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    
    /* In non-edit mode, show hand cursor for clickable regions */
    if (!CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
        POINT pt;
        GetCursorPos(&pt);
        
        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);
        
        const ClickableRegion* region = GetClickableRegionAt(pt);
        if (region) {
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
    (void)wp;
    
    /* In non-edit mode, check for clickable region clicks */
    if (!CLOCK_EDIT_MODE) {
        POINT pt;
        GetCursorPos(&pt);
        
        /* Update region positions */
        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);
        
        const ClickableRegion* region = GetClickableRegionAt(pt);
        if (region) {
            HandleRegionClick(region, hwnd);
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

#define TIMER_ID_TRANSITION_END 100
extern BOOL g_IsTransitioning;

LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == TIMER_ID_TRANSITION_END) {
        KillTimer(hwnd, TIMER_ID_TRANSITION_END);
        g_IsTransitioning = FALSE;
        // Force full update now that transition is done
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    if (wp == IDT_MENU_DEBOUNCE) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
        return 0;
    }
    if (wp == IDT_ANIMATION_PREVIEW_DELAY) {
        KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
        extern UINT GetPendingAnimationPreviewItem(void);
        UINT menuItem = GetPendingAnimationPreviewItem();
        if (menuItem != 0) {
            extern BOOL DispatchMenuPreview(HWND hwnd, UINT menuId);
            DispatchMenuPreview(hwnd, menuItem);
        }
        return 0;
    }
    /* Handle click-through timer for dynamic WS_EX_TRANSPARENT switching */
    extern UINT GetClickThroughTimerId(void);
    extern void UpdateClickThroughState(HWND hwnd);
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
    (void)wp; (void)lp;
    /* Delegate to main timer event handler */
    HandleTimerEvent(hwnd, TIMER_ID_MAIN);
    /* Force immediate repaint to bypass WM_PAINT queue delay */
    UpdateWindow(hwnd);
    return 0;
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    UnregisterGlobalHotkeys(hwnd);
    HandleWindowDestroy(hwnd);
    extern void ConfigWatcher_Stop(void);
    ConfigWatcher_Stop();
    
    return 0;
}

LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
    return 0;
}

LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    WINDOWPOS* pwp = (WINDOWPOS*)lp;
    if (!(pwp->flags & SWP_NOSIZE)) {
        if (CLOCK_EDIT_MODE) {
            // Region update logic removed - relying on UpdateLayeredWindow alpha channel
        }
    }
    return 0;
}

LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    return 0;
}

LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONUP, wp, lp);
}

LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        ToggleEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONDOWN, wp, lp);
}

LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL);
    return 0;
}

LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    SaveWindowSettings(hwnd);
    DestroyWindow(hwnd);
    return 0;
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
    if (TrayAnimation_HandleUpdateMessage()) return 0;
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
    TrayAnimation_HandleUpdateMessage();
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

    int colorIndex = lpdis->itemID - CMD_COLOR_OPTIONS_BASE;
    if (colorIndex < 0 || colorIndex >= (int)COLOR_OPTIONS_COUNT) return FALSE;

    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
    GradientType gradientType = GetGradientTypeByName(hexColor);
    
    /* Draw color/gradient with space for sequence number */
    RECT colorRect = lpdis->rcItem;
    colorRect.left += 28;  /* Leave space for number */
    
    if (gradientType != GRADIENT_NONE) {
        const GradientInfo* info = GetGradientInfo(gradientType);
        if (!info) return FALSE;
        DrawGradientRect(lpdis->hDC, &colorRect, info);
    } else {
        int r, g, b;
        sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);

        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));

        HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
        HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);

        Rectangle(lpdis->hDC, colorRect.left, colorRect.top,
                 colorRect.right, colorRect.bottom);

        SelectObject(lpdis->hDC, oldPen);
        SelectObject(lpdis->hDC, oldBrush);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    /* Draw sequence number on left side */
    wchar_t numStr[8];
    _snwprintf_s(numStr, 8, _TRUNCATE, L"%d", colorIndex + 1);
    RECT numRect = lpdis->rcItem;
    numRect.right = numRect.left + 26;
    SetBkMode(lpdis->hDC, TRANSPARENT);
    SetTextColor(lpdis->hDC, GetSysColor(COLOR_MENUTEXT));
    DrawTextW(lpdis->hDC, numStr, -1, &numRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (lpdis->itemState & ODS_SELECTED) {
        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
    }

    return TRUE;
}

static UINT g_pendingAnimationPreviewItem = 0;

UINT GetPendingAnimationPreviewItem(void) {
    return g_pendingAnimationPreviewItem;
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
        extern void CleanupBeforeTimerAction(void);
        extern BOOL StartCountdownWithTime(HWND hwnd, int seconds);
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
        extern void CancelPreview(HWND hwnd);
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
        extern void TriggerUpdateDownload(HWND hwnd);
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
    (void)lp;
    if (wp == 1) {
        /* Update available - show update dialog */
        extern void ShowStoredUpdateDialog(HWND hwnd);
        ShowStoredUpdateDialog(hwnd);
    } else {
        /* No update - show no update dialog */
        extern void ShowStoredNoUpdateDialog(HWND hwnd);
        ShowStoredNoUpdateDialog(hwnd);
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
        extern void SetFontLicenseAccepted(BOOL);
        extern void SetFontLicenseVersionAccepted(const char*);
        extern const char* GetCurrentFontLicenseVersion(void);
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
    
    extern int GetPendingPluginIndex(void);
    extern void ClearPendingPluginInfo(void);
    extern BOOL PluginManager_StartPluginAfterSecurityCheck(int index, BOOL trustPlugin);
    extern const PluginInfo* PluginManager_GetPlugin(int index);
    extern void PluginData_SetText(const wchar_t* text);
    extern void PluginData_SetActive(BOOL active);
    extern const wchar_t* PluginProcess_GetLastError(void);
    extern void StopNotificationSound(void);
    extern void GetActiveColor(char* outColor, size_t bufferSize);
    extern BOOL CLOCK_SHOW_CURRENT_TIME;
    extern BOOL CLOCK_COUNT_UP;
    extern BOOL CLOCK_IS_PAUSED;
    extern int CLOCK_TOTAL_TIME;
    extern int countdown_elapsed_time;
    extern int countup_elapsed_time;
    extern POMODORO_PHASE current_pomodoro_phase;
    
    int pluginIndex = GetPendingPluginIndex();
    
    if (wp == IDCANCEL) {
        /* User cancelled - just clear pending info, don't change display state */
        ClearPendingPluginInfo();
        return 0;
    }
    
    /* User confirmed - now change state and start plugin */
    
    /* Stop notification sound */
    StopNotificationSound();
    
    /* Prevent countdown completion notification from triggering */
    extern BOOL countdown_message_shown;
    countdown_message_shown = TRUE;
    
    /* Reset timer flags */
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_COUNT_UP = FALSE;
    CLOCK_IS_PAUSED = TRUE;
    
    /* Stop internal timer */
    KillTimer(hwnd, 1);
    
    /* Reset Pomodoro if active */
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
    }

    /* Reset timer values */
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    /* Show loading message */
    const PluginInfo* pluginInfo = PluginManager_GetPlugin(pluginIndex);
    if (pluginInfo) {
        wchar_t loadingText[256];
        _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", pluginInfo->displayName);
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
            PluginData_SetText(errorMsg);
        } else {
            PluginData_SetText(L"FAIL");
        }
        PluginData_SetActive(TRUE);
    }
    
    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientAnimated(GetGradientTypeByName(activeColor))) {
        SetTimer(hwnd, 1, 66, NULL);  /* 15 FPS for smooth animation */
    }
    
    /* Ensure window visible and redraw */
    if (!IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    
    ClearPendingPluginInfo();
    
    return 0;
}

/**
 * @brief Handle plugin hot-reload request from background thread
 * @param wp Plugin index to restart
 * @param lp Reserved
 */
LRESULT HandlePluginHotReload(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)lp;
    
    extern BOOL PluginManager_RestartPlugin(int index);
    
    int pluginIndex = (int)wp;
    LOG_INFO("[HotReload] Restarting plugin %d from main thread", pluginIndex);
    
    PluginManager_RestartPlugin(pluginIndex);
    
    return 0;
}

LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    UINT menuItem = LOWORD(wp);
    UINT flags = HIWORD(wp);
    HMENU hMenu = (HMENU)lp;

    static UINT lastMenuItem = 0;
    static BOOL lastWasColorOrFont = FALSE;
    static BOOL lastWasAnimationPreview = FALSE;

    /* Mouse moved outside any menu item - set debounce timer to cancel preview */
    if (menuItem == 0xFFFF) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL);
        lastMenuItem = 0;
        lastWasColorOrFont = FALSE;
        lastWasAnimationPreview = FALSE;
        return 0;
    } else {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    }

    if (hMenu == NULL) return 0;

    if (!(flags & MF_POPUP)) {
        BOOL isColorOrFontPreview = FALSE;
        BOOL isAnimationPreview = FALSE;

        int colorIndex = menuItem - CMD_COLOR_OPTIONS_BASE;
        if (colorIndex >= 0 && colorIndex < (int)COLOR_OPTIONS_COUNT) {
            isColorOrFontPreview = TRUE;
        }


        if (menuItem >= CMD_FONT_SELECTION_BASE && menuItem < CMD_FONT_SELECTION_END) {
            /* Exclude all animation menu items (2200-2220) from font preview */
            if (menuItem < CLOCK_IDM_ANIMATIONS_MENU || menuItem > CLOCK_IDM_ANIM_SPEED_TIMER) {
                isColorOrFontPreview = TRUE;
            }
        }

        if (menuItem == CLOCK_IDM_TIME_FORMAT_DEFAULT ||
            menuItem == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED ||
            menuItem == CLOCK_IDM_TIME_FORMAT_FULL_PADDED ||
            menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS ||
            menuItem == CLOCK_IDM_GLOW_EFFECT ||
            menuItem == CLOCK_IDM_GLASS_EFFECT ||
            menuItem == CLOCK_IDM_NEON_EFFECT ||
            menuItem == CLOCK_IDM_HOLOGRAPHIC_EFFECT ||
            menuItem == CLOCK_IDM_LIQUID_EFFECT) {
            isColorOrFontPreview = TRUE;
        }

        if (menuItem == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
            menuItem == CLOCK_IDM_ANIMATIONS_USE_CPU ||
            menuItem == CLOCK_IDM_ANIMATIONS_USE_MEM ||
            menuItem == CLOCK_IDM_ANIMATIONS_USE_NONE ||
            (menuItem >= CLOCK_IDM_ANIMATIONS_BASE && menuItem < CLOCK_IDM_ANIMATIONS_END)) {
            isAnimationPreview = TRUE;
        }

        if (isAnimationPreview != lastWasAnimationPreview) {
            if (lastWasAnimationPreview && !isAnimationPreview) {
                CancelPreview(hwnd);
            }
        }

        if (isColorOrFontPreview != lastWasColorOrFont) {
            if (lastWasColorOrFont && !isColorOrFontPreview) {
                CancelPreview(hwnd);
                RestoreWindowVisibility(hwnd);
            }
        }

        if (isColorOrFontPreview) {
            ShowWindowForPreview(hwnd);
        }

        lastWasColorOrFont = isColorOrFontPreview;
        lastWasAnimationPreview = isAnimationPreview;

        if (menuItem != lastMenuItem) {
            lastMenuItem = menuItem;

            KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
            g_pendingAnimationPreviewItem = menuItem;
            SetTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY, 30, NULL);
        }
    } else {
        KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
        g_pendingAnimationPreviewItem = 0;
        if (lastWasColorOrFont) {
            CancelPreview(hwnd);
            RestoreWindowVisibility(hwnd);
            lastWasColorOrFont = FALSE;
        }
        if (lastWasAnimationPreview) {
            CancelPreview(hwnd);
            lastWasAnimationPreview = FALSE;
        }
        lastMenuItem = 0;
    }

    return 0;
}

