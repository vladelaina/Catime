    track.dwFlags = TME_LEAVE | TME_NONCLIENT;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

static BOOL ModernControlOwnsVerticalScroll(const ModernControl* control) {
    if (!control || !control->hwnd) return FALSE;
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    if ((style & WS_VSCROLL) != 0) return TRUE;
    if (control->kind == MODERN_CONTROL_LIST ||
        (control->kind == MODERN_CONTROL_COMBO &&
         !ModernIsDateTimeControl(control))) {
        return TRUE;
    }
    if (control->kind == MODERN_CONTROL_FIELD &&
        (style & ES_MULTILINE) != 0 && !ModernIsCompactEdit(control)) {
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK ModernDateTimeChildSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    (void)wParam;
    (void)refData;
    switch (msg) {
        case WM_WINDOWPOSCHANGING:
            if (lParam) {
                WINDOWPOS* position = (WINDOWPOS*)lParam;
                position->flags &= ~SWP_SHOWWINDOW;
                position->flags |= SWP_HIDEWINDOW;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernDateTimeChildSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void ModernApplyComboListRegion(HWND hwnd, ModernControl* control) {
    if (!hwnd || !control || !control->owner) return;
    RECT client = {0};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    UINT dpi = control->owner->dpi;
    if (width <= 0 || height <= 0 ||
        (control->comboListRegionWidth == width &&
         control->comboListRegionHeight == height &&
         control->comboListRegionDpi == dpi)) {
        return;
    }
    int radius = DialogModern_Scale(control->owner->dpi, 9);
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                     radius * 2, radius * 2);
    if (!region) return;
    if (SetWindowRgn(hwnd, region, FALSE)) {
        control->comboListRegionWidth = width;
        control->comboListRegionHeight = height;
        control->comboListRegionDpi = dpi;
    } else {
        DeleteObject(region);
    }
}

static void ModernInvalidateComboListItem(HWND hwnd, int itemIndex) {
    if (!hwnd || itemIndex < 0) return;
    RECT itemRect = {0};
    if (SendMessageW(hwnd, LB_GETITEMRECT, itemIndex,
                     (LPARAM)&itemRect) != LB_ERR) {
        InvalidateRect(hwnd, &itemRect, FALSE);
    }
}

static int ModernGetComboListVisibleItems(
    HWND hwnd, const ModernControl* control) {
    if (!hwnd || !control || !control->owner) return 1;
    int itemHeight = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, 0, 0);
    if (itemHeight <= 0) {
        itemHeight = DialogModern_Scale(control->owner->dpi, 34);
    }
    RECT client = {0};
    GetClientRect(hwnd, &client);
    return max(1, (client.bottom - client.top) / max(1, itemHeight));
}

static BOOL ModernPointInComboScrollbar(
    const ModernControl* control, const RECT* rect, POINT point) {
    if (!control || !control->owner || !rect) return FALSE;
    RECT hitRect = *rect;
    InflateRect(&hitRect, DialogModern_Scale(control->owner->dpi, 5), 0);
    return PtInRect(&hitRect, point);
}

static BOOL ModernGetComboListScrollbarRects(
    HWND hwnd, ModernControl* control, RECT* track, RECT* thumb) {
    if (!hwnd || !control || !control->owner || !track || !thumb) {
        return FALSE;
    }
    int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
    if (count <= 0) return FALSE;
    RECT client = {0};
    GetClientRect(hwnd, &client);
    int margin = DialogModern_Scale(control->owner->dpi, 7);
    int width = DialogModern_Scale(control->owner->dpi, 4);
    int visibleItems = ModernGetComboListVisibleItems(hwnd, control);
    if (count <= visibleItems) return FALSE;

    track->right = client.right - margin;
    track->left = track->right - width;
    track->top = client.top + margin;
    track->bottom = client.bottom - margin;
    int trackHeight = max(1, track->bottom - track->top);
    int thumbHeight = MulDiv(trackHeight, visibleItems, count);
    int minimumThumb = DialogModern_Scale(control->owner->dpi, 24);
    if (thumbHeight < minimumThumb) thumbHeight = minimumThumb;
    if (thumbHeight > trackHeight) thumbHeight = trackHeight;

    int topIndex = (int)SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0);
    int maximumTop = max(1, count - visibleItems);
    if (topIndex < 0) topIndex = 0;
    if (topIndex > maximumTop) topIndex = maximumTop;
    int travel = trackHeight - thumbHeight;
    int thumbTop = track->top + MulDiv(travel, topIndex, maximumTop);
    *thumb = *track;
    thumb->left -= DialogModern_Scale(control->owner->dpi, 2);
    thumb->right += DialogModern_Scale(control->owner->dpi, 2);
    thumb->top = thumbTop;
    thumb->bottom = thumbTop + thumbHeight;
    return TRUE;
}

static void ModernDrawComboListScrollbar(
    HWND hwnd, ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !hdc) return;
    RECT track = {0};
    RECT thumb = {0};
    if (!ModernGetComboListScrollbarRects(hwnd, control, &track, &thumb)) return;
    COLORREF trackColor = ModernBlendColor(
        state->palette.field, state->palette.surface,
        state->palette.darkMode ? 26 : 48);
    COLORREF thumbColor = control->comboScrollDragging
        ? state->palette.accentHover
        : (control->comboScrollHovered ? state->palette.accent
                                       : state->palette.border);
    DialogModern_DrawRoundedRect(
        hdc, &track, DialogModern_Scale(state->dpi, 4),
        trackColor, trackColor, 0);
    DialogModern_DrawRoundedRect(
        hdc, &thumb, DialogModern_Scale(state->dpi, 6),
        thumbColor, thumbColor, 0);
}

