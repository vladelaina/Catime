/**
 * @file notification_wndproc.c
 * @brief Window message handling for notification windows.
 */

#include "notification_internal.h"

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            /* The layered-window frame is redrawn as a complete buffer. */
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!hdc) {
                return 0;
            }
            NotificationData* data = NotificationGetData(hwnd);
            if (data) {
                NotificationRenderWithRecovery(hwnd, data);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            NotificationData* data = NotificationGetData(hwnd);
            if (!data) break;

            if (wParam == NOTIFICATION_TIMER_ID) {
                NotificationBeginFadeOut(hwnd, data);
                return 0;
            }
            else if (wParam == NOTIFICATION_OPACITY_SAVE_TIMER_ID) {
                NotificationFlushPendingOpacity(hwnd, data, TRUE);
                return 0;
            }
            else if (wParam == NOTIFICATION_GRADIENT_TIMER_ID) {
                if (data->hasAnimatedGradient && !data->isInSizeMove) {
                    NotificationRenderWithRecovery(hwnd, data);
                }
                return 0;
            }
            else if (wParam == NOTIFICATION_RENDER_RETRY_TIMER_ID) {
                KillTimer(hwnd, NOTIFICATION_RENDER_RETRY_TIMER_ID);
                RenderRetry_MarkTimerFired(&data->renderRetry);
                NotificationRenderWithRecovery(hwnd, data);
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                BOOL shouldDestroy = FALSE;

                BYTE newOpacity = NotificationUpdateAnimationOpacity(data->animState, data->opacity,
                                                        data->maxOpacity, &shouldDestroy);

                if (shouldDestroy) {
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                    DestroyWindow(hwnd);
                    return 0;
                }

                data->opacity = newOpacity;
                NotificationRenderWithRecovery(hwnd, data);

                if (data->animState == ANIM_FADE_IN && newOpacity >= data->maxOpacity) {
                    data->animState = ANIM_VISIBLE;
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                }

                return 0;
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            const NotificationData* data = NotificationGetData(hwnd);
            if (data && data->isPreview && lParam) {
                MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
                minMaxInfo->ptMinTrackSize.x = NOTIFICATION_MIN_WIDTH;
                minMaxInfo->ptMinTrackSize.y = NOTIFICATION_MIN_HEIGHT;
                minMaxInfo->ptMaxTrackSize.x = NOTIFICATION_MAX_WIDTH;
                minMaxInfo->ptMaxTrackSize.y = NOTIFICATION_MAX_HEIGHT;
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            const NotificationData* data = NotificationGetData(hwnd);
            /* Normal notifications are non-interactive */
            if (!data || !data->isPreview) {
                return HTCLIENT;
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

            if (atTopLeft) return HTTOPLEFT;
            if (atTopRight) return HTTOPRIGHT;
            if (atBottomLeft) return HTBOTTOMLEFT;
            if (atBottomRight) return HTBOTTOMRIGHT;
            if (atTop) return HTTOP;
            if (atBottom) return HTBOTTOM;
            if (atLeft) return HTLEFT;
            if (atRight) return HTRIGHT;

            return HTCAPTION;
        }

        case WM_SETCURSOR: {
            const NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows show crosshair cursor for dragging */
            if (data && data->isPreview) {
                WORD hitTest = LOWORD(lParam);
                if (hitTest == HTCAPTION) {
                    SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_LBUTTONDOWN: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Normal notifications: left-click to dismiss */
            if (data && !data->isPreview) {
                NotificationBeginFadeOut(hwnd, data);
            }
            return 0;
        }

        case WM_NCLBUTTONDBLCLK: {
            /* Preview windows: double-click title to dismiss */
            if (wParam == HTCAPTION) {
                NotificationData* data = NotificationGetData(hwnd);
                if (data && data->isPreview) {
                    NotificationBeginFadeOut(hwnd, data);
                }
            }
            return 0;
        }

        case WM_ENTERSIZEMOVE: {
            NotificationData* data = NotificationGetData(hwnd);
            if (data && data->isPreview) {
                data->isInSizeMove = TRUE;
                NotificationStopGradientTimer(hwnd);
            }
            return 0;
        }

        case WM_SIZING: {
            return TRUE;
        }

        case WM_SIZE: {
            NotificationData* data = NotificationGetData(hwnd);
            if (data) {
                NotificationRenderWithRecovery(hwnd, data);
            }
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Save position/size only for preview windows */
            if (data && data->isPreview) {
                data->isInSizeMove = FALSE;
                NotificationStartGradientTimer(hwnd, data);

                RECT rect;
                if (GetWindowRect(hwnd, &rect)) {
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
            return 0;
        }

        case WM_RBUTTONDOWN: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows: right-click to dismiss */
            if (data && data->isPreview) {
                NotificationBeginFadeOut(hwnd, data);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            NotificationData* data = NotificationGetData(hwnd);
            /* Preview windows support quick appearance tuning via scroll wheel. */
            if (data && data->isPreview) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                BOOL ctrlDown = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;

                if (ctrlDown) {
                    int currentFontPercent = data->fontPercent;

                    if (delta > 0) {
                        currentFontPercent += 1;
                    } else {
                        currentFontPercent -= 1;
                    }

                    currentFontPercent = NotificationClampFontPercent(currentFontPercent);
                    SetToastNotificationFontPercent(hwnd, currentFontPercent);
                    UpdateNotificationFontPercentControls(currentFontPercent);
                    return 0;
                }

                int currentOpacity = data->opacityPercent;
                int step = 5;

                /* Scroll up increases opacity, scroll down decreases */
                if (delta > 0) {
                    currentOpacity += step;
                } else {
                    currentOpacity -= step;
                }

                currentOpacity = NotificationClampOpacityPercent(currentOpacity);
                SetToastNotificationOpacity(hwnd, currentOpacity);

                /* Sync settings dialog controls if dialog is open */
                UpdateNotificationOpacityControls(currentOpacity);

                data->pendingOpacity = currentOpacity;
                data->opacitySavePending = TRUE;
                data->opacitySaveRetryCount = 0;
                if (SetTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID,
                             NOTIFICATION_OPACITY_SAVE_DELAY_MS, NULL) == 0) {
                    NotificationTrySavePendingOpacity(data);
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            NotificationData* data = NotificationGetData(hwnd);
            if (data) {
                NotificationFlushPendingOpacity(hwnd, data, FALSE);
            }
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
            KillTimer(hwnd, NOTIFICATION_GRADIENT_TIMER_ID);
            KillTimer(hwnd, NOTIFICATION_RENDER_RETRY_TIMER_ID);

            if (data) {
                NotificationFreeData(hwnd, data);
            }

            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
