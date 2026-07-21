/**
 * @file dialog_modern.c
 * @brief Shared modern chrome, layout, and control drawing for resource dialogs.
 */

#include "dialog/dialog_modern.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <limits.h>
#include <stdlib.h>
#include <strsafe.h>
#include <wchar.h>
#include <windowsx.h>

#define MODERN_DIALOG_STATE_PROP L"Catime.ModernDialog.State"
#define MODERN_DIALOG_CLOSE_ID 0x7FEE
#define MODERN_DIALOG_SUBCLASS_ID 0xD140
#define MODERN_CONTROL_SUBCLASS_ID 0xD141
#define MODERN_DATETIME_CHILD_SUBCLASS_ID 0xD143
#define MODERN_COMBO_LIST_SUBCLASS_ID 0xD144
#define MODERN_DIALOG_FINALIZE_MESSAGE (WM_APP + 490)

typedef enum {
    MODERN_CONTROL_OTHER = 0,
    MODERN_CONTROL_PUSH,
    MODERN_CONTROL_CLOSE,
    MODERN_CONTROL_CHECK,
    MODERN_CONTROL_RADIO,
    MODERN_CONTROL_GROUP,
    MODERN_CONTROL_FIELD,
    MODERN_CONTROL_LIST,
    MODERN_CONTROL_COMBO,
    MODERN_CONTROL_SLIDER
} ModernControlKind;

typedef struct ModernDialogState ModernDialogState;

typedef struct {
    ModernDialogState* owner;
    HWND hwnd;
    int id;
    ModernControlKind kind;
    RECT source96;
    BOOL sourceVisible;
    BOOL footer;
    BOOL primary;
    BOOL hovered;
    BOOL pressed;
    BOOL focused;
} ModernControl;

struct ModernDialogState {
    HWND hwnd;
    int dialogType;
    UINT dpi;
    DialogModernPalette palette;
    HBRUSH backgroundBrush;
    HBRUSH surfaceBrush;
    HBRUSH fieldBrush;
    HFONT titleFont;
    HFONT bodyFont;
    HFONT labelFont;
    HFONT editFont;
    HFONT buttonFont;
    ModernControl* controls;
    size_t controlCount;
    size_t controlCapacity;
    HWND closeButton;
    int contentMinX96;
    int contentMinY96;
    int contentWidth96;
    int bodyHeight96;
    int desiredClientWidth96;
    int desiredClientHeight96;
    int clientWidth96;
    int clientHeight96;
    int headerHeight96;
    int sidePadding96;
    int bottomPadding96;
    int footerHeight96;
    int footerY96;
    int bodyViewportHeight96;
    int bodyScrollOffset96;
    int bodyScrollMax96;
    int scrollDragStartY;
    int scrollDragStartOffset96;
    BOOL hasFooter;
    BOOL scrollBarHovered;
    BOOL scrollBarDragging;
    BOOL attached;
    BOOL finalized;
    BOOL finalizing;
    BOOL refreshing;
    BOOL refreshPending;
};

static LRESULT CALLBACK ModernDialogSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
                                                 DWORD_PTR refData);
static LRESULT CALLBACK ModernControlSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData);
static LRESULT CALLBACK ModernDateTimeChildSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);
static LRESULT CALLBACK ModernComboListSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);
static void ModernLayoutControls(ModernDialogState* state);
static void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96);
static void ModernSyncClientSizeFromWindow(ModernDialogState* state);
static BOOL ModernControlOwnsVerticalScroll(const ModernControl* control);
static void ModernAttachComboList(ModernControl* control);
static int ModernTo96(UINT dpi, int value);

static ModernDialogState* ModernGetState(HWND hwnd) {
    return hwnd ? (ModernDialogState*)GetPropW(hwnd, MODERN_DIALOG_STATE_PROP)
                : NULL;
}

BOOL DialogModern_CopyPalette(HWND hwnd, DialogModernPalette* palette) {
    if (!palette) return FALSE;
    const ModernDialogState* state = ModernGetState(hwnd);
    if (!state && hwnd) state = ModernGetState(GetParent(hwnd));
    if (state && state->surfaceBrush) {
        *palette = state->palette;
        return TRUE;
    }
    DialogModern_ResolvePalette(palette);
    return TRUE;
}

BOOL DialogModern_IsAttached(HWND hwndDlg) {
    return ModernGetState(hwndDlg) != NULL;
}

static void ModernDeleteFonts(ModernDialogState* state) {
    if (!state) return;
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->bodyFont) DeleteObject(state->bodyFont);
    if (state->labelFont) DeleteObject(state->labelFont);
    if (state->editFont) DeleteObject(state->editFont);
    if (state->buttonFont) DeleteObject(state->buttonFont);
    state->titleFont = NULL;
    state->bodyFont = NULL;
    state->labelFont = NULL;
    state->editFont = NULL;
    state->buttonFont = NULL;
}

static void ModernDeleteBrushes(ModernDialogState* state) {
    if (!state) return;
    if (state->backgroundBrush) DeleteObject(state->backgroundBrush);
    if (state->surfaceBrush) DeleteObject(state->surfaceBrush);
    if (state->fieldBrush) DeleteObject(state->fieldBrush);
    state->backgroundBrush = NULL;
    state->surfaceBrush = NULL;
    state->fieldBrush = NULL;
}

static void ModernRebuildResources(ModernDialogState* state) {
    if (!state) return;
    ModernDeleteFonts(state);
    ModernDeleteBrushes(state);
    DialogModern_ResolvePalette(&state->palette);
    DialogModern_ApplyTheme(state->hwnd, state->palette.darkMode);
    state->backgroundBrush = CreateSolidBrush(state->palette.background);
    state->surfaceBrush = CreateSolidBrush(state->palette.surface);
    state->fieldBrush = CreateSolidBrush(state->palette.field);
    state->titleFont = DialogModern_CreateFont(state->dpi, 20, FW_SEMIBOLD);
    state->bodyFont = DialogModern_CreateFont(state->dpi, 12, FW_NORMAL);
    state->labelFont = DialogModern_CreateFont(state->dpi, 11, FW_SEMIBOLD);
    state->editFont = DialogModern_CreateFont(state->dpi, 14, FW_NORMAL);
    state->buttonFont = DialogModern_CreateFont(state->dpi, 12, FW_SEMIBOLD);
}

static int ModernTo96(UINT dpi, int value) {
    return MulDiv(value, 96, (int)(dpi ? dpi : 96u));
}

static BOOL ModernEnsureControlCapacity(ModernDialogState* state, size_t count) {
    if (state->controlCapacity >= count) return TRUE;
    size_t capacity = state->controlCapacity ? state->controlCapacity * 2 : 32;
    while (capacity < count) capacity *= 2;
    ModernControl* controls =
        (ModernControl*)realloc(state->controls, capacity * sizeof(*controls));
    if (!controls) return FALSE;
    state->controls = controls;
    state->controlCapacity = capacity;
    return TRUE;
}

static ModernControlKind ModernClassifyControl(HWND hwnd) {
    wchar_t className[64] = {0};
    GetClassNameW(hwnd, className, _countof(className));
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    if (_wcsicmp(className, L"Button") == 0) {
        UINT type = (UINT)style & BS_TYPEMASK;
        if (type == BS_GROUPBOX) return MODERN_CONTROL_GROUP;
        if (type == BS_AUTOCHECKBOX || type == BS_CHECKBOX ||
            type == BS_3STATE || type == BS_AUTO3STATE) {
            return MODERN_CONTROL_CHECK;
        }
        if (type == BS_AUTORADIOBUTTON || type == BS_RADIOBUTTON) {
            return MODERN_CONTROL_RADIO;
        }
        if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON ||
            type == BS_OWNERDRAW) {
            return MODERN_CONTROL_PUSH;
        }
    }
    if (_wcsicmp(className, L"Edit") == 0 ||
        _wcsicmp(className, L"msctls_hotkey32") == 0) {
        return MODERN_CONTROL_FIELD;
    }
    if (_wcsicmp(className, L"ListBox") == 0) return MODERN_CONTROL_LIST;
    if (_wcsicmp(className, L"ComboBox") == 0 ||
        _wcsicmp(className, L"SysDateTimePick32") == 0) {
        return MODERN_CONTROL_COMBO;
    }
    if (_wcsicmp(className, L"msctls_trackbar32") == 0) {
        return MODERN_CONTROL_SLIDER;
    }
    return MODERN_CONTROL_OTHER;
}

static BOOL ModernWindowHasClass(HWND hwnd, const wchar_t* expected) {
    wchar_t className[64] = {0};
    return hwnd && expected &&
           GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           _wcsicmp(className, expected) == 0;
}

