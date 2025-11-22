/**
 * @file update_control.c
 * @brief Custom UI controls for update dialogs (scrollbar, markdown view)
 */
#include "update/update_internal.h"
#include "markdown/markdown_parser.h"
#include <windowsx.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

/** @brief Calculate modern scrollbar thumb rectangle */
void CalculateScrollbarThumbRect(RECT clientRect, int scrollPos, int scrollMax,
                                         int scrollPage, RECT* outThumbRect) {
    int trackHeight = clientRect.bottom - clientRect.top;
    int contentHeight = scrollMax;

    if (contentHeight <= scrollPage || scrollPage == 0) {
        SetRectEmpty(outThumbRect);
        return;
    }

    int thumbHeight = (int)((float)scrollPage / contentHeight * trackHeight);
    if (thumbHeight < MODERN_SCROLLBAR_MIN_THUMB) {
        thumbHeight = MODERN_SCROLLBAR_MIN_THUMB;
    }

    int maxThumbTop = trackHeight - thumbHeight;
    int thumbTop = (int)((float)scrollPos / (contentHeight - scrollPage) * maxThumbTop);

    outThumbRect->left = clientRect.right - MODERN_SCROLLBAR_WIDTH - MODERN_SCROLLBAR_MARGIN;
    outThumbRect->top = clientRect.top + thumbTop;
    outThumbRect->right = clientRect.right - MODERN_SCROLLBAR_MARGIN;
    outThumbRect->bottom = outThumbRect->top + thumbHeight;
}

