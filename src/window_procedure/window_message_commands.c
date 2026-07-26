/**
 * @file window_message_commands.c
 * @brief Handles keyboard, CLI, hotkey, timer command, and tray update messages.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "cli.h"
#include "config.h"
#include "drag_scale.h"
#include "tray/tray.h"
#include "tray/tray_animation_core.h"
#include "window.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_procedure.h"

#include <string.h>

#define BUFFER_SIZE_CLI_INPUT 256

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

        /* The current WM_KEYDOWN is authoritative; poll the other arrows only
         * to preserve diagonal movement across remote/input-driver variants. */
        BOOL upDown = wp == VK_UP || (GetKeyState(VK_UP) & 0x8000);
        BOOL downDown = wp == VK_DOWN || (GetKeyState(VK_DOWN) & 0x8000);
        BOOL leftDown = wp == VK_LEFT || (GetKeyState(VK_LEFT) & 0x8000);
        BOOL rightDown = wp == VK_RIGHT || (GetKeyState(VK_RIGHT) & 0x8000);
        int dx = (rightDown ? step : 0) - (leftDown ? step : 0);
        int dy = (downDown ? step : 0) - (upDown ? step : 0);

        if (dx != 0 || dy != 0) {
            FinalizeScaleWindowGestureForManualMove(hwnd);
            /* Keyboard placement also supersedes a completed scale anchor. */
            ConsumePendingScaleResizeAnchor(hwnd);

            RECT rect;
            GetWindowRect(hwnd, &rect);
            int newX = rect.left + dx;
            int newY = rect.top + dy;
            if (SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
                CLOCK_WINDOW_POS_X = newX;
                CLOCK_WINDOW_POS_Y = newY;
                MarkManualEditWindowPosition(hwnd);
                ScheduleConfigSave(hwnd);
            }
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
    RefreshTrayBackgroundWorkState();
    return 0;
}
