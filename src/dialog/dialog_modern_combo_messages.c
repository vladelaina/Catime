/**
 * @file dialog_modern_combo_messages.c
 * @brief Combo popup window message handling.
 */

#include "dialog_modern_internal.h"

LRESULT CALLBACK ModernComboListSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernControl* control = (ModernControl*)refData;
    const ModernDialogState* state = control ? control->owner : NULL;
    switch (msg) {
        case WM_SHOWWINDOW: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control) {
                control->comboHotItem = -1;
                control->comboScrollHovered = FALSE;
                control->comboScrollDragging = FALSE;
                control->comboWheelDelta = 0;
            }
            if (wParam && control) {
                ModernApplyComboListRegion(hwnd, control);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
            }
            return result;
        }
        case WM_MOUSEWHEEL:
            if (control && state) {
                int visibleItems = ModernGetComboListVisibleItems(
                    hwnd, control);
                int previousTop = (int)SendMessageW(
                    hwnd, LB_GETTOPINDEX, 0, 0);
                UINT scrollLines = 3;
                SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0,
                                      &scrollLines, 0);
                if (scrollLines == 0) return 0;
                int lineCount = scrollLines == WHEEL_PAGESCROLL
                    ? max(1, visibleItems - 1)
                    : max(1, (int)scrollLines);
                control->comboWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
                int notches = control->comboWheelDelta / WHEEL_DELTA;
                control->comboWheelDelta -= notches * WHEEL_DELTA;
                if (notches != 0) {
                    int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
                    int maximumTop = max(0, count - visibleItems);
                    int topIndex = previousTop - notches * lineCount;
                    if (topIndex < 0) topIndex = 0;
                    if (topIndex > maximumTop) topIndex = maximumTop;
                    if (topIndex != previousTop) {
                        SendMessageW(hwnd, LB_SETTOPINDEX, topIndex, 0);
                        POINT point = {
                            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                        ScreenToClient(hwnd, &point);
                        int hotItem = (int)SendMessageW(
                            hwnd, LB_ITEMFROMPOINT, 0,
                            MAKELPARAM(point.x, point.y));
                        control->comboHotItem = HIWORD(hotItem)
                            ? -1 : LOWORD(hotItem);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
                return 0;
            }
            break;
        case WM_WINDOWPOSCHANGED:
        case WM_SIZE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            ModernApplyComboListRegion(hwnd, control);
            return result;
        }
        case WM_ERASEBKGND:
            if (state) return 1;
            break;
        case WM_PAINT:
            if (control && state) {
                ModernPaintComboList(hwnd, control, NULL);
                return 0;
            }
            break;
        case WM_PRINTCLIENT:
            if (control && state) {
                ModernPaintComboList(hwnd, control, (HDC)wParam);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (control && state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                BOOL hasScrollbar = ModernGetComboListScrollbarRects(
                    hwnd, control, &track, &thumb);
                if (control->comboScrollDragging && hasScrollbar) {
                    int travel = (track.bottom - track.top) -
                                 (thumb.bottom - thumb.top);
                    if (travel > 0) {
                        int count = (int)SendMessageW(
                            hwnd, LB_GETCOUNT, 0, 0);
                        int visibleItems = ModernGetComboListVisibleItems(
                            hwnd, control);
                        int maximumTop = max(0, count - visibleItems);
                        int previousTop = (int)SendMessageW(
                            hwnd, LB_GETTOPINDEX, 0, 0);
                        int topIndex = control->comboScrollDragStartTopIndex +
                            MulDiv(point.y - control->comboScrollDragStartY,
                                   maximumTop, travel);
                        if (topIndex < 0) topIndex = 0;
                        if (topIndex > maximumTop) topIndex = maximumTop;
                        if (topIndex != previousTop) {
                            SendMessageW(hwnd, LB_SETTOPINDEX, topIndex, 0);
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                    return 0;
                }
                BOOL hovered = hasScrollbar &&
                    ModernPointInComboScrollbar(control, &track, point);
                int hotItem = -1;
                if (!hovered) {
                    hotItem = (int)SendMessageW(
                        hwnd, LB_ITEMFROMPOINT, 0,
                        MAKELPARAM(point.x, point.y));
                    if (HIWORD(hotItem)) hotItem = -1;
                    else hotItem = LOWORD(hotItem);
                }
                if (hotItem != control->comboHotItem) {
                    int previousHotItem = control->comboHotItem;
                    control->comboHotItem = hotItem;
                    ModernInvalidateComboListItem(hwnd, previousHotItem);
                    ModernInvalidateComboListItem(hwnd, hotItem);
                }
                if (hovered != control->comboScrollHovered) {
                    control->comboScrollHovered = hovered;
                    InvalidateRect(hwnd, &track, FALSE);
                }
                ModernTrackMouse(hwnd);
                if (hovered) return 0;
            }
            break;
        case WM_MOUSELEAVE:
            if (control && !control->comboScrollDragging) {
                int previousHotItem = control->comboHotItem;
                BOOL repaintScrollbar = control->comboScrollHovered;
                control->comboScrollHovered = FALSE;
                control->comboHotItem = -1;
                ModernInvalidateComboListItem(hwnd, previousHotItem);
                if (repaintScrollbar) {
                    RECT track = {0};
                    RECT thumb = {0};
                    if (ModernGetComboListScrollbarRects(
                            hwnd, control, &track, &thumb)) {
                        InvalidateRect(hwnd, &track, FALSE);
                    }
                }
            }
            break;
        case WM_LBUTTONDOWN:
            if (control) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetComboListScrollbarRects(
                        hwnd, control, &track, &thumb) &&
                    ModernPointInComboScrollbar(control, &track, point)) {
                    int previousHotItem = control->comboHotItem;
                    control->comboHotItem = -1;
                    ModernInvalidateComboListItem(hwnd, previousHotItem);
                    if (!ModernPointInComboScrollbar(
                            control, &thumb, point)) {
                        int direction = point.y < thumb.top ? -1 : 1;
                        int topIndex = (int)SendMessageW(
                            hwnd, LB_GETTOPINDEX, 0, 0);
                        int page = max(
                            1, ModernGetComboListVisibleItems(
                                   hwnd, control) - 1);
                        int count = (int)SendMessageW(
                            hwnd, LB_GETCOUNT, 0, 0);
                        int maximumTop = max(
                            0, count - ModernGetComboListVisibleItems(
                                           hwnd, control));
                        int nextTop = topIndex + direction * page;
                        if (nextTop < 0) nextTop = 0;
                        if (nextTop > maximumTop) nextTop = maximumTop;
                        if (nextTop != topIndex) {
                            SendMessageW(hwnd, LB_SETTOPINDEX,
                                         nextTop, 0);
                        }
                    } else {
                        control->comboScrollDragging = TRUE;
                        control->comboScrollDragStartY = point.y;
                        control->comboScrollDragStartTopIndex =
                            (int)SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0);
                        SetCapture(hwnd);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (control && control->comboScrollDragging) {
                control->comboScrollDragging = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
                ModernRefreshComboListPointerState(hwnd, control);
                return 0;
            }
            break;
        case WM_CANCELMODE:
            if (control && control->comboScrollDragging) {
                control->comboScrollDragging = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
                ModernRefreshComboListPointerState(hwnd, control);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (control && control->comboScrollDragging &&
                (HWND)lParam != hwnd) {
                control->comboScrollDragging = FALSE;
                ModernRefreshComboListPointerState(hwnd, control);
            }
            break;
        case WM_SETCURSOR:
            if (control && control->comboScrollHovered) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_NCPAINT:
            return 0;
        case WM_NCDESTROY:
            if (control) {
                control->comboHotItem = -1;
                control->comboScrollHovered = FALSE;
                control->comboScrollDragging = FALSE;
                control->comboWheelDelta = 0;
                control->comboListRegionWidth = 0;
                control->comboListRegionHeight = 0;
                control->comboListRegionDpi = 0;
            }
            RemoveWindowSubclass(hwnd, ModernComboListSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