/** @brief Draw rounded rectangle */
void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color) {
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

/** @brief Subclassed window procedure for scrollable notes control */
LRESULT CALLBACK NotesControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
        case WM_LBUTTONDOWN: {
            int scrollPos = (int)(INT_PTR)GetProp(hwnd, L"ScrollPos");
            int scrollMax = (int)(INT_PTR)GetProp(hwnd, L"ScrollMax");
            int scrollPage = (int)(INT_PTR)GetProp(hwnd, L"ScrollPage");

            if (scrollMax > scrollPage) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);

                RECT thumbRect;
                CalculateScrollbarThumbRect(clientRect, scrollPos, scrollMax, scrollPage, &thumbRect);

                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

                if (PtInRect(&thumbRect, pt)) {
                    SetProp(hwnd, L"ThumbDragging", (HANDLE)1);
                    SetProp(hwnd, L"DragStartY", (HANDLE)(INT_PTR)pt.y);
                    SetProp(hwnd, L"DragStartScrollPos", (HANDLE)(INT_PTR)scrollPos);
                    SetCapture(hwnd);
                    return 0;
                } else if (pt.x >= clientRect.right - MODERN_SCROLLBAR_WIDTH - MODERN_SCROLLBAR_MARGIN) {
                    int trackHeight = clientRect.bottom - clientRect.top;
                    int thumbHeight = (int)((float)scrollPage / scrollMax * trackHeight);
                    if (thumbHeight < MODERN_SCROLLBAR_MIN_THUMB) thumbHeight = MODERN_SCROLLBAR_MIN_THUMB;

                    if (pt.y < thumbRect.top) {
                        scrollPos -= scrollPage;
                    } else if (pt.y > thumbRect.bottom) {
                        scrollPos += scrollPage;
                    }

                    if (scrollPos < 0) scrollPos = 0;
                    if (scrollPos > scrollMax - scrollPage) scrollPos = scrollMax - scrollPage;

                    SetProp(hwnd, L"ScrollPos", (HANDLE)(INT_PTR)scrollPos);
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            MarkdownLink* links = (MarkdownLink*)GetProp(hwnd, L"MarkdownLinks");
            int linkCount = (int)(INT_PTR)GetProp(hwnd, L"LinkCount");
            scrollPos = (int)(INT_PTR)GetProp(hwnd, L"ScrollPos");

            if (links && linkCount > 0) {
                POINT pt;
                pt.x = GET_X_LPARAM(lParam) + 5;
                pt.y = GET_Y_LPARAM(lParam) + scrollPos + 5;

                if (HandleMarkdownClick(links, linkCount, pt)) {
                    return 0;
                }
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        case WM_LBUTTONUP: {
            if ((INT_PTR)GetProp(hwnd, L"ThumbDragging")) {
                SetProp(hwnd, L"ThumbDragging", (HANDLE)0);
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if ((INT_PTR)GetProp(hwnd, L"ThumbDragging")) {
                int dragStartY = (int)(INT_PTR)GetProp(hwnd, L"DragStartY");
                int dragStartScrollPos = (int)(INT_PTR)GetProp(hwnd, L"DragStartScrollPos");
                int scrollMax = (int)(INT_PTR)GetProp(hwnd, L"ScrollMax");
                int scrollPage = (int)(INT_PTR)GetProp(hwnd, L"ScrollPage");

                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int trackHeight = clientRect.bottom - clientRect.top;

                int thumbHeight = (int)((float)scrollPage / scrollMax * trackHeight);
                if (thumbHeight < MODERN_SCROLLBAR_MIN_THUMB) thumbHeight = MODERN_SCROLLBAR_MIN_THUMB;

                int maxThumbTop = trackHeight - thumbHeight;
                int deltaY = pt.y - dragStartY;
                int deltaScroll = (int)((float)deltaY / maxThumbTop * (scrollMax - scrollPage));

                int newScrollPos = dragStartScrollPos + deltaScroll;
                if (newScrollPos < 0) newScrollPos = 0;
                if (newScrollPos > scrollMax - scrollPage) newScrollPos = scrollMax - scrollPage;

                SetProp(hwnd, L"ScrollPos", (HANDLE)(INT_PTR)newScrollPos);
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }

            int scrollPos = (int)(INT_PTR)GetProp(hwnd, L"ScrollPos");
            int scrollMax = (int)(INT_PTR)GetProp(hwnd, L"ScrollMax");
            int scrollPage = (int)(INT_PTR)GetProp(hwnd, L"ScrollPage");

            if (scrollMax > scrollPage) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);

                RECT thumbRect;
                CalculateScrollbarThumbRect(clientRect, scrollPos, scrollMax, scrollPage, &thumbRect);

                BOOL wasHovered = (BOOL)(INT_PTR)GetProp(hwnd, L"ThumbHovered");
                BOOL isHovered = PtInRect(&thumbRect, pt);

                if (wasHovered != isHovered) {
                    SetProp(hwnd, L"ThumbHovered", (HANDLE)(INT_PTR)isHovered);
                    InvalidateRect(hwnd, NULL, TRUE);

                    if (isHovered) {
                        TRACKMOUSEEVENT tme = {0};
                        tme.cbSize = sizeof(TRACKMOUSEEVENT);
                        tme.dwFlags = TME_LEAVE;
                        tme.hwndTrack = hwnd;
                        TrackMouseEvent(&tme);
                    }
                }
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSELEAVE: {
            if ((INT_PTR)GetProp(hwnd, L"ThumbHovered")) {
                SetProp(hwnd, L"ThumbHovered", (HANDLE)0);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int wheelScrollLines = 3;
            SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &wheelScrollLines, 0);

            int scrollAmount = wheelScrollLines * 20;

            int scrollPos = (int)(INT_PTR)GetProp(hwnd, L"ScrollPos");
            int scrollMax = (int)(INT_PTR)GetProp(hwnd, L"ScrollMax");
            int scrollPage = (int)(INT_PTR)GetProp(hwnd, L"ScrollPage");

            scrollPos -= (delta > 0 ? scrollAmount : -scrollAmount);

            if (scrollPos < 0) scrollPos = 0;
            if (scrollPos > scrollMax - scrollPage) scrollPos = scrollMax - scrollPage;

            SetProp(hwnd, L"ScrollPos", (HANDLE)(INT_PTR)scrollPos);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_SETCURSOR: {
            MarkdownLink* links = (MarkdownLink*)GetProp(hwnd, L"MarkdownLinks");
            int linkCount = (int)(INT_PTR)GetProp(hwnd, L"LinkCount");
            int scrollPos = (int)(INT_PTR)GetProp(hwnd, L"ScrollPos");

            if (links && linkCount > 0) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                pt.x += 5;
                pt.y += scrollPos + 5;

                const wchar_t* url = GetClickedLinkUrl(links, linkCount, pt);
                if (url) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        }

        case WM_NCDESTROY:
            RemoveProp(hwnd, L"ScrollPos");
            RemoveProp(hwnd, L"ScrollMax");
            RemoveProp(hwnd, L"ScrollPage");
            RemoveProp(hwnd, L"ThumbDragging");
            RemoveProp(hwnd, L"DragStartY");
            RemoveProp(hwnd, L"DragStartScrollPos");
            RemoveProp(hwnd, L"ThumbHovered");
            RemoveProp(hwnd, L"MarkdownLinks");
            RemoveProp(hwnd, L"LinkCount");
            RemoveWindowSubclass(hwnd, NotesControlProc, uIdSubclass);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