static BOOL ModernIsDateTimeControl(const ModernControl* control) {
    return control && control->kind == MODERN_CONTROL_COMBO &&
           ModernWindowHasClass(control->hwnd, L"SysDateTimePick32");
}

static BOOL CALLBACK ModernAttachDateTimeChild(HWND child, LPARAM data) {
    ModernControl* control = (ModernControl*)data;
    if (ModernWindowHasClass(child, L"msctls_updown32")) {
        SetWindowSubclass(child, ModernDateTimeChildSubclassProc,
                          MODERN_DATETIME_CHILD_SUBCLASS_ID,
                          (DWORD_PTR)control);
    }
    return TRUE;
}

static BOOL ModernIsPrimaryButton(int id) {
    return id == IDOK || id == IDYES || id == CLOCK_IDC_BUTTON_OK ||
           id == IDC_PLUGIN_SECURITY_TRUST_BTN ||
           id == IDC_FONT_LICENSE_AGREE_BTN;
}

static BOOL CALLBACK ModernCaptureChild(HWND child, LPARAM lParam) {
    ModernDialogState* state = (ModernDialogState*)lParam;
    if (!state || GetParent(child) != state->hwnd) {
        return TRUE;
    }
    if (!ModernEnsureControlCapacity(state, state->controlCount + 1)) {
        return FALSE;
    }

    ModernControl* control = &state->controls[state->controlCount++];
    ZeroMemory(control, sizeof(*control));
    control->owner = state;
    control->hwnd = child;
    control->id = GetDlgCtrlID(child);
    control->kind = ModernClassifyControl(child);
    control->primary = ModernIsPrimaryButton(control->id);
    control->sourceVisible =
        (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0;

    RECT rect = {0};
    GetWindowRect(child, &rect);
    MapWindowPoints(NULL, state->hwnd, (POINT*)&rect, 2);
    control->source96.left = ModernTo96(state->dpi, rect.left);
    control->source96.top = ModernTo96(state->dpi, rect.top);
    control->source96.right = ModernTo96(state->dpi, rect.right);
    control->source96.bottom = ModernTo96(state->dpi, rect.bottom);
    return TRUE;
}

static int ModernControlCompareX(const void* left, const void* right) {
    const ModernControl* const* a = (const ModernControl* const*)left;
    const ModernControl* const* b = (const ModernControl* const*)right;
    return (*a)->source96.left - (*b)->source96.left;
}

static void ModernMeasureText(HWND hwnd, HFONT font, SIZE* size) {
    if (!size) return;
    size->cx = 0;
    size->cy = 0;
    wchar_t text[256] = {0};
    GetWindowTextW(hwnd, text, (int)_countof(text));
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), size);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
}

static void ModernAnalyzeLayout(ModernDialogState* state) {
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

static void ModernSetControlFont(const ModernDialogState* state,
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

static void ModernApplyFieldRegion(ModernControl* control) {
    if (!control || !control->hwnd) return;
    if (control->kind != MODERN_CONTROL_FIELD &&
        control->kind != MODERN_CONTROL_LIST &&
        control->kind != MODERN_CONTROL_COMBO) return;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int radius = DialogModern_Scale(control->owner->dpi, 9);
    HRGN region = CreateRoundRectRgn(client.left, client.top,
                                     client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (region && !SetWindowRgn(control->hwnd, region, TRUE)) {
        DeleteObject(region);
    }
}

static void ModernStyleControl(ModernDialogState* state,
                               ModernControl* control) {
    if (!state || !control || !control->hwnd) return;
    ModernSetControlFont(state, control);

    if (control->kind == MODERN_CONTROL_PUSH) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        style = (style & ~(BS_TYPEMASK | BS_NOTIFY)) | BS_OWNERDRAW;
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style);
    } else if (control->kind == MODERN_CONTROL_FIELD ||
               control->kind == MODERN_CONTROL_LIST ||
               control->kind == MODERN_CONTROL_COMBO) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        LONG_PTR exStyle = GetWindowLongPtrW(control->hwnd, GWL_EXSTYLE);
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style & ~WS_BORDER);
        SetWindowLongPtrW(control->hwnd, GWL_EXSTYLE,
                          exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        if (control->kind == MODERN_CONTROL_FIELD) {
            SendMessageW(control->hwnd, EM_SETMARGINS,
                         EC_LEFTMARGIN | EC_RIGHTMARGIN,
                         MAKELONG(DialogModern_Scale(state->dpi, 10),
                                  DialogModern_Scale(state->dpi, 10)));
        }
        if (control->kind == MODERN_CONTROL_COMBO &&
            !ModernIsDateTimeControl(control)) {
            style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            SetWindowLongPtrW(control->hwnd, GWL_STYLE,
                              style | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, (WPARAM)-1,
                         DialogModern_Scale(state->dpi, 30));
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, 0,
                         DialogModern_Scale(state->dpi, 34));
            ModernAttachComboList(control);
        }
        ModernApplyFieldRegion(control);
    }

    if (ModernIsDateTimeControl(control)) {
        EnumChildWindows(control->hwnd, ModernAttachDateTimeChild,
                         (LPARAM)control);
    }

    if (control->kind == MODERN_CONTROL_PUSH ||
        control->kind == MODERN_CONTROL_CHECK ||
        control->kind == MODERN_CONTROL_RADIO ||
        control->kind == MODERN_CONTROL_GROUP ||
        control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO ||
        control->kind == MODERN_CONTROL_SLIDER) {
        SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                          MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control);
    }
}

/* Owner-draw buttons cannot retain BS_DEFPUSHBUTTON in their type mask.
 * Restore the dialog manager's default-command contract explicitly so Enter
 * keeps activating the same primary action after visual styling. */
static void ModernSetDefaultButton(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    for (size_t i = 0; i < state->controlCount; i++) {
        const ModernControl* control = &state->controls[i];
        if (control->sourceVisible && control->kind == MODERN_CONTROL_PUSH &&
            control->primary) {
            SendMessageW(state->hwnd, DM_SETDEFID, (WPARAM)control->id, 0);
            return;
        }
    }
}

static void ModernUpdateBodyScrollMetrics(ModernDialogState* state) {
    if (!state) return;

    int viewportBottom96 = state->hasFooter
        ? state->footerY96 - 16
        : state->clientHeight96 - state->bottomPadding96;
    int viewportHeight96 = viewportBottom96 - state->headerHeight96;
    if (viewportHeight96 < 1) viewportHeight96 = 1;

    state->bodyViewportHeight96 = viewportHeight96;
    state->bodyScrollMax96 =
        state->bodyHeight96 > viewportHeight96
            ? state->bodyHeight96 - viewportHeight96
            : 0;
    if (state->bodyScrollOffset96 < 0) {
        state->bodyScrollOffset96 = 0;
    }
    if (state->bodyScrollOffset96 > state->bodyScrollMax96) {
        state->bodyScrollOffset96 = state->bodyScrollMax96;
    }
    if (state->bodyScrollMax96 == 0) {
        state->scrollBarHovered = FALSE;
        state->scrollBarDragging = FALSE;
        if (GetCapture() == state->hwnd) ReleaseCapture();
    }
}

