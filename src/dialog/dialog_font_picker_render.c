/**
 * @file dialog_font_picker_render.c
 * @brief Owner-drawn font list rendering.
 */

#include "dialog_font_picker_internal.h"
#include "dialog/dialog_modern.h"
#include "../../resource/resource.h"

BOOL DialogFontPickerInternal_OnMeasureItem(HWND hdlg, LPARAM lp) {
    MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
    if (!mis || mis->CtlID != IDC_FONT_LIST_SIMPLE) {
        return FALSE;
    }

    HDC hdc = GetDC(hdlg);
    if (!hdc) {
        mis->itemHeight = 20;
        return TRUE;
    }

    TEXTMETRIC tm = {0};
    if (!GetTextMetrics(hdc, &tm)) {
        ReleaseDC(hdlg, hdc);
        mis->itemHeight = 20;
        return TRUE;
    }
    ReleaseDC(hdlg, hdc);
    mis->itemHeight = tm.tmHeight + 4;
    return TRUE;
}

BOOL DialogFontPickerInternal_OnDrawItem(HWND hdlg, LPARAM lp) {
    DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
    if (!dis || dis->CtlID != IDC_FONT_LIST_SIMPLE) {
        return FALSE;
    }
    if (dis->itemID == (UINT)-1) {
        return TRUE;
    }

    wchar_t text[LF_FACESIZE];
    LRESULT textLen = SendMessageW(dis->hwndItem, LB_GETTEXT,
                                   dis->itemID, (LPARAM)text);
    if (textLen == LB_ERR) {
        return TRUE;
    }

    DialogModernPalette palette;
    DialogModern_CopyPalette(hdlg, &palette);
    BOOL selected = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF bgColor = selected ? palette.accent : palette.field;
    COLORREF txtColor = selected ?
        (palette.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                                RGB(0xFF, 0xFF, 0xFF)) :
        palette.text;
    COLORREF oldTextColor = GetTextColor(dis->hDC);
    int oldBkMode = SetBkMode(dis->hDC, TRANSPARENT);

    HBRUSH background = CreateSolidBrush(bgColor);
    if (background) {
        FillRect(dis->hDC, &dis->rcItem, background);
        DeleteObject(background);
    }
    if ((dis->itemState & ODS_FOCUS) && !selected) {
        RECT focusMark = dis->rcItem;
        focusMark.right = focusMark.left +
            DialogModern_Scale(DialogModern_GetDpi(hdlg), 2);
        HBRUSH accent = CreateSolidBrush(palette.accent);
        if (accent) {
            FillRect(dis->hDC, &focusMark, accent);
            DeleteObject(accent);
        }
    }

    int textLeft = dis->rcItem.left + 4;
    if ((int)dis->itemID == g_currentFontIndex) {
        SetTextColor(dis->hDC, txtColor);
        TextOutW(dis->hDC, dis->rcItem.left + 2, dis->rcItem.top + 2,
                 L"✓", 1);
        textLeft += 16;
    }

    SetTextColor(dis->hDC, txtColor);
    RECT textRect = dis->rcItem;
    textRect.left = textLeft;
    DrawTextW(dis->hDC, text, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (oldTextColor != CLR_INVALID) {
        SetTextColor(dis->hDC, oldTextColor);
    }
    SetBkMode(dis->hDC, oldBkMode);
    return TRUE;
}
