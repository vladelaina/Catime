/**
 * @file notification_wndproc.c
 * @brief Window message handling for notification windows.
 */

#include "notification_internal.h"

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT interactionResult = 0;
    if (NotificationHandleInteractionMessage(hwnd, msg, wParam, lParam,
                                             &interactionResult)) {
        return interactionResult;
    }

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
            if (wParam == NOTIFICATION_OPACITY_SAVE_TIMER_ID) {
                NotificationFlushPendingOpacity(hwnd, data, TRUE);
                return 0;
            }
            if (wParam == NOTIFICATION_GRADIENT_TIMER_ID) {
                if (data->hasAnimatedGradient && !data->isInSizeMove) {
                    NotificationRenderWithRecovery(hwnd, data);
                }
                return 0;
            }
            if (wParam == NOTIFICATION_RENDER_RETRY_TIMER_ID) {
                KillTimer(hwnd, NOTIFICATION_RENDER_RETRY_TIMER_ID);
                RenderRetry_MarkTimerFired(&data->renderRetry);
                NotificationRenderWithRecovery(hwnd, data);
                return 0;
            }
            if (wParam == ANIMATION_TIMER_ID) {
                BOOL shouldDestroy = FALSE;
                BYTE newOpacity = NotificationUpdateAnimationOpacity(
                    data->animState, data->opacity, data->maxOpacity,
                    &shouldDestroy);

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
