/**
 * @file notification_wndproc_interaction.c
 * @brief Interactive and placement-related notification window messages.
 */

#include "notification_internal.h"

BOOL NotificationHandleInteractionMessage(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam,
                                           LRESULT* result) {
    if (!result) {
        return FALSE;
    }

    switch (msg) {
        case WM_GETMINMAXINFO: {
            const NotificationData* data = NotificationGetData(hwnd);
            if (data && data->isPreview && lParam) {
                MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
                minMaxInfo->ptMinTrackSize.x = NOTIFICATION_MIN_WIDTH;
                minMaxInfo->ptMinTrackSize.y = NOTIFICATION_MIN_HEIGHT;
                minMaxInfo->ptMaxTrackSize.x = NOTIFICATION_MAX_WIDTH;
                minMaxInfo->ptMaxTrackSize.y = NOTIFICATION_MAX_HEIGHT;
                *result = 0;
                return TRUE;
            }
            return FALSE;
        }

        case WM_NCHITTEST: {
            const NotificationData* data = NotificationGetData(hwnd);
            /* Normal notifications are non-interactive. */
            if (!data || !data->isPreview) {
                *result = HTCLIENT;
                return TRUE;
            }

            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            RECT rect;
            GetWindowRect(hwnd, &rect);

            int borderSize = 8;
            int cornerSize = 16;
            int relX = pt.x - rect.left;
            int relY = pt.y - rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            BOOL atLeft = (relX < borderSize);
            BOOL atRight = (relX >= width - borderSize);
            BOOL atTop = (relY < borderSize);
            BOOL atBottom = (relY >= height - borderSize);
            BOOL atTopLeft = (relX < cornerSize && relY < cornerSize);
            BOOL atTopRight = (relX >= width - cornerSize && relY < cornerSize);
            BOOL atBottomLeft = (relX < cornerSize && relY >= height - cornerSize);
            BOOL atBottomRight = (relX >= width - cornerSize && relY >= height - cornerSize);

            if (atTopLeft) *result = HTTOPLEFT;
            else if (atTopRight) *result = HTTOPRIGHT;
            else if (atBottomLeft) *result = HTBOTTOMLEFT;
            else if (atBottomRight) *result = HTBOTTOMRIGHT;
            else if (atTop) *result = HTTOP;
            else if (atBottom) *result = HTBOTTOM;
            else if (atLeft) *result = HTLEFT;
            else if (atRight) *result = HTRIGHT;
            else *result = HTCAPTION;
            return TRUE;
        }

        case WM_SETCURSOR: {
            const NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows show a move cursor while dragging. */
            if (data && data->isPreview && LOWORD(lParam) == HTCAPTION) {
                SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                *result = TRUE;
                return TRUE;
            }
            *result = DefWindowProc(hwnd, msg, wParam, lParam);
            return TRUE;
        }

        case WM_LBUTTONDOWN: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Normal notifications: left-click to dismiss. */
            if (data && !data->isPreview) {
                NotificationBeginFadeOut(hwnd, data);
            }
            *result = 0;
            return TRUE;
        }

        case WM_NCLBUTTONDBLCLK: {
            /* Preview windows: double-click the title area to dismiss. */
            if (wParam == HTCAPTION) {
                NotificationData* data = NotificationGetData(hwnd);
                if (data && data->isPreview) {
                    NotificationBeginFadeOut(hwnd, data);
                }
            }
            *result = 0;
            return TRUE;
        }

        case WM_ENTERSIZEMOVE: {
            NotificationData* data = NotificationGetData(hwnd);
            if (data && data->isPreview) {
                data->isInSizeMove = TRUE;
                NotificationStopGradientTimer(hwnd);
            }
            *result = 0;
            return TRUE;
        }

        case WM_SIZING:
            *result = TRUE;
            return TRUE;

        case WM_SIZE: {
            NotificationData* data = NotificationGetData(hwnd);
            if (data) {
                NotificationRenderWithRecovery(hwnd, data);
            }
            *result = 0;
            return TRUE;
        }

        case WM_EXITSIZEMOVE: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Save position/size only for preview windows. */
            if (data && data->isPreview) {
                data->isInSizeMove = FALSE;
                NotificationStartGradientTimer(hwnd, data);

                RECT rect;
                if (GetWindowRect(hwnd, &rect)) {
                    int x = rect.left;
                    int y = rect.top;
                    int width = rect.right - rect.left;
                    int height = rect.bottom - rect.top;
                    if (NotificationConstrainPosition(width, height, &x, &y) &&
                        (x != rect.left || y != rect.top) &&
                        SetWindowPos(hwnd, NULL, x, y, 0, 0,
                                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
                        rect.left = x;
                        rect.top = y;
                        rect.right = x + width;
                        rect.bottom = y + height;
                    }
                    if (!WriteConfigNotificationWindow(rect.left, rect.top,
                                                       rect.right - rect.left,
                                                       rect.bottom - rect.top)) {
                        LOG_WARNING("Failed to save notification preview window placement");
                    }
                }
            }
            if (data) {
                NotificationRenderWithRecovery(hwnd, data);
            }
            *result = 0;
            return TRUE;
        }

        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE: {
            if (msg == WM_SETTINGCHANGE && wParam != SPI_SETWORKAREA) {
                return FALSE;
            }

            RECT rect = {0};
            if (GetWindowRect(hwnd, &rect)) {
                int x = rect.left;
                int y = rect.top;
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;
                if (NotificationConstrainPosition(width, height, &x, &y) &&
                    (x != rect.left || y != rect.top)) {
                    SetWindowPos(hwnd, NULL, x, y, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            *result = 0;
            return TRUE;
        }

        case WM_RBUTTONDOWN: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows: right-click to dismiss. */
            if (data && data->isPreview) {
                NotificationBeginFadeOut(hwnd, data);
            }
            *result = 0;
            return TRUE;
        }

        case WM_MOUSEWHEEL: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows support quick appearance tuning via the wheel. */
            if (data && data->isPreview) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                BOOL ctrlDown = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;

                if (ctrlDown) {
                    int currentFontPercent = data->fontPercent + (delta > 0 ? 1 : -1);
                    currentFontPercent = NotificationClampFontPercent(currentFontPercent);
                    SetToastNotificationFontPercent(hwnd, currentFontPercent);
                    UpdateNotificationFontPercentControls(currentFontPercent);
                    *result = 0;
                    return TRUE;
                }

                int currentOpacity = data->opacityPercent + (delta > 0 ? 5 : -5);
                currentOpacity = NotificationClampOpacityPercent(currentOpacity);
                SetToastNotificationOpacity(hwnd, currentOpacity);
                UpdateNotificationOpacityControls(currentOpacity);

                data->pendingOpacity = currentOpacity;
                data->opacitySavePending = TRUE;
                data->opacitySaveRetryCount = 0;
                if (SetTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID,
                             NOTIFICATION_OPACITY_SAVE_DELAY_MS, NULL) == 0) {
                    NotificationTrySavePendingOpacity(data);
                }
            }
            *result = 0;
            return TRUE;
        }
    }

    return FALSE;
}
