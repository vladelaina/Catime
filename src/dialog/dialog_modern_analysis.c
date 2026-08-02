/**
 * @file dialog_modern_analysis.c
 * @brief Resource-dialog analysis and compact edit preparation.
 */

#include "dialog_modern_internal.h"

void ModernAnalyzeLayout(ModernDialogState* state) {
    int minX = INT_MAX;
    int minY = INT_MAX;
    int maxX = INT_MIN;
    int maxY = INT_MIN;

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible) continue;
        if (control->source96.left < minX) minX = control->source96.left;
        if (control->source96.top < minY) minY = control->source96.top;
        if (control->source96.right > maxX) maxX = control->source96.right;
        if (control->source96.bottom > maxY) maxY = control->source96.bottom;
    }

    if (minX == INT_MAX) {
        RECT client = {0};
        GetClientRect(state->hwnd, &client);
        minX = minY = 0;
        maxX = ModernTo96(state->dpi, client.right);
        maxY = ModernTo96(state->dpi, client.bottom);
    }

    int totalHeight = maxY - minY;
    int footerThreshold = maxY - 34;
    size_t footerCount = 0;
    int footerWidth = 0;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible ||
            control->kind != MODERN_CONTROL_PUSH) continue;
        if (control->source96.bottom >= footerThreshold ||
            control->source96.top >= minY + totalHeight * 3 / 4) {
            control->footer = TRUE;
            SIZE textSize = {0};
            ModernMeasureText(control->hwnd, state->buttonFont, &textSize);
            int textWidth96 = ModernTo96(state->dpi, textSize.cx);
            int originalWidth = control->source96.right - control->source96.left;
            int desiredWidth = textWidth96 + 30;
            if (desiredWidth < 80) desiredWidth = 80;
            if (desiredWidth < originalWidth) desiredWidth = originalWidth;
            control->source96.right = control->source96.left + desiredWidth;
            footerWidth += desiredWidth;
            footerCount++;
        }
    }
    if (footerCount > 1) footerWidth += (int)(footerCount - 1) * 10;

    int bodyMaxY = minY;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible || control->footer) continue;
        if (control->source96.bottom > bodyMaxY) bodyMaxY = control->source96.bottom;
    }
    if (bodyMaxY <= minY) bodyMaxY = maxY;

    state->contentMinX96 = minX;
    state->contentMinY96 = minY;
    state->contentWidth96 = maxX - minX;
    state->bodyHeight96 = bodyMaxY - minY;
    state->headerHeight96 = state->bodyHeight96 > 620 ? 66 : 72;
    state->sidePadding96 = state->bodyHeight96 > 620 ? 18 : 24;
    state->bottomPadding96 = state->bodyHeight96 > 620 ? 14 : 20;
    state->footerHeight96 = footerCount ? 36 : 0;
    state->hasFooter = footerCount > 0;

    int desiredWidth = state->contentWidth96 + state->sidePadding96 * 2;
    int footerDesiredWidth = footerWidth + state->sidePadding96 * 2;
    if (desiredWidth < footerDesiredWidth) desiredWidth = footerDesiredWidth;

    SIZE titleSize = {0};
    ModernMeasureText(state->hwnd, state->titleFont, &titleSize);
    int titleDesiredWidth = ModernTo96(state->dpi, titleSize.cx) +
                            state->sidePadding96 * 2 + 64;
    if (desiredWidth < titleDesiredWidth) desiredWidth = titleDesiredWidth;
    if (desiredWidth < 360) desiredWidth = 360;
    state->desiredClientWidth96 = desiredWidth;
    state->clientWidth96 = desiredWidth;

    int footerSpace = footerCount ? 24 + state->footerHeight96 : 0;
    state->desiredClientHeight96 = state->headerHeight96 +
                                   state->bodyHeight96 + footerSpace +
                                   state->bottomPadding96;
    state->clientHeight96 = state->desiredClientHeight96;
    state->footerY96 = state->clientHeight96 - state->bottomPadding96 -
                       state->footerHeight96;
}