static void ModernApplyBodyControlRegion(
    ModernDialogState* state, ModernControl* control, int y96) {
    if (!state || !control || !control->hwnd || !control->sourceVisible) {
        return;
    }

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    int controlTop = DialogModern_Scale(state->dpi, y96);
    int viewportTop =
        DialogModern_Scale(state->dpi, state->headerHeight96);
    int viewportBottom = DialogModern_Scale(
        state->dpi,
        state->headerHeight96 + state->bodyViewportHeight96);
    int visibleTop = viewportTop - controlTop;
    int visibleBottom = viewportBottom - controlTop;

    /* Empty window regions are not reliable for child controls on all
     * supported Windows builds: SetWindowRgn may reject them and leave the
     * previous pixels visible. Hide controls that are completely outside the
     * viewport, and only use a non-empty region for partial clipping. */
    if (visibleBottom <= 0 || visibleTop >= height) {
        SetWindowRgn(control->hwnd, NULL, TRUE);
        ShowWindow(control->hwnd, SW_HIDE);
        return;
    }

    int viewportHeight = viewportBottom - viewportTop;
    BOOL fullyInside = visibleTop <= 0 && visibleBottom >= height;

    /* Group boxes are decorative sibling windows; their labelled frame must
     * not be moved or shortened as it scrolls out of the body.  Once the
     * group title is above the viewport, hide only the frame.  Its sibling
     * controls remain visible and continue to be clipped independently. */
    if (!fullyInside && control->kind == MODERN_CONTROL_GROUP &&
        controlTop < viewportTop) {
        SetWindowRgn(control->hwnd, NULL, TRUE);
        ShowWindow(control->hwnd, SW_HIDE);
        return;
    }

    if (!fullyInside && height > viewportHeight &&
        ModernControlOwnsVerticalScroll(control)) {
        int clippedTop = controlTop < viewportTop ? viewportTop : controlTop;
        int controlBottom = controlTop + height;
        int clippedBottom = controlBottom > viewportBottom
            ? viewportBottom
            : controlBottom;
        int clippedHeight = clippedBottom - clippedTop;
        if (clippedHeight <= 0) {
            SetWindowRgn(control->hwnd, NULL, TRUE);
            ShowWindow(control->hwnd, SW_HIDE);
            return;
        }

        RECT windowRect = {0};
        GetWindowRect(control->hwnd, &windowRect);
        MapWindowPoints(NULL, state->hwnd, (POINT*)&windowRect, 2);
        SetWindowRgn(control->hwnd, NULL, TRUE);
        ShowWindow(control->hwnd, SW_SHOWNA);
        SetWindowPos(control->hwnd, NULL,
                     windowRect.left, clippedTop,
                     windowRect.right - windowRect.left, clippedHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    /* Plain instruction statics should leave the fixed title area clean once
     * their top edge has scrolled above the body viewport. Region-clipping a
     * tall static is rendered inconsistently by PrintWindow on older Windows
     * versions. Owner-draw Markdown panels keep their clipped scrolling. */
    if (!fullyInside && controlTop < viewportTop &&
        control->kind == MODERN_CONTROL_OTHER) {
        wchar_t className[32] = {0};
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        if (GetClassNameW(control->hwnd, className, _countof(className)) &&
            _wcsicmp(className, L"Static") == 0 &&
            (style & SS_OWNERDRAW) == 0) {
            SetWindowRgn(control->hwnd, NULL, TRUE);
            ShowWindow(control->hwnd, SW_HIDE);
            return;
        }
    }

    if (!fullyInside && height <= viewportHeight &&
        control->kind != MODERN_CONTROL_GROUP) {
        SetWindowRgn(control->hwnd, NULL, TRUE);
        ShowWindow(control->hwnd, SW_HIDE);
        return;
    }

    ShowWindow(control->hwnd, SW_SHOWNA);
    if (visibleTop < 0) visibleTop = 0;
    if (visibleBottom > height) visibleBottom = height;

    BOOL fullyVisible = visibleTop == 0 && visibleBottom == height;
    BOOL rounded = control->kind == MODERN_CONTROL_FIELD ||
                   control->kind == MODERN_CONTROL_LIST ||
                   control->kind == MODERN_CONTROL_COMBO;
    if (fullyVisible) {
        if (rounded) {
            ModernApplyFieldRegion(control);
        } else {
            SetWindowRgn(control->hwnd, NULL, TRUE);
        }
        return;
    }

    if (visibleBottom < visibleTop) visibleBottom = visibleTop;
    HRGN shape = NULL;
    if (rounded) {
        int radius = DialogModern_Scale(state->dpi, 9);
        shape = CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                   radius * 2, radius * 2);
    } else {
        shape = CreateRectRgn(0, 0, width, height);
    }
    HRGN clip = CreateRectRgn(0, visibleTop, width, visibleBottom);
    if (!shape || !clip) {
        if (shape) DeleteObject(shape);
        if (clip) DeleteObject(clip);
        return;
    }

    CombineRgn(shape, shape, clip, RGN_AND);
    DeleteObject(clip);
    if (!SetWindowRgn(control->hwnd, shape, TRUE)) {
        DeleteObject(shape);
    }
}

static void ModernLayoutControls(ModernDialogState* state) {
    if (!state || !state->finalized) return;
    ModernUpdateBodyScrollMetrics(state);
    int extraWidth96 = state->clientWidth96 -
                       (state->contentWidth96 + state->sidePadding96 * 2);
    int contentOffsetX96 = state->sidePadding96 +
                           (extraWidth96 > 0 ? extraWidth96 / 2 : 0);

    ModernControl** footer = NULL;
    size_t footerCount = 0;
    if (state->hasFooter) {
        footer = (ModernControl**)calloc(state->controlCount, sizeof(*footer));
    }

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (control->footer) {
            if (footer) footer[footerCount++] = control;
            continue;
        }
        if (control->kind == MODERN_CONTROL_CLOSE) continue;

        int x96 = contentOffsetX96 +
                  (control->source96.left - state->contentMinX96);
        int y96 = state->headerHeight96 +
                  (control->source96.top - state->contentMinY96) -
                  state->bodyScrollOffset96;
        int width96 = control->source96.right - control->source96.left;
        int height96 = control->source96.bottom - control->source96.top;

        /* Markdown/text panels authored as full-width owner-draw statics
         * should use the available surface width when a localized title makes
         * the dialog wider. Narrow canvases (HSV/swatch controls) intentionally
         * remain at their authored dimensions. */
        if (control->kind == MODERN_CONTROL_OTHER) {
            wchar_t className[32] = {0};
            LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            if (GetClassNameW(control->hwnd, className, _countof(className)) &&
                _wcsicmp(className, L"Static") == 0 &&
                (style & SS_OWNERDRAW) != 0 &&
                width96 >= state->contentWidth96 - 24) {
                x96 = state->sidePadding96;
                width96 = state->clientWidth96 -
                          state->sidePadding96 * 2 -
                          (state->bodyScrollMax96 > 0 ? 12 : 0);
            }
        }
        SetWindowPos(control->hwnd, NULL,
                     DialogModern_Scale(state->dpi, x96),
                     DialogModern_Scale(state->dpi, y96),
                     DialogModern_Scale(state->dpi, width96),
                     DialogModern_Scale(state->dpi, height96),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ModernApplyBodyControlRegion(state, control, y96);
    }

    if (footer && footerCount) {
        qsort(footer, footerCount, sizeof(*footer), ModernControlCompareX);
        int totalWidth96 = (int)(footerCount - 1) * 10;
        for (size_t i = 0; i < footerCount; i++) {
            totalWidth96 += footer[i]->source96.right - footer[i]->source96.left;
        }
        int x96 = state->clientWidth96 - state->sidePadding96 - totalWidth96;
        for (size_t i = 0; i < footerCount; i++) {
            ModernControl* control = footer[i];
            int width96 = control->source96.right - control->source96.left;
            SetWindowPos(control->hwnd, NULL,
                         DialogModern_Scale(state->dpi, x96),
                         DialogModern_Scale(state->dpi, state->footerY96),
                         DialogModern_Scale(state->dpi, width96),
                         DialogModern_Scale(state->dpi, state->footerHeight96),
                         SWP_NOZORDER | SWP_NOACTIVATE);
            x96 += width96 + 10;
        }
    }
    free(footer);

    if (state->closeButton) {
        int size96 = 32;
        int x96 = state->clientWidth96 - state->sidePadding96 - size96;
        SetWindowPos(state->closeButton, HWND_TOP,
                     DialogModern_Scale(state->dpi, x96),
                     DialogModern_Scale(state->dpi, 16),
                     DialogModern_Scale(state->dpi, size96),
                     DialogModern_Scale(state->dpi, size96),
                     SWP_NOACTIVATE);
    }
}

static void ModernAttachComboList(ModernControl* control) {
    if (!control || !control->hwnd) return;
    COMBOBOXINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetComboBoxInfo(control->hwnd, &info) || !info.hwndList) return;

    LONG_PTR style = GetWindowLongPtrW(info.hwndList, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(info.hwndList, GWL_EXSTYLE);
    SetWindowLongPtrW(info.hwndList, GWL_STYLE, style & ~WS_BORDER);
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE,
                      exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    DialogModern_ApplyTheme(info.hwndList,
                            control->owner->palette.darkMode);
    SetWindowSubclass(info.hwndList, ModernComboListSubclassProc,
                      MODERN_COMBO_LIST_SUBCLASS_ID,
                      (DWORD_PTR)control);
}

static void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96) {
    if (!state) return;
    ModernUpdateBodyScrollMetrics(state);
    if (offset96 < 0) offset96 = 0;
    if (offset96 > state->bodyScrollMax96) {
        offset96 = state->bodyScrollMax96;
    }
    if (offset96 == state->bodyScrollOffset96) return;

    state->bodyScrollOffset96 = offset96;

    /* Moving, clipping and hiding a large group of child windows one by one
     * exposes intermediate frames on slower machines.  In particular the
     * notification settings page could leave stale group-box and slider
     * pixels behind while dragging its scrollbar.  Freeze composition for
     * the short layout transaction, then repaint the complete dialog once. */
    SendMessageW(state->hwnd, WM_SETREDRAW, FALSE, 0);
    ModernLayoutControls(state);
    SendMessageW(state->hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME |
                 RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static BOOL ModernGetScrollbarRects(const ModernDialogState* state,
                                    RECT* track, RECT* thumb) {
    if (!state || !track || !thumb || state->bodyScrollMax96 <= 0) {
        return FALSE;
    }

    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    int gutter = DialogModern_Scale(state->dpi, 14);
    int width = DialogModern_Scale(state->dpi, 5);
    int margin = DialogModern_Scale(state->dpi, 6);
    track->left = client.right - gutter;
    track->right = track->left + width;
    track->top = DialogModern_Scale(state->dpi, state->headerHeight96) +
                 margin;
    track->bottom = DialogModern_Scale(
                        state->dpi,
                        state->headerHeight96 +
                            state->bodyViewportHeight96) -
                    margin;
    if (track->bottom <= track->top) return FALSE;

    int trackHeight = track->bottom - track->top;
    int thumbHeight = MulDiv(
        trackHeight, state->bodyViewportHeight96, state->bodyHeight96);
    int minimumThumb = DialogModern_Scale(state->dpi, 28);
    if (thumbHeight < minimumThumb) thumbHeight = minimumThumb;
    if (thumbHeight > trackHeight) thumbHeight = trackHeight;

    int travel = trackHeight - thumbHeight;
    int thumbTop = track->top;
    if (travel > 0 && state->bodyScrollMax96 > 0) {
        thumbTop += MulDiv(travel, state->bodyScrollOffset96,
                           state->bodyScrollMax96);
    }
    thumb->left = track->left - DialogModern_Scale(state->dpi, 2);
    thumb->right = track->right + DialogModern_Scale(state->dpi, 2);
    thumb->top = thumbTop;
    thumb->bottom = thumbTop + thumbHeight;
    return TRUE;
}

static void ModernDrawBodyScrollbar(const ModernDialogState* state, HDC hdc) {
    if (!state || !hdc) return;
    RECT track = {0};
    RECT thumb = {0};
    if (!ModernGetScrollbarRects(state, &track, &thumb)) return;

    COLORREF trackColor = state->palette.field;
    COLORREF thumbColor = state->scrollBarDragging
        ? state->palette.accentHover
        : (state->scrollBarHovered ? state->palette.accent
                                   : state->palette.border);
    DialogModern_DrawRoundedRect(
        hdc, &track,
        DialogModern_Scale(state->dpi, 3),
        trackColor, trackColor, 0);
    DialogModern_DrawRoundedRect(
        hdc, &thumb,
        DialogModern_Scale(state->dpi, 4),
        thumbColor, thumbColor, 0);
}

static void ModernEnsureControlVisible(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control || control->footer || control->kind == MODERN_CONTROL_CLOSE ||
        state->bodyScrollMax96 <= 0) {
        return;
    }

    int top96 = control->source96.top - state->contentMinY96;
    int bottom96 = control->source96.bottom - state->contentMinY96;
    int offset = state->bodyScrollOffset96;
    if (top96 < offset) {
        offset = top96;
    } else if (bottom96 > offset + state->bodyViewportHeight96) {
        offset = bottom96 - state->bodyViewportHeight96;
    }
    ModernSetBodyScrollOffset(state, offset);
}

static BOOL ModernAppendCloseButton(ModernDialogState* state) {
    const wchar_t* cancel = GetLocalizedString(NULL, L"Cancel");
    state->closeButton = CreateWindowExW(
        0, L"BUTTON", cancel ? cancel : L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, state->hwnd,
        (HMENU)(INT_PTR)MODERN_DIALOG_CLOSE_ID,
        GetModuleHandleW(NULL), NULL);
    if (!state->closeButton) {
        return FALSE;
    }
    if (!ModernEnsureControlCapacity(state, state->controlCount + 1)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        return FALSE;
    }

    ModernControl* control = &state->controls[state->controlCount];
    ZeroMemory(control, sizeof(*control));
    control->owner = state;
    control->hwnd = state->closeButton;
    control->id = MODERN_DIALOG_CLOSE_ID;
    control->kind = MODERN_CONTROL_CLOSE;
    control->sourceVisible = TRUE;
    ModernSetControlFont(state, control);
    if (!SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                           MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        ZeroMemory(control, sizeof(*control));
        return FALSE;
    }
    state->controlCount++;
    return TRUE;
}

static void ModernCommitClientSize(ModernDialogState* state,
                                   int width, int height) {
    state->clientWidth96 = ModernTo96(state->dpi, width);
    state->clientHeight96 = ModernTo96(state->dpi, height);
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 - state->bottomPadding96 -
                           state->footerHeight96;
    }
}

