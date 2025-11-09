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
#include "color/color.h"
#include "drawing.h"
#include "menu_preview.h"
#include "../resource/resource.h"
#include <stdio.h>

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

LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDT_MENU_DEBOUNCE) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        CancelPreview(hwnd);
        return 0;
    }
    HandleTimerEvent(hwnd, wp);
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
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) SaveWindowSettings(hwnd);
    return 0;
}

LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    AdjustWindowPosition(hwnd, TRUE);
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
        CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
        if (CLOCK_EDIT_MODE) {
            SetClickThrough(hwnd, FALSE);
        } else {
            extern char CLOCK_TEXT_COLOR[10];
            SetClickThrough(hwnd, TRUE);
            SaveWindowSettings(hwnd);
            WriteConfigColor(CLOCK_TEXT_COLOR);
        }
        InvalidateRect(hwnd, NULL, TRUE);
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

LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (DispatchHotkey(hwnd, (int)wp)) return 0;
    return DefWindowProc(hwnd, WM_HOTKEY, wp, lp);
}

LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
    if (pcds && pcds->dwData == COPYDATA_ID_CLI_TEXT && pcds->lpData && pcds->cbData > 0) {
        const size_t maxLen = BUFFER_SIZE_CLI_INPUT - 1;
        char buf[BUFFER_SIZE_CLI_INPUT];
        size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
        memcpy(buf, pcds->lpData, n);
        buf[maxLen] = '\0';
        buf[n] = '\0';
        HandleCliArguments(hwnd, buf);
        return 0;
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
    int r, g, b;
    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
    
    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    
    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
    
    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
             lpdis->rcItem.right, lpdis->rcItem.bottom);
    
    SelectObject(lpdis->hDC, oldPen);
    SelectObject(lpdis->hDC, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
    
    if (lpdis->itemState & ODS_SELECTED) {
        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
    }
    
    return TRUE;
}

LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    UINT menuItem = LOWORD(wp);
    UINT flags = HIWORD(wp);
    HMENU hMenu = (HMENU)lp;

    if (menuItem == 0xFFFF) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL);
        return 0;
    }

    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    if (hMenu == NULL) return 0;

    if (menuItem >= CLOCK_IDM_ANIMATIONS_USE_LOGO && menuItem <= CLOCK_IDM_ANIMATIONS_USE_MEM) {
        if (DispatchMenuPreview(hwnd, menuItem)) {
            return 0;
        }
    }

    if (!(flags & MF_POPUP)) {
        if (DispatchMenuPreview(hwnd, menuItem)) {
            return 0;
        }
    }

    CancelPreview(hwnd);
    return 0;
}