void ModernSetControlFont(const ModernDialogState* state,
                                 const ModernControl* control) {
    HFONT font = state->bodyFont;
    if (control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO) {
        font = state->editFont;
    } else if (control->kind == MODERN_CONTROL_GROUP) {
        font = state->labelFont;
    } else if (control->kind == MODERN_CONTROL_PUSH ||
               control->kind == MODERN_CONTROL_CLOSE) {
        font = state->buttonFont;
    }
    if (font) SendMessageW(control->hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    if (control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO ||
        control->kind == MODERN_CONTROL_SLIDER) {
        DialogModern_ApplyTheme(control->hwnd, state->palette.darkMode);
    }
    if (control->kind == MODERN_CONTROL_COMBO &&
        !ModernIsDateTimeControl(control)) {
        ModernAttachComboList((ModernControl*)control);
    }
}

BOOL ModernApplyFieldRegionRaw(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return FALSE;
    if (control->kind != MODERN_CONTROL_FIELD &&
        control->kind != MODERN_CONTROL_LIST &&
        control->kind != MODERN_CONTROL_COMBO &&
        control->kind != MODERN_CONTROL_INSTRUCTION) return FALSE;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int radius = DialogModern_Scale(control->owner->dpi, 9);
    HRGN region = CreateRoundRectRgn(client.left, client.top,
                                     client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (!region) return FALSE;
    if (!SetWindowRgn(control->hwnd, region, redraw)) {
        DeleteObject(region);
        return FALSE;
    }
    return TRUE;
}

void ModernApplyFieldRegion(ModernControl* control) {
    if (!control) return;
    control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
    ModernApplyFieldRegionRaw(control, TRUE);
}

/* Compact visual single-line edits use the multiline formatting rectangle so
 * Win32 can center text without replacing its native editing behavior. */
BOOL ModernIsCompactEdit(const ModernControl* control) {
    if (!control || control->kind != MODERN_CONTROL_FIELD ||
        !ModernWindowHasClass(control->hwnd, L"Edit")) {
        return FALSE;
    }

    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    if ((style & (ES_MULTILINE | ES_AUTOHSCROLL)) !=
            (ES_MULTILINE | ES_AUTOHSCROLL) ||
        (style & (ES_WANTRETURN | WS_VSCROLL)) != 0) {
        return FALSE;
    }

    RECT client = {0};
    if (!GetClientRect(control->hwnd, &client)) return FALSE;
    return client.bottom - client.top <=
           DialogModern_Scale(control->owner->dpi, 56);
}

void ModernApplyEditLayout(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || control->kind != MODERN_CONTROL_FIELD ||
        !ModernWindowHasClass(control->hwnd, L"Edit")) {
        return;
    }

    int horizontalInset = DialogModern_Scale(state->dpi, 12);
    if (!ModernIsCompactEdit(control)) {
        SendMessageW(control->hwnd, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELONG(horizontalInset, horizontalInset));
        return;
    }

    RECT client = {0};
    if (!GetClientRect(control->hwnd, &client)) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;

    HFONT font = (HFONT)SendMessageW(control->hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    TEXTMETRICW metrics = {0};
    if (GetTextMetricsW(hdc, &metrics)) {
        int height = client.bottom - client.top;
        int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;
        int verticalInset = max(DialogModern_Scale(state->dpi, 3),
                                (height - lineHeight) / 2);
        RECT formatRect = {
            horizontalInset,
            verticalInset,
            max(horizontalInset + 1, client.right - horizontalInset),
            max(verticalInset + 1, client.bottom - verticalInset)
        };
        SendMessageW(control->hwnd, EM_SETRECTNP, 0,
                     (LPARAM)&formatRect);
    }
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(control->hwnd, hdc);
}

wchar_t* ModernCreateSingleLineText(const wchar_t* source,
                                           size_t length,
                                           BOOL* changed) {
    if (changed) *changed = FALSE;
    if (!source) return NULL;

    wchar_t* result = (wchar_t*)malloc((length + 1) * sizeof(*result));
    if (!result) return NULL;

    size_t output = 0;
    BOOL pendingSpace = FALSE;
    for (size_t i = 0; i < length; i++) {
        wchar_t ch = source[i];
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
            pendingSpace = output > 0;
            if (changed) *changed = TRUE;
            continue;
        }
        if (pendingSpace && result[output - 1] != L' ') {
            result[output++] = L' ';
        }
        pendingSpace = FALSE;
        result[output++] = ch;
    }
    result[output] = L'\0';
    return result;
}