/* A dialog can be constrained by the work area or resized by accessibility
 * tooling. Keep the layout model tied to the actual client region so the
 * footer remains anchored and the body can scroll into the remaining space. */
static void ModernSyncClientSizeFromWindow(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    RECT client = {0};
    if (!GetClientRect(state->hwnd, &client)) return;

    int width96 = ModernTo96(state->dpi, client.right - client.left);
    int height96 = ModernTo96(state->dpi, client.bottom - client.top);
    if (width96 > 0) state->clientWidth96 = width96;
    if (height96 > 0) state->clientHeight96 = height96;
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 -
                           state->bottomPadding96 - state->footerHeight96;
    }
}

static void ModernCenterAndResize(ModernDialogState* state) {
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        ModernCommitClientSize(state, width, height);
        SetWindowPos(state->hwnd, NULL, 0, 0, width, height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
        return;
    }

    int maxWidth = info.rcWork.right - info.rcWork.left -
                   DialogModern_Scale(state->dpi, 24);
    int maxHeight = info.rcWork.bottom - info.rcWork.top -
                    DialogModern_Scale(state->dpi, 24);
    if (width > maxWidth) width = maxWidth;
    if (height > maxHeight) height = maxHeight;
    ModernCommitClientSize(state, width, height);
    int x = info.rcWork.left +
            ((info.rcWork.right - info.rcWork.left) - width) / 2;
    int y = info.rcWork.top +
            ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static BOOL ModernFinalize(ModernDialogState* state) {
    if (!state || state->finalized || state->finalizing) return state != NULL;
    state->finalizing = TRUE;
    state->dpi = DialogModern_GetDpi(state->hwnd);
    ModernRebuildResources(state);

    state->controlCount = 0;
    if (!EnumChildWindows(state->hwnd, ModernCaptureChild, (LPARAM)state)) {
        state->finalizing = FALSE;
        return FALSE;
    }
    ModernAnalyzeLayout(state);

    if (!ModernAppendCloseButton(state)) {
        state->finalizing = FALSE;
        return FALSE;
    }

    LONG_PTR style = GetWindowLongPtrW(state->hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(state->hwnd, GWL_EXSTYLE);
    style &= ~(WS_CAPTION | WS_DLGFRAME | WS_THICKFRAME | WS_BORDER |
               WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    style |= WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    exStyle |= WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT;
    SetWindowLongPtrW(state->hwnd, GWL_STYLE, style);
    SetWindowLongPtrW(state->hwnd, GWL_EXSTYLE, exStyle);

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernStyleControl(state, &state->controls[i]);
    }
    ModernSetDefaultButton(state);

    state->finalized = TRUE;
    state->finalizing = FALSE;
    ModernCenterAndResize(state);
    ModernLayoutControls(state);
    DialogModern_ApplyWindowShape(state->hwnd, state->dpi, 20);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return TRUE;
}

static ModernControl* ModernFindControl(ModernDialogState* state, HWND hwnd) {
    if (!state || !hwnd) return NULL;
    for (size_t i = 0; i < state->controlCount; i++) {
        if (state->controls[i].hwnd == hwnd) return &state->controls[i];
    }
    return NULL;
}

static void ModernDrawButton(ModernDialogState* state,
                             const DRAWITEMSTRUCT* item) {
    ModernControl* control = ModernFindControl(state, item->hwndItem);
    if (!control) return;

    RECT rect = item->rcItem;
    HBRUSH surface = CreateSolidBrush(state->palette.surface);
    FillRect(item->hDC, &rect, surface);
    DeleteObject(surface);
    BOOL disabled = (item->itemState & ODS_DISABLED) != 0;
    BOOL pressed = control->pressed || (item->itemState & ODS_SELECTED) != 0;
    BOOL focused = control->focused || (item->itemState & ODS_FOCUS) != 0;

    if (control->kind == MODERN_CONTROL_CLOSE) {
        DialogModern_DrawCloseButton(item->hDC, &rect, state->dpi,
                                     control->hovered, focused,
                                     state->palette.highContrast,
                                     state->palette.accent,
                                     state->palette.mutedText);
        return;
    }

    COLORREF fill = control->primary ? state->palette.accent
                                     : state->palette.field;
    COLORREF border = control->primary ? state->palette.accent
                                       : state->palette.border;
    COLORREF text = control->primary ?
        (state->palette.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                                       RGB(0xFF, 0xFF, 0xFF)) :
        state->palette.text;
    if (control->hovered) {
        fill = control->primary ? state->palette.accentHover
                                : state->palette.surface;
    }
    /* Secondary actions keep their current hover/rest appearance on press. */
    if (pressed && control->primary) fill = state->palette.accentHover;
    if (disabled) {
        fill = state->palette.field;
        text = state->palette.mutedText;
        border = state->palette.border;
    }

    DialogModern_DrawRoundedRect(item->hDC, &rect,
                                 DialogModern_Scale(state->dpi, 18),
                                 fill, border,
                                 state->palette.highContrast ? 1 : 0);
    wchar_t textBuffer[256] = {0};
    GetWindowTextW(control->hwnd, textBuffer, (int)_countof(textBuffer));
    if (focused) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -DialogModern_Scale(state->dpi, 4),
                    -DialogModern_Scale(state->dpi, 4));
        DialogModern_DrawRoundedRect(item->hDC, &focusRect,
                                     DialogModern_Scale(state->dpi, 14),
                                     fill, state->palette.accent, 1);
    }
    DialogModern_DrawText(item->hDC, state->buttonFont, text, &rect, textBuffer,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
}

static void ModernDrawComboItem(ModernDialogState* state,
                                const DRAWITEMSTRUCT* item) {
    ModernControl* control = ModernFindControl(state, item->hwndItem);
    if (!control || control->kind != MODERN_CONTROL_COMBO ||
        ModernIsDateTimeControl(control)) {
        return;
    }

    RECT rect = item->rcItem;
    FillRect(item->hDC, &rect, state->fieldBrush);
    if (item->itemID == (UINT)-1) return;

    BOOL selected = (item->itemState & ODS_SELECTED) != 0;
    BOOL disabled = (item->itemState & ODS_DISABLED) != 0;
    RECT selection = rect;
    InflateRect(&selection, -DialogModern_Scale(state->dpi, 4),
                -DialogModern_Scale(state->dpi, 2));
    if (selected) {
        DialogModern_DrawRoundedRect(item->hDC, &selection,
                                     DialogModern_Scale(state->dpi, 7),
                                     state->palette.accent,
                                     state->palette.accent, 0);
    }

    wchar_t text[512] = {0};
    SendMessageW(control->hwnd, CB_GETLBTEXT, item->itemID, (LPARAM)text);
    RECT textRect = rect;
    textRect.left += DialogModern_Scale(state->dpi, 12);
    textRect.right -= DialogModern_Scale(state->dpi, 10);
    COLORREF textColor = disabled ? state->palette.mutedText :
        (selected ? RGB(0xFF, 0xFF, 0xFF) : state->palette.text);
    DialogModern_DrawText(item->hDC, state->bodyFont, textColor,
                          &textRect, text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
}

static void ModernDrawDialog(ModernDialogState* state, HDC hdc) {
    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    FillRect(hdc, &client, state->backgroundBrush);

    RECT surface = {DialogModern_Scale(state->dpi, 8),
                    DialogModern_Scale(state->dpi, 8),
                    client.right - DialogModern_Scale(state->dpi, 8),
                    client.bottom - DialogModern_Scale(state->dpi, 8)};
    COLORREF shadow = state->palette.darkMode ? RGB(0x0D, 0x0E, 0x11)
                                              : RGB(0xD6, 0xDB, 0xE5);
    RECT shadowRect = surface;
    OffsetRect(&shadowRect, DialogModern_Scale(state->dpi, 1),
               DialogModern_Scale(state->dpi, 3));
    DialogModern_DrawRoundedRect(hdc, &shadowRect,
                                 DialogModern_Scale(state->dpi, 42),
                                 shadow, shadow, 0);
    DialogModern_DrawRoundedRect(hdc, &surface,
                                 DialogModern_Scale(state->dpi, 42),
                                 state->palette.surface,
                                 state->palette.highContrast ?
                                 state->palette.border : state->palette.surface,
                                 state->palette.highContrast ? 1 : 0);

    int side = DialogModern_Scale(state->dpi, state->sidePadding96);
    COLORREF accentColor = state->palette.accent;
    if (state->dialogType == DIALOG_INSTANCE_ERROR ||
        state->dialogType == DIALOG_INSTANCE_MESSAGE_ERROR ||
        state->dialogType == DIALOG_INSTANCE_UPDATE_ERROR) {
        accentColor = state->palette.danger;
    } else if (state->dialogType == DIALOG_INSTANCE_MESSAGE_WARNING) {
        accentColor = state->palette.warning;
    }
    wchar_t title[256] = {0};
    GetWindowTextW(state->hwnd, title, (int)_countof(title));
    RECT titleRect = {surface.left + side,
                      surface.top + DialogModern_Scale(state->dpi, 12),
                      client.right - side - DialogModern_Scale(state->dpi, 52),
                      DialogModern_Scale(state->dpi,
                                         state->headerHeight96 - 18)};
    SIZE titleSize = {0};
    HGDIOBJ oldTitleFont = state->titleFont
        ? SelectObject(hdc, state->titleFont) : NULL;
    GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &titleSize);
    if (oldTitleFont) SelectObject(hdc, oldTitleFont);
    RECT signatureRect = titleRect;
    signatureRect.bottom = DialogModern_Scale(
        state->dpi, state->headerHeight96 - 17);
    DialogModern_DrawTitleSignature(
        hdc, &signatureRect, state->dpi, titleSize.cx, accentColor,
        state->palette.surface, state->palette.darkMode,
        state->palette.highContrast);
    DialogModern_DrawText(hdc, state->titleFont, state->palette.text,
                          &titleRect, title,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
    ModernDrawBodyScrollbar(state, hdc);
}

static void ModernPaintBuffered(ModernDialogState* state, HDC target) {
    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    if (buffer && bitmap) {
        ModernDrawDialog(state, buffer);
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    } else {
        ModernDrawDialog(state, target);
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
    }
}

static void ModernDrawFieldOutline(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;
    RECT rect = {0};
    GetClientRect(control->hwnd, &rect);
    InflateRect(&rect, -1, -1);
    COLORREF border = control->focused ? state->palette.accent
                                      : state->palette.border;
    HPEN pen = CreatePen(PS_SOLID,
                         control->focused ? DialogModern_Scale(state->dpi, 2) : 1,
                         border);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom,
              DialogModern_Scale(state->dpi, 18),
              DialogModern_Scale(state->dpi, 18));
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
    ReleaseDC(control->hwnd, hdc);
}

static void ModernPaintChoiceControl(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state) return;
    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->surfaceBrush);

    wchar_t text[512] = {0};
    GetWindowTextW(control->hwnd, text, (int)_countof(text));
    BOOL enabled = IsWindowEnabled(control->hwnd);
    COLORREF textColor = enabled ? state->palette.text : state->palette.mutedText;

    if (control->kind == MODERN_CONTROL_GROUP) {
        int labelHeight = DialogModern_Scale(state->dpi, 18);
        RECT borderRect = client;
        borderRect.top += labelHeight / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, state->palette.border);
        HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, borderRect.left, borderRect.top,
                  borderRect.right, borderRect.bottom,
                  DialogModern_Scale(state->dpi, 16),
                  DialogModern_Scale(state->dpi, 16));
        SelectObject(hdc, oldBrush);
        if (oldPen) SelectObject(hdc, oldPen);
        if (pen) DeleteObject(pen);
        RECT label = {DialogModern_Scale(state->dpi, 12), 0,
                      client.right - DialogModern_Scale(state->dpi, 12),
                      labelHeight};
        if (text[0]) {
            SIZE textSize = {0};
            ModernMeasureText(control->hwnd, state->labelFont, &textSize);
            RECT labelBackdrop = {
                label.left - DialogModern_Scale(state->dpi, 4),
                0,
                label.left + textSize.cx + DialogModern_Scale(state->dpi, 4),
                labelHeight
            };
            if (labelBackdrop.right > label.right) {
                labelBackdrop.right = label.right;
            }
            FillRect(hdc, &labelBackdrop, state->surfaceBrush);
        }
        DialogModern_DrawText(hdc, state->labelFont, state->palette.mutedText,
                              &label, text,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    } else {
        int glyphSize = DialogModern_Scale(state->dpi, 16);
        int glyphY = (client.bottom - glyphSize) / 2;
        RECT glyph = {1, glyphY, 1 + glyphSize, glyphY + glyphSize};
        LRESULT checked = SendMessageW(control->hwnd, BM_GETCHECK, 0, 0);
        BOOL selected = checked == BST_CHECKED || checked == BST_INDETERMINATE;
        COLORREF selectionMark = state->palette.highContrast
            ? GetSysColor(COLOR_HIGHLIGHTTEXT)
            : RGB(0xFF, 0xFF, 0xFF);
        if (control->kind == MODERN_CONTROL_RADIO) {
            HBRUSH brush = CreateSolidBrush(selected ? state->palette.accent :
                                                       state->palette.surface);
            HPEN pen = CreatePen(PS_SOLID, 1,
                                 selected ? state->palette.accent :
                                            state->palette.border);
            HGDIOBJ oldBrush = brush ? SelectObject(hdc, brush) : NULL;
            HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
            Ellipse(hdc, glyph.left, glyph.top, glyph.right, glyph.bottom);
            if (selected) {
                RECT dot = glyph;
                InflateRect(&dot, -DialogModern_Scale(state->dpi, 5),
                            -DialogModern_Scale(state->dpi, 5));
                HBRUSH dotBrush = CreateSolidBrush(selectionMark);
                HGDIOBJ prior = SelectObject(hdc, dotBrush);
                Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
                SelectObject(hdc, prior);
                DeleteObject(dotBrush);
            }
            if (oldPen) SelectObject(hdc, oldPen);
            if (oldBrush) SelectObject(hdc, oldBrush);
            if (pen) DeleteObject(pen);
            if (brush) DeleteObject(brush);
        } else {
            DialogModern_DrawRoundedRect(hdc, &glyph,
                                         DialogModern_Scale(state->dpi, 7),
                                         selected ? state->palette.accent :
                                                    state->palette.surface,
                                         selected ? state->palette.accent :
                                                    state->palette.border, 1);
            if (selected) {
                HPEN checkPen = CreatePen(PS_SOLID, 2, selectionMark);
                HGDIOBJ oldPen = checkPen ? SelectObject(hdc, checkPen) : NULL;
                int midX = (glyph.left + glyph.right) / 2;
                int midY = (glyph.top + glyph.bottom) / 2;
                MoveToEx(hdc, glyph.left + glyphSize / 4, midY, NULL);
                LineTo(hdc, midX - glyphSize / 10, glyph.bottom - glyphSize / 4);
                LineTo(hdc, glyph.right - glyphSize / 5,
                       glyph.top + glyphSize / 4);
                if (oldPen) SelectObject(hdc, oldPen);
                if (checkPen) DeleteObject(checkPen);
            }
        }

        RECT textRect = {glyph.right + DialogModern_Scale(state->dpi, 8), 0,
                         client.right, client.bottom};
        DialogModern_DrawText(hdc, state->bodyFont, textColor, &textRect, text,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    }

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernPaintSlider(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->surfaceBrush);

    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    BOOL vertical = (style & TBS_VERT) != 0;
    BOOL reversed = (style & TBS_REVERSED) != 0;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    int position = (int)SendMessageW(control->hwnd, TBM_GETPOS, 0, 0);
    if (maximum <= minimum) maximum = minimum + 1;
    if (position < minimum) position = minimum;
    if (position > maximum) position = maximum;

    int thumbRadius = DialogModern_Scale(state->dpi,
                                         control->pressed ? 7 : 6);
    int channelThickness = DialogModern_Scale(state->dpi, 4);
    int edge = thumbRadius + DialogModern_Scale(state->dpi, 2);
    int usable = vertical ? (client.bottom - client.top - edge * 2) :
                            (client.right - client.left - edge * 2);
    if (usable < 1) usable = 1;
    int offset = MulDiv(position - minimum, usable, maximum - minimum);
    if (reversed) offset = usable - offset;

    POINT thumb = {0};
    RECT channel = client;
    RECT completed = client;
    if (vertical) {
        thumb.x = (client.left + client.right) / 2;
        thumb.y = client.top + edge + offset;
        channel.left = thumb.x - channelThickness / 2;
        channel.right = channel.left + channelThickness;
        channel.top = client.top + edge;
        channel.bottom = client.bottom - edge;
        completed = channel;
        completed.bottom = thumb.y;
    } else {
        thumb.x = client.left + edge + offset;
        thumb.y = (client.top + client.bottom) / 2;
        channel.top = thumb.y - channelThickness / 2;
        channel.bottom = channel.top + channelThickness;
        channel.left = client.left + edge;
        channel.right = client.right - edge;
        completed = channel;
        completed.right = thumb.x;
    }

    DialogModern_DrawRoundedRect(hdc, &channel, channelThickness,
                                 state->palette.border,
                                 state->palette.border, 0);
    DialogModern_DrawRoundedRect(hdc, &completed, channelThickness,
                                 state->palette.accent,
                                 state->palette.accent, 0);

    BOOL enabled = IsWindowEnabled(control->hwnd);
    COLORREF thumbColor = enabled ? state->palette.accent :
                                   state->palette.mutedText;
    COLORREF outline = control->focused || control->hovered ?
                       state->palette.accentHover : state->palette.surface;
    HBRUSH thumbBrush = CreateSolidBrush(thumbColor);
    HPEN thumbPen = CreatePen(PS_SOLID,
                              control->focused ?
                                  DialogModern_Scale(state->dpi, 2) : 1,
                              outline);
    HGDIOBJ oldBrush = thumbBrush ? SelectObject(hdc, thumbBrush) : NULL;
    HGDIOBJ oldPen = thumbPen ? SelectObject(hdc, thumbPen) : NULL;
    Ellipse(hdc, thumb.x - thumbRadius, thumb.y - thumbRadius,
            thumb.x + thumbRadius + 1, thumb.y + thumbRadius + 1);
    if (oldPen) SelectObject(hdc, oldPen);
    if (oldBrush) SelectObject(hdc, oldBrush);
    if (thumbPen) DeleteObject(thumbPen);
    if (thumbBrush) DeleteObject(thumbBrush);

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static HWND ModernFindDateTimeSpinner(HWND hwnd) {
    HWND child = hwnd ? GetWindow(hwnd, GW_CHILD) : NULL;
    while (child) {
        if (ModernWindowHasClass(child, L"msctls_updown32")) return child;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return NULL;
}

static void ModernPaintCombo(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd || ModernIsDateTimeControl(control)) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->fieldBrush);

    wchar_t text[512] = {0};
    int selected = (int)SendMessageW(control->hwnd, CB_GETCURSEL, 0, 0);
    if (selected != CB_ERR) {
        SendMessageW(control->hwnd, CB_GETLBTEXT, selected, (LPARAM)text);
    } else {
        GetWindowTextW(control->hwnd, text, (int)_countof(text));
    }

    int arrowWidth = DialogModern_Scale(state->dpi, 34);
    RECT textRect = client;
    textRect.left += DialogModern_Scale(state->dpi, 10);
    textRect.right -= arrowWidth;
    COLORREF textColor = IsWindowEnabled(control->hwnd)
        ? state->palette.text : state->palette.mutedText;
    DialogModern_DrawText(hdc, state->editFont, textColor, &textRect, text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);

    int centerX = client.right - arrowWidth / 2;
    int centerY = (client.top + client.bottom) / 2;
    int arm = max(3, DialogModern_Scale(state->dpi, 4));
    HPEN pen = CreatePen(PS_SOLID,
                         max(1, DialogModern_Scale(state->dpi, 1)),
                         control->hovered ? state->palette.accent : textColor);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    MoveToEx(hdc, centerX - arm, centerY - arm / 2, NULL);
    LineTo(hdc, centerX, centerY + arm / 2);
    LineTo(hdc, centerX + arm, centerY - arm / 2);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernPaintDateTime(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->fieldBrush);

    SYSTEMTIME systemTime = {0};
    wchar_t value[64] = {0};
    if (SendMessageW(control->hwnd, DTM_GETSYSTEMTIME, 0,
                     (LPARAM)&systemTime) != GDT_VALID) {
        GetLocalTime(&systemTime);
    }
    if (!GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_FORCE24HOURFORMAT,
                         &systemTime, NULL, value, (int)_countof(value))) {
        StringCchPrintfW(value, _countof(value), L"%u:%02u:%02u",
                         systemTime.wHour, systemTime.wMinute,
                         systemTime.wSecond);
    }

    RECT textRect = client;
    textRect.left += DialogModern_Scale(state->dpi, 10);
    HWND spinner = ModernFindDateTimeSpinner(control->hwnd);
    if (spinner) {
        RECT spinnerRect = {0};
        if (GetWindowRect(spinner, &spinnerRect)) {
            MapWindowPoints(NULL, control->hwnd, (POINT*)&spinnerRect, 2);
            textRect.right = spinnerRect.left -
                             DialogModern_Scale(state->dpi, 4);
        }
    } else {
        textRect.right -= DialogModern_Scale(state->dpi, 10);
    }
    COLORREF textColor = IsWindowEnabled(control->hwnd) ?
                         state->palette.text : state->palette.mutedText;
    DialogModern_DrawText(hdc, state->editFont, textColor, &textRect, value,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernPaintDateTimeSpinner(HWND hwnd, ModernControl* control,
                                       HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !hwnd) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(hwnd, &paint);
    if (!hdc) return;
    RECT client = {0};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, state->fieldBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, state->palette.border);
    HGDIOBJ oldPen = borderPen ? SelectObject(hdc, borderPen) : NULL;
    int middle = (client.top + client.bottom) / 2;
    MoveToEx(hdc, client.left, client.top, NULL);
    LineTo(hdc, client.left, client.bottom);
    MoveToEx(hdc, client.left, middle, NULL);
    LineTo(hdc, client.right, middle);
    if (oldPen) SelectObject(hdc, oldPen);
    if (borderPen) DeleteObject(borderPen);

    COLORREF arrowColor = IsWindowEnabled(hwnd) ? state->palette.text :
                                                 state->palette.mutedText;
    HPEN arrowPen = CreatePen(PS_SOLID,
                              max(1, DialogModern_Scale(state->dpi, 1)),
                              arrowColor);
    oldPen = arrowPen ? SelectObject(hdc, arrowPen) : NULL;
    int centerX = (client.left + client.right) / 2;
    int arm = max(2, DialogModern_Scale(state->dpi, 3));
    int upperY = (client.top + middle) / 2;
    int lowerY = (middle + client.bottom) / 2;
    MoveToEx(hdc, centerX - arm, upperY + arm / 2, NULL);
    LineTo(hdc, centerX, upperY - arm / 2);
    LineTo(hdc, centerX + arm, upperY + arm / 2);
    MoveToEx(hdc, centerX - arm, lowerY - arm / 2, NULL);
    LineTo(hdc, centerX, lowerY + arm / 2);
    LineTo(hdc, centerX + arm, lowerY - arm / 2);
    if (oldPen) SelectObject(hdc, oldPen);
    if (arrowPen) DeleteObject(arrowPen);

    if (!suppliedDc) EndPaint(hwnd, &paint);
}