static void ModernDrawComboListFrame(
    HWND hwnd, ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !hdc) return;
    RECT client = {0};
    GetClientRect(hwnd, &client);
    InflateRect(&client, -1, -1);
    HPEN pen = CreatePen(PS_SOLID, 1, state->palette.border);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, client.left, client.top, client.right, client.bottom,
              DialogModern_Scale(state->dpi, 10),
              DialogModern_Scale(state->dpi, 10));
    if (oldBrush) SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void ModernDrawComboList(HWND hwnd, ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!hwnd || !state || !hdc) return;

    RECT client = {0};
    GetClientRect(hwnd, &client);
    COLORREF popupSurface = state->palette.darkMode
        ? ModernBlendColor(state->palette.surface, state->palette.field, 58)
        : state->palette.surface;
    HBRUSH popupBrush = CreateSolidBrush(popupSurface);
    if (popupBrush) {
        FillRect(hdc, &client, popupBrush);
        DeleteObject(popupBrush);
    }

    int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
    int topIndex = (int)SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0);
    int selectedIndex = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
    if (topIndex < 0) topIndex = 0;
    if (count < 0) count = 0;
    BOOL enabled = IsWindowEnabled(control->hwnd) && IsWindowEnabled(hwnd);

    int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, client.top,
                      client.right, client.bottom);
    for (int itemIndex = topIndex; itemIndex < count; itemIndex++) {
        RECT itemRect = {0};
        if (SendMessageW(hwnd, LB_GETITEMRECT, itemIndex,
                         (LPARAM)&itemRect) == LB_ERR) {
            break;
        }
        if (itemRect.top >= client.bottom) break;
        if (itemRect.bottom <= client.top) continue;

        UINT itemState = 0;
        if (itemIndex == selectedIndex) itemState |= ODS_SELECTED;
        if (!enabled) itemState |= ODS_DISABLED;
        ModernDrawComboItemContent(control, hdc, &itemRect,
                                   (UINT)itemIndex, itemState);
    }
    if (savedDc != 0) RestoreDC(hdc, savedDc);

    ModernDrawComboListFrame(hwnd, control, hdc);
    ModernDrawComboListScrollbar(hwnd, control, hdc);
}

static void ModernPaintComboList(HWND hwnd, ModernControl* control,
                                 HDC suppliedDc) {
    if (!hwnd || !control || !control->owner) return;

    PAINTSTRUCT paint = {0};
    HDC target = suppliedDc ? suppliedDc : BeginPaint(hwnd, &paint);
    if (!target) return;

    RECT client = {0};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = width > 0 && height > 0 ? CreateCompatibleDC(target) : NULL;
    HBITMAP bitmap = buffer
        ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap
        ? SelectObject(buffer, bitmap) : NULL;

    if (buffer && bitmap && oldBitmap) {
        ModernDrawComboList(hwnd, control, buffer);
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    } else {
        ModernDrawComboList(hwnd, control, target);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    if (!suppliedDc) EndPaint(hwnd, &paint);
}

static LRESULT CALLBACK ModernComboListSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernControl* control = (ModernControl*)refData;
    ModernDialogState* state = control ? control->owner : NULL;
    switch (msg) {
        case WM_SHOWWINDOW: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control) {
