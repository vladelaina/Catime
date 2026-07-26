/**
 * @file window_message_menu_draw.c
 * @brief Measures and paints owner-drawn color menu entries.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "color/color.h"
#include "color/gradient.h"
#include "tray/tray_menu_submenus.h"
#include "tray/tray_menu_theme.h"
#include "../resource/resource.h"

#include <stdio.h>

#define BUFFER_SIZE_MENU_ITEM 100

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

    BOOL darkMenu = IsNativeMenuDarkModeActive();
    COLORREF itemBackground = darkMenu
        ? ((lpdis->itemState & ODS_SELECTED)
               ? RGB(62, 62, 62)
               : RGB(32, 32, 32))
        : GetSysColor((lpdis->itemState & ODS_SELECTED)
                          ? COLOR_HIGHLIGHT
                          : COLOR_MENU);
    HBRUSH backgroundBrush = CreateSolidBrush(itemBackground);
    if (backgroundBrush) {
        FillRect(lpdis->hDC, &lpdis->rcItem, backgroundBrush);
        DeleteObject(backgroundBrush);
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
        ColorStringToColorRef(hexColor, &color);

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
    COLORREF numberColor = darkMenu
        ? RGB(255, 255, 255)
        : GetSysColor((lpdis->itemState & ODS_SELECTED)
                          ? COLOR_HIGHLIGHTTEXT
                          : COLOR_MENUTEXT);
    COLORREF oldTextColor = SetTextColor(lpdis->hDC, numberColor);
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