static void ModernTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

static BOOL ModernControlOwnsVerticalScroll(const ModernControl* control) {
    if (!control || !control->hwnd) return FALSE;
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    if ((style & WS_VSCROLL) != 0) return TRUE;
    if (control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO) {
        return TRUE;
    }
    if (control->kind == MODERN_CONTROL_FIELD &&
        (style & ES_MULTILINE) != 0) {
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK ModernDateTimeChildSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernControl* control = (ModernControl*)refData;
    switch (msg) {
        case WM_PAINT:
            ModernPaintDateTimeSpinner(hwnd, control, NULL);
            return 0;
        case WM_PRINTCLIENT:
            ModernPaintDateTimeSpinner(hwnd, control, (HDC)wParam);
            return 0;
        case WM_ERASEBKGND:
            return 1;
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
    int radius = DialogModern_Scale(control->owner->dpi, 9);
    HRGN region = CreateRoundRectRgn(client.left, client.top,
                                     client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
}

static LRESULT CALLBACK ModernComboListSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernControl* control = (ModernControl*)refData;
    ModernDialogState* state = control ? control->owner : NULL;
    switch (msg) {
        case WM_WINDOWPOSCHANGED:
        case WM_SIZE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            ModernApplyComboListRegion(hwnd, control);
            return result;
        }
        case WM_ERASEBKGND:
            if (state) {
                RECT client = {0};
                GetClientRect(hwnd, &client);
                FillRect((HDC)wParam, &client, state->fieldBrush);
                return 1;
            }
            break;
        case WM_NCPAINT:
            return 0;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernComboListSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static int ModernSliderPositionFromPoint(ModernControl* control, int x, int y) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return 0;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    BOOL vertical = (style & TBS_VERT) != 0;
    BOOL reversed = (style & TBS_REVERSED) != 0;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    if (maximum <= minimum) return minimum;
    int thumbRadius = DialogModern_Scale(state->dpi, 7);
    int edge = thumbRadius + DialogModern_Scale(state->dpi, 2);
    int start = vertical ? client.top + edge : client.left + edge;
    int finish = vertical ? client.bottom - edge : client.right - edge;
    int coordinate = vertical ? y : x;
    if (coordinate < start) coordinate = start;
    if (coordinate > finish) coordinate = finish;
    int position = minimum + MulDiv(coordinate - start,
                                    maximum - minimum,
                                    max(1, finish - start));
    if (reversed) position = maximum - (position - minimum);
    return position;
}

static void ModernSetSliderFromPoint(ModernControl* control, int x, int y,
                                     UINT notification) {
    if (!control || !control->hwnd) return;
    int position = ModernSliderPositionFromPoint(control, x, y);
    SendMessageW(control->hwnd, TBM_SETPOS, TRUE, position);
    HWND parent = GetParent(control->hwnd);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    UINT message = (style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    SendMessageW(parent, message, MAKEWPARAM(notification, position),
                 (LPARAM)control->hwnd);
}

static LRESULT CALLBACK ModernControlSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData) {
    (void)subclassId;
    ModernControl* control = (ModernControl*)refData;
    ModernDialogState* state = control ? control->owner : NULL;

    switch (msg) {
        case WM_MOUSEMOVE:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed && GetCapture() == hwnd) {
                ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                         GET_Y_LPARAM(lParam), TB_THUMBTRACK);
                return 0;
            }
            if (control && !control->hovered) {
                control->hovered = TRUE;
                ModernTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            if (control && control->hovered) {
                control->hovered = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_LBUTTONDOWN:
            if (control) {
                control->pressed = TRUE;
                if (control->kind == MODERN_CONTROL_SLIDER) {
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                    ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                             GET_Y_LPARAM(lParam),
                                             TB_THUMBTRACK);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed) {
                ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                         GET_Y_LPARAM(lParam),
                                         TB_THUMBPOSITION);
                control->pressed = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
                LONG_PTR sliderStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
                UINT scrollMessage = (sliderStyle & TBS_VERT)
                    ? WM_VSCROLL : WM_HSCROLL;
                SendMessageW(GetParent(hwnd), scrollMessage,
                             MAKEWPARAM(TB_ENDTRACK, 0), (LPARAM)hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            /* fall through */
        case WM_CAPTURECHANGED:
            if (control) {
                control->pressed = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETFOCUS:
            if (control) {
                ModernEnsureControlVisible(control);
                control->focused = TRUE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_KILLFOCUS:
            if (control) {
                control->focused = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_PAINT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, NULL);
                return 0;
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, NULL);
                ModernDrawFieldOutline(control);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, NULL);
                ModernDrawFieldOutline(control);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, NULL);
                return 0;
            }
            if (control && (control->kind == MODERN_CONTROL_FIELD ||
                            control->kind == MODERN_CONTROL_LIST ||
                            control->kind == MODERN_CONTROL_COMBO)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernDrawFieldOutline(control);
                return result;
            }
            break;
        case WM_PRINTCLIENT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, (HDC)wParam);
                return 0;
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, (HDC)wParam);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, (HDC)wParam);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, (HDC)wParam);
                return 0;
            }
            break;
        case WM_SIZE:
            ModernApplyFieldRegion(control);
            break;
        case WM_MOUSEWHEEL:
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int step = DialogModern_Scale(state->dpi, 48);
                int amount = MulDiv(delta, step, WHEEL_DELTA);
                if (amount == 0 && delta != 0) {
                    amount = delta > 0 ? step : -step;
                }
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 - amount);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && state && state->hwnd) {
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    SendMessageW(hwnd, CB_GETDROPPEDSTATE, 0, 0)) {
                    break;
                }
                SendMessageW(state->hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control) &&
                (wParam == VK_PRIOR || wParam == VK_NEXT)) {
                int page = state->bodyViewportHeight96;
                int direction = wParam == VK_PRIOR ? -1 : 1;
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 + direction * page);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (control && (control->kind == MODERN_CONTROL_CLOSE ||
                            control->kind == MODERN_CONTROL_PUSH)) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernControlSubclassProc,
                                 MODERN_CONTROL_SUBCLASS_ID);
            break;
    }
    (void)state;
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void ModernHandleDpiChanged(ModernDialogState* state, WPARAM wParam,
                                   LPARAM lParam) {
    state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96u;
    ModernRebuildResources(state);
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernSetControlFont(state, &state->controls[i]);
    }
    RECT* suggested = (RECT*)lParam;
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = suggested ?
        MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST) :
        MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        int maxWidth = info.rcWork.right - info.rcWork.left -
                       DialogModern_Scale(state->dpi, 24);
        int maxHeight = info.rcWork.bottom - info.rcWork.top -
                        DialogModern_Scale(state->dpi, 24);
        if (width > maxWidth) width = maxWidth;
        if (height > maxHeight) height = maxHeight;
    }
    ModernCommitClientSize(state, width, height);
    int x = suggested ? suggested->left : 0;
    int y = suggested ? suggested->top : 0;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE |
                 (suggested ? 0 : SWP_NOMOVE));
    ModernLayoutControls(state);
    DialogModern_ApplyWindowShape(state->hwnd, state->dpi, 20);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void ModernFreeState(ModernDialogState* state) {
    if (!state) return;
    ModernDeleteFonts(state);
    ModernDeleteBrushes(state);
    free(state->controls);
    free(state);
}

