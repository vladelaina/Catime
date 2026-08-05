#include "update/update_internal.h"

int UpdateClampScrollPosition(int position, int maximum, int page) {
    if (maximum <= page || page <= 0) return 0;
    int limit = maximum - page;
    if (position < 0) return 0;
    return position > limit ? limit : position;
}

void CalculateScrollbarThumbRect(RECT clientRect, int scrollPos,
                                 int scrollMax, int scrollPage,
                                 RECT* outThumbRect) {
    if (!outThumbRect) return;
    int trackHeight = clientRect.bottom - clientRect.top;
    if (trackHeight <= 0 || scrollMax <= scrollPage || scrollPage <= 0) {
        SetRectEmpty(outThumbRect);
        return;
    }

    scrollPos = UpdateClampScrollPosition(scrollPos, scrollMax, scrollPage);
    int thumbHeight = MulDiv(scrollPage, trackHeight, scrollMax);
    if (thumbHeight < MODERN_SCROLLBAR_MIN_THUMB) {
        thumbHeight = MODERN_SCROLLBAR_MIN_THUMB;
    }
    if (thumbHeight > trackHeight) thumbHeight = trackHeight;

    int travel = trackHeight - thumbHeight;
    int range = scrollMax - scrollPage;
    int thumbTop = travel > 0 ? MulDiv(scrollPos, travel, range) : 0;
    outThumbRect->left = clientRect.right - MODERN_SCROLLBAR_WIDTH -
                      MODERN_SCROLLBAR_MARGIN;
    outThumbRect->top = clientRect.top + thumbTop;
    outThumbRect->right = clientRect.right - MODERN_SCROLLBAR_MARGIN;
    outThumbRect->bottom = outThumbRect->top + thumbHeight;
}

void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color) {
    if (!hdc || IsRectEmpty(&rect)) return;
    HGDIOBJ brush = GetStockObject(DC_BRUSH);
    HGDIOBJ pen = GetStockObject(DC_PEN);
    if (!brush || !pen) return;

    COLORREF previousBrushColor = SetDCBrushColor(hdc, color);
    COLORREF previousPenColor = SetDCPenColor(hdc, color);
    HGDIOBJ previousBrush = SelectObject(hdc, brush);
    HGDIOBJ previousPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom,
              radius, radius);
    if (previousPen) SelectObject(hdc, previousPen);
    if (previousBrush) SelectObject(hdc, previousBrush);
    if (previousPenColor != CLR_INVALID) {
        SetDCPenColor(hdc, previousPenColor);
    }
    if (previousBrushColor != CLR_INVALID) {
        SetDCBrushColor(hdc, previousBrushColor);
    }
}