static LRESULT CALLBACK ModernDialogSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
                                                 DWORD_PTR refData) {
    (void)subclassId;
    ModernDialogState* state = (ModernDialogState*)refData;

    switch (msg) {
        case WM_SHOWWINDOW:
            if (wParam && state && !state->finalized) ModernFinalize(state);
            break;
        case MODERN_DIALOG_FINALIZE_MESSAGE:
            ModernFinalize(state);
            return 0;
        case WM_PAINT:
            if (state && state->finalized) {
                PAINTSTRUCT paint = {0};
                HDC hdc = BeginPaint(hwnd, &paint);
                ModernPaintBuffered(state, hdc);
                EndPaint(hwnd, &paint);
                return 0;
            }
            break;
        case WM_PRINTCLIENT:
            if (state && state->finalized) {
                ModernDrawDialog(state, (HDC)wParam);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            if (state && state->finalized) return 1;
            break;
        case WM_DRAWITEM:
            if (state && state->finalized && lParam) {
                const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
                const ModernControl* control =
                    ModernFindControl(state, item->hwndItem);
                if (control && (control->kind == MODERN_CONTROL_PUSH ||
                                control->kind == MODERN_CONTROL_CLOSE)) {
                    ModernDrawButton(state, item);
                    return TRUE;
                }
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    !ModernIsDateTimeControl(control)) {
                    ModernDrawComboItem(state, item);
                    return TRUE;
                }
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == MODERN_DIALOG_CLOSE_ID) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int step = DialogModern_Scale(state->dpi, 48);
                int amount = MulDiv(delta, step, WHEEL_DELTA);
                if (amount == 0 && delta != 0) {
                    amount = delta > 0 ? step : -step;
                }
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 - amount);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb)) {
                    if (PtInRect(&thumb, point)) {
                        state->scrollBarDragging = TRUE;
                        state->scrollDragStartY = point.y;
                        state->scrollDragStartOffset96 =
                            state->bodyScrollOffset96;
                        SetCapture(hwnd);
                        return 0;
                    }
                    if (PtInRect(&track, point)) {
                        int direction = point.y < thumb.top ? -1 : 1;
                        ModernSetBodyScrollOffset(
                            state,
                            state->bodyScrollOffset96 +
                                direction * state->bodyViewportHeight96);
                        return 0;
                    }
                }
            }
            break;
        case WM_LBUTTONUP:
            if (state && state->scrollBarDragging) {
                state->scrollBarDragging = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb)) {
                    if (state->scrollBarDragging) {
                        int travel = (track.bottom - track.top) -
                                     (thumb.bottom - thumb.top);
                        if (travel > 0) {
                            int offset = state->scrollDragStartOffset96 +
                                MulDiv(point.y - state->scrollDragStartY,
                                       state->bodyScrollMax96, travel);
                            ModernSetBodyScrollOffset(state, offset);
                        }
                        return 0;
                    }
                    BOOL hovered = PtInRect(&track, point);
                    if (hovered != state->scrollBarHovered) {
                        state->scrollBarHovered = hovered;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    ModernTrackMouse(hwnd);
                }
            }
            break;
        case WM_MOUSELEAVE:
            if (state && state->scrollBarHovered &&
                !state->scrollBarDragging) {
                state->scrollBarHovered = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_CAPTURECHANGED:
            if (state && state->scrollBarDragging &&
                (HWND)lParam != hwnd) {
                state->scrollBarDragging = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETCURSOR:
            if (state && state->bodyScrollMax96 > 0) {
                POINT point = {0};
                GetCursorPos(&point);
                ScreenToClient(hwnd, &point);
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb) &&
                    PtInRect(&track, point)) {
                    SetCursor(LoadCursorW(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_CTLCOLORDLG:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.surface);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_CTLCOLORSTATIC:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.field);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->fieldBrush;
            }
            break;
        case WM_CTLCOLORBTN:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_NCHITTEST:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                RECT closeRect = {0};
                if (state->closeButton) GetWindowRect(state->closeButton, &closeRect);
                if (state->closeButton) MapWindowPoints(NULL, hwnd,
                                                       (POINT*)&closeRect, 2);
                if (PtInRect(&closeRect, point)) return HTCLIENT;
                if (point.y < DialogModern_Scale(state->dpi,
                                                 state->headerHeight96)) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            }
            break;
        case WM_DPICHANGED:
            if (state && state->finalized) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernHandleDpiChanged(state, wParam, lParam);
                return result;
            }
            break;
        case WM_SIZE:
            if (state && state->finalized && !state->finalizing) {
                ModernSyncClientSizeFromWindow(state);
                ModernLayoutControls(state);
                DialogModern_ApplyWindowShape(hwnd, state->dpi, 20);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (state && state->finalized) {
                DialogModern_Refresh(hwnd);
                return 0;
            }
            break;
        case WM_NCDESTROY: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            RemovePropW(hwnd, MODERN_DIALOG_STATE_PROP);
            RemoveWindowSubclass(hwnd, ModernDialogSubclassProc,
                                 MODERN_DIALOG_SUBCLASS_ID);
            ModernFreeState(state);
            return result;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BOOL DialogModern_Attach(HWND hwndDlg, int dialogType) {
    if (!hwndDlg || !IsWindow(hwndDlg) || DialogModern_IsAttached(hwndDlg)) {
        return hwndDlg && DialogModern_IsAttached(hwndDlg);
    }

    wchar_t className[64] = {0};
    if (!GetClassNameW(hwndDlg, className, _countof(className)) ||
        wcscmp(className, L"#32770") != 0) {
        return FALSE;
    }

    ModernDialogState* state =
        (ModernDialogState*)calloc(1, sizeof(*state));
    if (!state) return FALSE;
    state->hwnd = hwndDlg;
    state->dialogType = dialogType;
    state->dpi = DialogModern_GetDpi(hwndDlg);
    state->attached = TRUE;
    if (!SetPropW(hwndDlg, MODERN_DIALOG_STATE_PROP, (HANDLE)state) ||
        !SetWindowSubclass(hwndDlg, ModernDialogSubclassProc,
                           MODERN_DIALOG_SUBCLASS_ID, (DWORD_PTR)state)) {
        RemovePropW(hwndDlg, MODERN_DIALOG_STATE_PROP);
        free(state);
        return FALSE;
    }
    PostMessageW(hwndDlg, MODERN_DIALOG_FINALIZE_MESSAGE, 0, 0);
    return TRUE;
}

void DialogModern_Refresh(HWND hwndDlg) {
    ModernDialogState* state = ModernGetState(hwndDlg);
    if (!state) return;
    if (state->refreshing) {
        state->refreshPending = TRUE;
        return;
    }

    state->refreshing = TRUE;
    for (;;) {
        state->refreshPending = FALSE;
        ModernRebuildResources(state);
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernSetControlFont(state, &state->controls[i]);
            ModernApplyFieldRegion(&state->controls[i]);
        }
        RedrawWindow(hwndDlg, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        if (ModernGetState(hwndDlg) != state) return;
        if (!state->refreshPending) break;
    }
    state->refreshing = FALSE;
}
