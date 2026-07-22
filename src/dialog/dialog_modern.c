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
#define MODERN_DIALOG_CLEAR_FOCUS_MESSAGE (WM_APP + 491)
#define MODERN_COMBO_VISIBLE_ITEMS 7
#define MODERN_TITLE_HOVER_COLOR RGB(0xF7, 0x7D, 0xAA)
#define MODERN_DATETIME_REPEAT_TIMER 0xD145
#define MODERN_DATETIME_INPUT_TIMEOUT_MS 1200u

typedef enum {
    MODERN_DATETIME_HIT_NONE = -1,
    MODERN_DATETIME_HOUR = 0,
    MODERN_DATETIME_MINUTE,
    MODERN_DATETIME_SECOND,
    MODERN_DATETIME_STEP_UP,
    MODERN_DATETIME_STEP_DOWN
} ModernDateTimeHit;

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

typedef enum {
    MODERN_BODY_REGION_UNKNOWN = 0,
    MODERN_BODY_REGION_HIDDEN,
    MODERN_BODY_REGION_FULL_PLAIN,
    MODERN_BODY_REGION_FULL_ROUNDED,
    MODERN_BODY_REGION_PARTIAL_PLAIN,
    MODERN_BODY_REGION_PARTIAL_ROUNDED,
    MODERN_BODY_REGION_CROPPED_SCROLL
} ModernBodyRegionMode;

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
    int comboHotItem;
    BOOL comboScrollHovered;
    BOOL comboScrollDragging;
    int comboScrollDragStartY;
    int comboScrollDragStartTopIndex;
    int comboWheelDelta;
    int comboListRegionWidth;
    int comboListRegionHeight;
    UINT comboListRegionDpi;
    int sliderWheelDelta;
    int dateTimeSelectedPart;
    int dateTimeHotPart;
    int dateTimePressedPart;
    int dateTimeWheelDelta;
    int dateTimeDigitValue;
    int dateTimeDigitCount;
    DWORD dateTimeDigitTick;
    BOOL dateTimeRepeatStarted;
    ModernBodyRegionMode bodyRegionMode;
    int bodyRegionWidth;
    int bodyRegionHeight;
    int bodyRegionTop;
    int bodyRegionBottom;
    UINT bodyRegionDpi;
    int bodyLayoutX;
    int bodyLayoutY;
    int bodyLayoutWidth;
    int bodyLayoutHeight;
    int bodyLayoutY96;
    BOOL bodyLayoutPending;
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
    RECT titleFrame;
    BOOL hasFooter;
    BOOL titleHovered;
    BOOL scrollBarHovered;
    BOOL scrollBarDragging;
    BOOL attached;
    BOOL finalized;
    BOOL finalizing;
    BOOL refreshing;
    BOOL refreshPending;
};

static void ModernDrawComboItem(ModernDialogState* state,
                                const DRAWITEMSTRUCT* item);

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
static void ModernLayoutBodyControls(ModernDialogState* state,
                                     BOOL suppressRedraw);
static void ModernLayoutControls(ModernDialogState* state);
static void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96);
static void ModernSyncClientSizeFromWindow(ModernDialogState* state);
static BOOL ModernControlOwnsVerticalScroll(const ModernControl* control);
static void ModernAttachComboList(ModernControl* control);
static void ModernApplyComboListRegion(HWND hwnd, ModernControl* control);
static int ModernTo96(UINT dpi, int value);

static COLORREF ModernBlendColor(COLORREF from, COLORREF to, int toPercent) {
    if (toPercent < 0) toPercent = 0;
    if (toPercent > 100) toPercent = 100;
    int fromPercent = 100 - toPercent;
    return RGB(
        (GetRValue(from) * fromPercent + GetRValue(to) * toPercent) / 100,
        (GetGValue(from) * fromPercent + GetGValue(to) * toPercent) / 100,
        (GetBValue(from) * fromPercent + GetBValue(to) * toPercent) / 100);
}

static BOOL ModernUpdateTitleHover(ModernDialogState* state, POINT point) {
    if (!state || !state->finalized) return FALSE;
    BOOL hovered = PtInRect(&state->titleFrame, point);
    if (hovered == state->titleHovered) return FALSE;
    state->titleHovered = hovered;
    InvalidateRect(state->hwnd, &state->titleFrame, FALSE);
    return TRUE;
}

static void ModernRefreshTitleHoverFromCursor(ModernDialogState* state) {
    POINT point = {0};
    if (!state || !GetCursorPos(&point)) return;
    ScreenToClient(state->hwnd, &point);
    ModernUpdateTitleHover(state, point);
}

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
        ShowWindow(child, SW_HIDE);
    }
    return TRUE;
}

static void ModernHideDateTimeSpinner(ModernControl* control) {
    if (!ModernIsDateTimeControl(control)) return;
    EnumChildWindows(control->hwnd, ModernAttachDateTimeChild,
                     (LPARAM)control);
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
    control->comboHotItem = -1;
    control->dateTimeHotPart = MODERN_DATETIME_HIT_NONE;
    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
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

static BOOL ModernApplyFieldRegionRaw(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return FALSE;
    if (control->kind != MODERN_CONTROL_FIELD &&
        control->kind != MODERN_CONTROL_LIST &&
        control->kind != MODERN_CONTROL_COMBO) return FALSE;
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

static void ModernApplyFieldRegion(ModernControl* control) {
    if (!control) return;
    control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
    ModernApplyFieldRegionRaw(control, TRUE);
}

/* Compact visual single-line edits use the multiline formatting rectangle so
 * Win32 can center text without replacing its native editing behavior. */
static BOOL ModernIsCompactEdit(const ModernControl* control) {
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

static void ModernApplyEditLayout(ModernControl* control) {
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

static wchar_t* ModernCreateSingleLineText(const wchar_t* source,
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

static BOOL ModernPasteCompactEdit(HWND hwnd) {
    if (!hwnd || !IsClipboardFormatAvailable(CF_UNICODETEXT) ||
        !OpenClipboard(hwnd)) {
        return FALSE;
    }

    BOOL handled = FALSE;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t* source = data ? (const wchar_t*)GlobalLock(data) : NULL;
    if (source) {
        SIZE_T capacity = GlobalSize(data) / sizeof(*source);
        size_t length = 0;
        while (length < capacity && source[length] != L'\0') length++;
        if (length < capacity) {
            BOOL changed = FALSE;
            wchar_t* text = ModernCreateSingleLineText(
                source, length, &changed);
            if (text && changed) {
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)text);
                handled = TRUE;
            }
            free(text);
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
    return handled;
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
        if (control->kind == MODERN_CONTROL_COMBO &&
            !ModernIsDateTimeControl(control)) {
            style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            SetWindowLongPtrW(control->hwnd, GWL_STYLE,
                              style | CBS_HASSTRINGS);
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, (WPARAM)-1,
                         DialogModern_Scale(state->dpi, 30));
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, 0,
                         DialogModern_Scale(state->dpi, 34));
            SendMessageW(control->hwnd, CB_SETMINVISIBLE,
                         MODERN_COMBO_VISIBLE_ITEMS, 0);
            ModernAttachComboList(control);
        }
        ModernApplyFieldRegion(control);
        if (control->kind == MODERN_CONTROL_FIELD) {
            ModernApplyEditLayout(control);
        }
    }

    if (ModernIsDateTimeControl(control)) {
        SendMessageW(control->hwnd, DTM_SETFORMATW, 0,
                     (LPARAM)L"HH':'mm':'ss");
        ModernHideDateTimeSpinner(control);
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

static BOOL ModernBodyRegionMatches(const ModernControl* control,
                                    ModernBodyRegionMode mode,
                                    int width, int height,
                                    int top, int bottom, UINT dpi) {
    return control && control->bodyRegionMode == mode &&
           control->bodyRegionWidth == width &&
           control->bodyRegionHeight == height &&
           control->bodyRegionTop == top &&
           control->bodyRegionBottom == bottom &&
           control->bodyRegionDpi == dpi;
}

static void ModernRememberBodyRegion(ModernControl* control,
                                     ModernBodyRegionMode mode,
                                     int width, int height,
                                     int top, int bottom, UINT dpi) {
    if (!control) return;
    control->bodyRegionMode = mode;
    control->bodyRegionWidth = width;
    control->bodyRegionHeight = height;
    control->bodyRegionTop = top;
    control->bodyRegionBottom = bottom;
    control->bodyRegionDpi = dpi;
}

static void ModernHideBodyControl(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode != MODERN_BODY_REGION_HIDDEN) {
        SetWindowRgn(control->hwnd, NULL, redraw);
        ShowWindow(control->hwnd, SW_HIDE);
    }
    ModernRememberBodyRegion(control, MODERN_BODY_REGION_HIDDEN,
                             0, 0, 0, 0, 0);
}

static void ModernShowBodyControl(ModernControl* control) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode == MODERN_BODY_REGION_HIDDEN ||
        control->bodyRegionMode == MODERN_BODY_REGION_UNKNOWN) {
        ShowWindow(control->hwnd, SW_SHOWNA);
    }
}

static void ModernApplyBodyControlRegion(
    ModernDialogState* state, ModernControl* control, int y96,
    BOOL suppressRedraw) {
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
        ModernHideBodyControl(control, !suppressRedraw);
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
        ModernHideBodyControl(control, !suppressRedraw);
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
            ModernHideBodyControl(control, !suppressRedraw);
            return;
        }

        RECT windowRect = {0};
        GetWindowRect(control->hwnd, &windowRect);
        MapWindowPoints(NULL, state->hwnd, (POINT*)&windowRect, 2);
        if (!ModernBodyRegionMatches(
                control, MODERN_BODY_REGION_CROPPED_SCROLL,
                windowRect.right - windowRect.left, clippedHeight,
                0, clippedHeight, state->dpi)) {
            SetWindowRgn(control->hwnd, NULL, !suppressRedraw);
        }
        ModernShowBodyControl(control);
        SetWindowPos(control->hwnd, NULL,
                     windowRect.left, clippedTop,
                     windowRect.right - windowRect.left, clippedHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE |
                         (suppressRedraw ? SWP_NOREDRAW | SWP_NOCOPYBITS : 0));
        ModernRememberBodyRegion(
            control, MODERN_BODY_REGION_CROPPED_SCROLL,
            windowRect.right - windowRect.left, clippedHeight,
            0, clippedHeight, state->dpi);
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
            ModernHideBodyControl(control, !suppressRedraw);
            return;
        }
    }

    if (!fullyInside && height <= viewportHeight &&
        control->kind != MODERN_CONTROL_GROUP) {
        ModernHideBodyControl(control, !suppressRedraw);
        return;
    }

    ModernShowBodyControl(control);
    if (visibleTop < 0) visibleTop = 0;
    if (visibleBottom > height) visibleBottom = height;

    BOOL fullyVisible = visibleTop == 0 && visibleBottom == height;
    BOOL rounded = control->kind == MODERN_CONTROL_FIELD ||
                   control->kind == MODERN_CONTROL_LIST ||
                   control->kind == MODERN_CONTROL_COMBO;
    if (fullyVisible) {
        ModernBodyRegionMode mode = rounded
            ? MODERN_BODY_REGION_FULL_ROUNDED
            : MODERN_BODY_REGION_FULL_PLAIN;
        if (ModernBodyRegionMatches(control, mode, width, height,
                                    0, height, state->dpi)) {
            return;
        }
        if (rounded) {
            if (!ModernApplyFieldRegionRaw(control, !suppressRedraw)) {
                control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
                return;
            }
        } else {
            SetWindowRgn(control->hwnd, NULL, !suppressRedraw);
        }
        ModernRememberBodyRegion(control, mode, width, height,
                                 0, height, state->dpi);
        return;
    }

    if (visibleBottom < visibleTop) visibleBottom = visibleTop;
    ModernBodyRegionMode mode = rounded
        ? MODERN_BODY_REGION_PARTIAL_ROUNDED
        : MODERN_BODY_REGION_PARTIAL_PLAIN;
    if (ModernBodyRegionMatches(control, mode, width, height,
                                visibleTop, visibleBottom, state->dpi)) {
        return;
    }
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
    if (!SetWindowRgn(control->hwnd, shape, !suppressRedraw)) {
        DeleteObject(shape);
        control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
        return;
    }
    ModernRememberBodyRegion(control, mode, width, height,
                             visibleTop, visibleBottom, state->dpi);
}

static void ModernLayoutBodyControls(ModernDialogState* state,
                                     BOOL suppressRedraw) {
    if (!state || !state->finalized) return;
    ModernUpdateBodyScrollMetrics(state);
    int extraWidth96 = state->clientWidth96 -
                       (state->contentWidth96 + state->sidePadding96 * 2);
    int contentOffsetX96 = state->sidePadding96 +
                           (extraWidth96 > 0 ? extraWidth96 / 2 : 0);

    size_t pendingCount = 0;
    int viewportTop = DialogModern_Scale(state->dpi,
                                         state->headerHeight96);
    int viewportBottom = DialogModern_Scale(
        state->dpi,
        state->headerHeight96 + state->bodyViewportHeight96);

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        control->bodyLayoutPending = FALSE;
        if (control->footer || control->kind == MODERN_CONTROL_CLOSE) continue;

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
        control->bodyLayoutX = DialogModern_Scale(state->dpi, x96);
        control->bodyLayoutY = DialogModern_Scale(state->dpi, y96);
        control->bodyLayoutWidth = DialogModern_Scale(state->dpi, width96);
        control->bodyLayoutHeight = DialogModern_Scale(state->dpi, height96);
        control->bodyLayoutY96 = y96;

        int controlBottom = control->bodyLayoutY +
                            control->bodyLayoutHeight;
        BOOL completelyOutside = controlBottom <= viewportTop ||
                                 control->bodyLayoutY >= viewportBottom;
        if (suppressRedraw && completelyOutside &&
            control->bodyRegionMode == MODERN_BODY_REGION_HIDDEN) {
            continue;
        }
        control->bodyLayoutPending = TRUE;
        pendingCount++;
    }

    UINT positionFlags = SWP_NOZORDER | SWP_NOACTIVATE |
        (suppressRedraw ? SWP_NOREDRAW | SWP_NOCOPYBITS : 0);
    HDWP deferred = pendingCount > 0
        ? BeginDeferWindowPos((int)pendingCount) : NULL;
    BOOL deferredComplete = deferred != NULL;
    if (deferredComplete) {
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernControl* control = &state->controls[i];
            if (!control->bodyLayoutPending) continue;
            deferred = DeferWindowPos(
                deferred, control->hwnd, NULL,
                control->bodyLayoutX, control->bodyLayoutY,
                control->bodyLayoutWidth, control->bodyLayoutHeight,
                positionFlags);
            if (!deferred) {
                deferredComplete = FALSE;
                break;
            }
        }
        if (deferredComplete && !EndDeferWindowPos(deferred)) {
            deferredComplete = FALSE;
        }
    }

    if (!deferredComplete) {
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernControl* control = &state->controls[i];
            if (!control->bodyLayoutPending) continue;
            SetWindowPos(control->hwnd, NULL,
                         control->bodyLayoutX, control->bodyLayoutY,
                         control->bodyLayoutWidth, control->bodyLayoutHeight,
                         positionFlags);
        }
    }

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->bodyLayoutPending) continue;
        ModernApplyBodyControlRegion(state, control,
                                      control->bodyLayoutY96,
                                      suppressRedraw);
    }
}

static void ModernLayoutControls(ModernDialogState* state) {
    if (!state || !state->finalized) return;
    ModernLayoutBodyControls(state, FALSE);

    ModernControl** footer = NULL;
    size_t footerCount = 0;
    if (state->hasFooter) {
        footer = (ModernControl**)calloc(state->controlCount, sizeof(*footer));
        if (footer) {
            for (size_t i = 0; i < state->controlCount; i++) {
                if (state->controls[i].footer) {
                    footer[footerCount++] = &state->controls[i];
                }
            }
        }
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
    SetWindowLongPtrW(info.hwndList, GWL_STYLE,
                      style & ~(WS_BORDER | WS_HSCROLL | WS_VSCROLL));
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE,
                      exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    DialogModern_ApplyTheme(info.hwndList,
                            control->owner->palette.darkMode);
    DialogModern_DisablePopupShadow(info.hwndList);
    SetWindowSubclass(info.hwndList, ModernComboListSubclassProc,
                      MODERN_COMBO_LIST_SUBCLASS_ID,
                      (DWORD_PTR)control);
    ModernApplyComboListRegion(info.hwndList, control);
    RedrawWindow(info.hwndList, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_FRAME);
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
     * exposes intermediate frames on slower machines.  Freeze the short
     * layout transaction, then invalidate one complete frame.  Keeping the
     * repaint asynchronous lets consecutive wheel/drag messages coalesce;
     * forcing every intermediate frame here makes scrolling visibly stall. */
    SendMessageW(state->hwnd, WM_SETREDRAW, FALSE, 0);
    ModernLayoutBodyControls(state, TRUE);
    SendMessageW(state->hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN);
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

static void ModernDrawComboItemContent(ModernControl* control, HDC hdc,
                                       const RECT* itemRect, UINT itemId,
                                       UINT itemState) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !hdc || !itemRect || itemId == (UINT)-1) return;

    RECT rect = *itemRect;
    COLORREF popupSurface = state->palette.darkMode
        ? ModernBlendColor(state->palette.surface, state->palette.field, 58)
        : state->palette.surface;
    HBRUSH popupBrush = CreateSolidBrush(popupSurface);
    FillRect(hdc, &rect, popupBrush);
    DeleteObject(popupBrush);

    BOOL selected = (itemState & ODS_SELECTED) != 0 ||
                    control->comboHotItem == (int)itemId;
    BOOL chosen = (int)itemId ==
        (int)SendMessageW(control->hwnd, CB_GETCURSEL, 0, 0);
    BOOL disabled = (itemState & ODS_DISABLED) != 0;
    RECT selection = rect;
    int horizontalInset = DialogModern_Scale(state->dpi, 6);
    InflateRect(&selection, -horizontalInset,
                -DialogModern_Scale(state->dpi, 3));
    if (selected || chosen) {
        COLORREF selectionFill = selected
            ? ModernBlendColor(state->palette.accent, popupSurface,
                               state->palette.darkMode ? 69 : 84)
            : ModernBlendColor(state->palette.accent, popupSurface,
                               state->palette.darkMode ? 83 : 92);
        COLORREF selectionBorder = selected
            ? ModernBlendColor(state->palette.accent, popupSurface, 32)
            : selectionFill;
        DialogModern_DrawRoundedRect(hdc, &selection,
                                     DialogModern_Scale(state->dpi, 12),
                                     selectionFill, selectionBorder,
                                     selected ? 1 : 0);
    }

    wchar_t text[512] = {0};
    SendMessageW(control->hwnd, CB_GETLBTEXT, itemId, (LPARAM)text);
    RECT textRect = rect;
    textRect.left += DialogModern_Scale(state->dpi, 16);
    textRect.right -= DialogModern_Scale(state->dpi, chosen ? 34 : 24);
    COLORREF textColor = disabled ? state->palette.mutedText :
        (selected || chosen ? state->palette.accent : state->palette.text);
    DialogModern_DrawText(hdc, state->editFont, textColor,
                          &textRect, text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);

    if (chosen) {
        int centerX = rect.right - DialogModern_Scale(state->dpi, 18);
        int centerY = (rect.top + rect.bottom) / 2;
        int arm = max(2, DialogModern_Scale(state->dpi, 3));
        LOGBRUSH penBrush = {BS_SOLID, state->palette.accent, 0};
        HPEN checkPen = ExtCreatePen(
            PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
            (DWORD)max(1, DialogModern_Scale(state->dpi, 2)),
            &penBrush, 0, NULL);
        HGDIOBJ oldPen = checkPen ? SelectObject(hdc, checkPen) : NULL;
        if (checkPen) {
            MoveToEx(hdc, centerX - arm, centerY, NULL);
            LineTo(hdc, centerX - arm / 3, centerY + arm);
            LineTo(hdc, centerX + arm, centerY - arm);
        }
        if (oldPen) SelectObject(hdc, oldPen);
        if (checkPen) DeleteObject(checkPen);
    }
}

static void ModernDrawComboItem(ModernDialogState* state,
                                const DRAWITEMSTRUCT* item) {
    ModernControl* control = ModernFindControl(state, item->hwndItem);
    if (!control && item->CtlID != 0) {
        HWND combo = GetDlgItem(state->hwnd, (int)item->CtlID);
        control = ModernFindControl(state, combo);
    }
    if (!control || control->kind != MODERN_CONTROL_COMBO ||
        ModernIsDateTimeControl(control)) {
        return;
    }

    if (item->itemID == (UINT)-1) {
        COLORREF popupSurface = state->palette.darkMode
            ? ModernBlendColor(state->palette.surface,
                               state->palette.field, 58)
            : state->palette.surface;
        HBRUSH popupBrush = CreateSolidBrush(popupSurface);
        FillRect(item->hDC, &item->rcItem, popupBrush);
        DeleteObject(popupBrush);
        return;
    }
    ModernDrawComboItemContent(control, item->hDC, &item->rcItem,
                               item->itemID, item->itemState);
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
    state->titleFrame.left = titleRect.left;
    state->titleFrame.top = titleRect.top +
        ((titleRect.bottom - titleRect.top) - titleSize.cy) / 2;
    state->titleFrame.right = state->titleFrame.left + titleSize.cx;
    if (state->titleFrame.right > titleRect.right) {
        state->titleFrame.right = titleRect.right;
    }
    state->titleFrame.bottom = state->titleFrame.top + titleSize.cy;
    RECT signatureRect = titleRect;
    signatureRect.bottom = DialogModern_Scale(
        state->dpi, state->headerHeight96 - 17);
    DialogModern_DrawTitleSignature(
        hdc, &signatureRect, state->dpi, titleSize.cx, accentColor,
        state->palette.surface, state->palette.darkMode,
        state->palette.highContrast);
    COLORREF titleColor = state->palette.highContrast
        ? state->palette.text
        : (state->titleHovered ? MODERN_TITLE_HOVER_COLOR
                               : state->palette.accent);
    DialogModern_DrawText(hdc, state->titleFont, titleColor,
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

static void ModernDrawFieldOutlineToDc(ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd || !hdc) return;
    RECT rect = {0};
    GetClientRect(control->hwnd, &rect);
    InflateRect(&rect, -1, -1);
    COLORREF border = control->focused
        ? state->palette.accent
        : (control->hovered
               ? ModernBlendColor(state->palette.border,
                                  state->palette.accent, 38)
               : state->palette.border);
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
}

static void ModernDrawFieldOutline(ModernControl* control) {
    if (!control || !control->hwnd) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;
    ModernDrawFieldOutlineToDc(control, hdc);
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
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = width > 0 && height > 0 ? CreateCompatibleDC(hdc) : NULL;
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(hdc, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    HDC drawDc = buffer && bitmap ? buffer : hdc;
    FillRect(drawDc, &client, state->surfaceBrush);

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

    DialogModern_DrawRoundedRect(drawDc, &channel, channelThickness,
                                 state->palette.border,
                                 state->palette.border, 0);
    DialogModern_DrawRoundedRect(drawDc, &completed, channelThickness,
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
    HGDIOBJ oldBrush = thumbBrush ? SelectObject(drawDc, thumbBrush) : NULL;
    HGDIOBJ oldPen = thumbPen ? SelectObject(drawDc, thumbPen) : NULL;
    Ellipse(drawDc, thumb.x - thumbRadius, thumb.y - thumbRadius,
            thumb.x + thumbRadius + 1, thumb.y + thumbRadius + 1);
    if (oldPen) SelectObject(drawDc, oldPen);
    if (oldBrush) SelectObject(drawDc, oldBrush);
    if (thumbPen) DeleteObject(thumbPen);
    if (thumbBrush) DeleteObject(thumbBrush);

    if (buffer && bitmap) {
        BitBlt(hdc, client.left, client.top, width, height,
               buffer, client.left, client.top, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
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

typedef struct {
    RECT part[3];
    RECT stepper;
    RECT stepUp;
    RECT stepDown;
    RECT content;
} ModernDateTimeLayout;

static BOOL ModernGetDateTimeLayout(const ModernControl* control,
                                    ModernDateTimeLayout* layout) {
    if (!control || !control->owner || !control->hwnd || !layout) return FALSE;
    ZeroMemory(layout, sizeof(*layout));
    ModernDialogState* state = control->owner;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return FALSE;

    int inset = max(2, DialogModern_Scale(state->dpi, 5));
    int stepperWidth = max(DialogModern_Scale(state->dpi, 24), height - inset * 2);
    if (stepperWidth > width / 3) stepperWidth = max(18, width / 4);
    layout->stepper.left = max(client.left, client.right - stepperWidth - inset);
    layout->stepper.top = client.top + inset;
    layout->stepper.right = client.right - inset;
    layout->stepper.bottom = client.bottom - inset;
    layout->stepUp = layout->stepper;
    layout->stepUp.bottom = (layout->stepper.top + layout->stepper.bottom) / 2;
    layout->stepDown = layout->stepper;
    layout->stepDown.top = layout->stepUp.bottom;

    layout->content.left = client.left + inset;
    layout->content.right = layout->stepper.left - inset;
    layout->content.top = client.top + inset;
    layout->content.bottom = client.bottom - inset;
    int contentWidth = layout->content.right - layout->content.left;
    int gap = DialogModern_Scale(state->dpi, 7);
    if (contentWidth < gap * 2 + DialogModern_Scale(state->dpi, 36)) {
        gap = max(2, DialogModern_Scale(state->dpi, 4));
    }
    int partWidth = (contentWidth - gap * 2) / 3;
    if (partWidth < 1) partWidth = 1;
    for (int i = 0; i < 3; i++) {
        layout->part[i].left = layout->content.left + i * (partWidth + gap);
        layout->part[i].right = layout->part[i].left + partWidth;
        layout->part[i].top = layout->content.top;
        layout->part[i].bottom = layout->content.bottom;
    }
    layout->part[2].right = layout->content.right;
    return TRUE;
}

static int ModernDateTimePartMaximum(int part) {
    return part == MODERN_DATETIME_HOUR ? 23 : 59;
}

static BOOL ModernReadDateTime(const ModernControl* control, SYSTEMTIME* value) {
    if (!control || !control->hwnd || !value) return FALSE;
    return SendMessageW(control->hwnd, DTM_GETSYSTEMTIME, 0,
                        (LPARAM)value) == GDT_VALID;
}

static void ModernResetDateTimeInput(ModernControl* control) {
    if (!control) return;
    control->dateTimeDigitValue = 0;
    control->dateTimeDigitCount = 0;
    control->dateTimeDigitTick = 0;
}

static BOOL ModernWriteDateTimePart(ModernControl* control, int part, int value) {
    if (!control || !control->hwnd || part < 0 || part > 2 ||
        !IsWindowEnabled(control->hwnd)) return FALSE;
    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time)) return FALSE;
    int maximum = ModernDateTimePartMaximum(part);
    if (value < 0) value = 0;
    if (value > maximum) value = maximum;
    if (part == MODERN_DATETIME_HOUR) time.wHour = (WORD)value;
    else if (part == MODERN_DATETIME_MINUTE) time.wMinute = (WORD)value;
    else time.wSecond = (WORD)value;
    LRESULT result = SendMessageW(control->hwnd, DTM_SETSYSTEMTIME,
                                  GDT_VALID, (LPARAM)&time);
    if (result == 0) return FALSE;
    ModernResetDateTimeInput(control);
    InvalidateRect(control->hwnd, NULL, FALSE);
    return TRUE;
}

static BOOL ModernAdjustDateTimePart(ModernControl* control, int part,
                                     int delta) {
    if (!control || delta == 0) return FALSE;
    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time) || !IsWindowEnabled(control->hwnd)) {
        return FALSE;
    }
    int current = part == MODERN_DATETIME_HOUR ? time.wHour
                 : part == MODERN_DATETIME_MINUTE ? time.wMinute : time.wSecond;
    int maximum = ModernDateTimePartMaximum(part);
    int span = maximum + 1;
    int next = (current + (delta % span) + span) % span;
    return ModernWriteDateTimePart(control, part, next);
}

static int ModernDateTimeHitTest(const ModernDateTimeLayout* layout, POINT point) {
    if (!layout) return MODERN_DATETIME_HIT_NONE;
    if (PtInRect(&layout->stepUp, point)) return MODERN_DATETIME_STEP_UP;
    if (PtInRect(&layout->stepDown, point)) return MODERN_DATETIME_STEP_DOWN;
    for (int i = 0; i < 3; i++) {
        if (PtInRect(&layout->part[i], point)) return i;
    }
    if (PtInRect(&layout->content, point)) {
        int best = 0;
        int bestDistance = INT_MAX;
        for (int i = 0; i < 3; i++) {
            int center = (layout->part[i].left + layout->part[i].right) / 2;
            int distance = abs(point.x - center);
            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    }
    return MODERN_DATETIME_HIT_NONE;
}

static void ModernSelectDateTimePart(ModernControl* control, int part) {
    if (!control) return;
    if (part < MODERN_DATETIME_HOUR) part = MODERN_DATETIME_HOUR;
    if (part > MODERN_DATETIME_SECOND) part = MODERN_DATETIME_SECOND;
    if (control->dateTimeSelectedPart != part ||
        control->dateTimeDigitCount != 0) {
        if (control->dateTimeSelectedPart != part) {
            control->dateTimeWheelDelta = 0;
        }
        control->dateTimeSelectedPart = part;
        ModernResetDateTimeInput(control);
        InvalidateRect(control->hwnd, NULL, FALSE);
    }
}

static BOOL ModernInputDateTimeDigit(ModernControl* control, int digit) {
    if (!control || digit < 0 || digit > 9 ||
        !IsWindowEnabled(control->hwnd)) return FALSE;
    int part = control->dateTimeSelectedPart;
    if (part < MODERN_DATETIME_HOUR || part > MODERN_DATETIME_SECOND) {
        part = MODERN_DATETIME_HOUR;
        control->dateTimeSelectedPart = part;
    }
    DWORD now = GetTickCount();
    BOOL continuing = control->dateTimeDigitCount == 1 &&
        (DWORD)(now - control->dateTimeDigitTick) <=
            MODERN_DATETIME_INPUT_TIMEOUT_MS;
    int maximum = ModernDateTimePartMaximum(part);
    if (continuing) {
        int candidate = control->dateTimeDigitValue * 10 + digit;
        if (candidate <= maximum) {
            if (!ModernWriteDateTimePart(control, part, candidate)) return FALSE;
            if (part < MODERN_DATETIME_SECOND) {
                ModernSelectDateTimePart(control, part + 1);
            }
            return TRUE;
        }
    }

    if (!ModernWriteDateTimePart(control, part, digit)) return FALSE;
    if (digit > maximum / 10) {
        if (part < MODERN_DATETIME_SECOND) {
            ModernSelectDateTimePart(control, part + 1);
        }
    } else {
        control->dateTimeDigitValue = digit;
        control->dateTimeDigitCount = 1;
        control->dateTimeDigitTick = now;
    }
    return TRUE;
}

static void ModernStopDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    KillTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER);
    control->dateTimeRepeatStarted = FALSE;
}

static void ModernStartDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    ModernStopDateTimeRepeat(control);
    SetTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER, 420, NULL);
}

static void ModernPaintDateTime(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = width > 0 && height > 0 ? CreateCompatibleDC(hdc) : NULL;
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(hdc, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    HDC drawDc = buffer && bitmap ? buffer : hdc;
    FillRect(drawDc, &client, state->fieldBrush);

    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time)) GetLocalTime(&time);
    ModernDateTimeLayout layout = {0};
    if (ModernGetDateTimeLayout(control, &layout)) {
        BOOL enabled = IsWindowEnabled(control->hwnd);
        COLORREF normalText = enabled ? state->palette.text : state->palette.mutedText;
        COLORREF selectedFill = state->palette.highContrast
            ? state->palette.accent
            : ModernBlendColor(state->palette.field, state->palette.accent,
                               state->palette.darkMode ? 24 : 13);
        COLORREF hoverFill = ModernBlendColor(
            state->palette.field, state->palette.surface,
            state->palette.darkMode ? 42 : 58);
        wchar_t digits[3][4] = {0};
        StringCchPrintfW(digits[0], _countof(digits[0]), L"%02u", time.wHour);
        StringCchPrintfW(digits[1], _countof(digits[1]), L"%02u", time.wMinute);
        StringCchPrintfW(digits[2], _countof(digits[2]), L"%02u", time.wSecond);
        for (int i = 0; i < 3; i++) {
            BOOL selected = enabled && control->focused &&
                            control->dateTimeSelectedPart == i;
            BOOL hovered = enabled && control->dateTimeHotPart == i;
            if (selected || hovered) {
                DialogModern_DrawRoundedRect(
                    drawDc, &layout.part[i], DialogModern_Scale(state->dpi, 5),
                    selected ? selectedFill : hoverFill,
                    selected ? selectedFill : hoverFill, 0);
            }
            RECT textRect = layout.part[i];
            DialogModern_DrawText(
                drawDc, state->editFont,
                selected ? (state->palette.highContrast
                                ? state->palette.surface
                                : state->palette.accent)
                         : normalText, &textRect,
                digits[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        COLORREF separator = enabled ? state->palette.mutedText : normalText;
        SetBkMode(drawDc, TRANSPARENT);
        for (int i = 0; i < 2; i++) {
            RECT colon = {layout.part[i].right,
                          layout.content.top,
                          layout.part[i + 1].left,
                          layout.content.bottom};
            DialogModern_DrawText(drawDc, state->editFont, separator, &colon,
                                  L":", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        COLORREF divider = ModernBlendColor(state->palette.border,
                                             state->palette.field, 48);
        HPEN dividerPen = CreatePen(PS_SOLID, 1, divider);
        HGDIOBJ oldPen = dividerPen ? SelectObject(drawDc, dividerPen) : NULL;
        int dividerX = layout.stepper.left - DialogModern_Scale(state->dpi, 3);
        MoveToEx(drawDc, dividerX, layout.stepper.top, NULL);
        LineTo(drawDc, dividerX, layout.stepper.bottom);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (dividerPen) DeleteObject(dividerPen);

        BOOL stepUpHot = enabled && control->dateTimeHotPart == MODERN_DATETIME_STEP_UP;
        BOOL stepDownHot = enabled && control->dateTimeHotPart == MODERN_DATETIME_STEP_DOWN;
        BOOL stepUpPressed = control->dateTimePressedPart == MODERN_DATETIME_STEP_UP;
        BOOL stepDownPressed = control->dateTimePressedPart == MODERN_DATETIME_STEP_DOWN;
        if (stepUpHot || stepUpPressed) {
            DialogModern_DrawRoundedRect(drawDc, &layout.stepUp,
                                         DialogModern_Scale(state->dpi, 5),
                                         stepUpPressed ? selectedFill : hoverFill,
                                         stepUpPressed ? selectedFill : hoverFill, 0);
        }
        if (stepDownHot || stepDownPressed) {
            DialogModern_DrawRoundedRect(drawDc, &layout.stepDown,
                                         DialogModern_Scale(state->dpi, 5),
                                         stepDownPressed ? selectedFill : hoverFill,
                                         stepDownPressed ? selectedFill : hoverFill, 0);
        }
        COLORREF idleArrow = enabled ? state->palette.text
                                     : state->palette.mutedText;
        COLORREF upArrow = stepUpHot || stepUpPressed
            ? (state->palette.highContrast && stepUpPressed
                   ? state->palette.surface : state->palette.accent)
            : idleArrow;
        COLORREF downArrow = stepDownHot || stepDownPressed
            ? (state->palette.highContrast && stepDownPressed
                   ? state->palette.surface : state->palette.accent)
            : idleArrow;
        int arrowWidth = max(1, DialogModern_Scale(state->dpi, 1));
        int centerX = (layout.stepper.left + layout.stepper.right) / 2;
        int arm = max(2, DialogModern_Scale(state->dpi, 3));
        int upY = (layout.stepUp.top + layout.stepUp.bottom) / 2;
        int downY = (layout.stepDown.top + layout.stepDown.bottom) / 2;
        HPEN arrowPen = CreatePen(PS_SOLID, arrowWidth, upArrow);
        oldPen = arrowPen ? SelectObject(drawDc, arrowPen) : NULL;
        MoveToEx(drawDc, centerX - arm, upY + arm / 2, NULL);
        LineTo(drawDc, centerX, upY - arm / 2);
        LineTo(drawDc, centerX + arm, upY + arm / 2);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (arrowPen) DeleteObject(arrowPen);
        arrowPen = CreatePen(PS_SOLID, arrowWidth, downArrow);
        oldPen = arrowPen ? SelectObject(drawDc, arrowPen) : NULL;
        MoveToEx(drawDc, centerX - arm, downY - arm / 2, NULL);
        LineTo(drawDc, centerX, downY + arm / 2);
        LineTo(drawDc, centerX + arm, downY - arm / 2);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (arrowPen) DeleteObject(arrowPen);
    }
    ModernDrawFieldOutlineToDc(control, drawDc);

    if (buffer && bitmap) {
        BitBlt(hdc, client.left, client.top, width, height,
               buffer, client.left, client.top, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

static void ModernTrackNonClientMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
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
                int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
                int visibleItems = ModernGetComboListVisibleItems(
                    hwnd, control);
                int maximumTop = max(0, count - visibleItems);
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
                    int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
                    int visibleItems = ModernGetComboListVisibleItems(
                        hwnd, control);
                    int maximumTop = max(0, count - visibleItems);
                    int travel = (track.bottom - track.top) -
                                 (thumb.bottom - thumb.top);
                    if (travel > 0) {
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
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (control && control->comboScrollDragging &&
                (HWND)lParam != hwnd) {
                control->comboScrollDragging = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
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

static BOOL ModernSetSliderPosition(ModernControl* control, int position,
                                    UINT notification) {
    if (!control || !control->hwnd ||
        control->kind != MODERN_CONTROL_SLIDER) return FALSE;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    if (position < minimum) position = minimum;
    if (position > maximum) position = maximum;
    int previous = (int)SendMessageW(control->hwnd, TBM_GETPOS, 0, 0);
    if (position == previous) return FALSE;
    SendMessageW(control->hwnd, TBM_SETPOS, FALSE, position);
    InvalidateRect(control->hwnd, NULL, FALSE);
    HWND parent = GetParent(control->hwnd);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    UINT message = (style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    SendMessageW(parent, message, MAKEWPARAM(notification, position),
                 (LPARAM)control->hwnd);
    return TRUE;
}

static BOOL ModernSetSliderFromPoint(ModernControl* control, int x, int y,
                                     UINT notification) {
    if (!control || !control->hwnd) return FALSE;
    int position = ModernSliderPositionFromPoint(control, x, y);
    return ModernSetSliderPosition(control, position, notification);
}

static ModernControl* ModernFindWheelControl(ModernDialogState* state,
                                             POINT screenPoint) {
    if (!state) return NULL;
    HWND hitWindow = WindowFromPoint(screenPoint);
    if (!hitWindow) return NULL;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->hwnd || !IsWindowVisible(control->hwnd) ||
            !IsWindowEnabled(control->hwnd) ||
            (control->kind != MODERN_CONTROL_SLIDER &&
             !ModernIsDateTimeControl(control))) {
            continue;
        }
        if (hitWindow == control->hwnd ||
            IsChild(control->hwnd, hitWindow)) {
            return control;
        }
    }
    return NULL;
}

static BOOL ModernWindowOwnsFocus(HWND owner, HWND focused) {
    if (!owner || !focused) return FALSE;
    HWND current = focused;
    while (current) {
        if (current == owner || IsChild(owner, current)) return TRUE;
        HWND parent = GetParent(current);
        if (!parent || parent == current) break;
        current = parent;
    }
    return FALSE;
}

static void ModernClearFocusedChild(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    HWND focused = GetFocus();
    if (ModernWindowOwnsFocus(state->hwnd, focused)) {
        SetFocus(state->hwnd);
    }
}

static BOOL ModernPointIsPassiveContent(ModernDialogState* state,
                                        POINT point) {
    if (!state || !state->hwnd) return FALSE;
    HWND target = ChildWindowFromPointEx(
        state->hwnd, point,
        CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
    if (!target || target == state->hwnd) return TRUE;
    while (target && GetParent(target) != state->hwnd) {
        target = GetParent(target);
    }
    ModernControl* control = ModernFindControl(state, target);
    return control && (control->kind == MODERN_CONTROL_OTHER ||
                       control->kind == MODERN_CONTROL_GROUP);
}

static BOOL ModernCursorIsOverPassiveContent(ModernDialogState* state) {
    if (!state || !state->hwnd) return FALSE;
    POINT point = {0};
    RECT client = {0};
    if (!GetCursorPos(&point) ||
        !ScreenToClient(state->hwnd, &point) ||
        !GetClientRect(state->hwnd, &client) ||
        !PtInRect(&client, point)) {
        return FALSE;
    }
    return ModernPointIsPassiveContent(state, point);
}

static BOOL ModernHandleInteractiveWheel(ModernDialogState* state,
                                         WPARAM wParam, LPARAM lParam) {
    POINT screenPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ModernControl* target = ModernFindWheelControl(state, screenPoint);
    if (!target) return FALSE;
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta == 0) return TRUE;

    if (target->kind == MODERN_CONTROL_SLIDER) {
        target->sliderWheelDelta += delta;
        int detents = target->sliderWheelDelta / WHEEL_DELTA;
        target->sliderWheelDelta %= WHEEL_DELTA;
        if (detents != 0) {
            int lineSize = (int)SendMessageW(target->hwnd, TBM_GETLINESIZE, 0, 0);
            if (lineSize <= 0) lineSize = 1;
            int current = (int)SendMessageW(target->hwnd, TBM_GETPOS, 0, 0);
            long long requested = (long long)current +
                                  (long long)detents * lineSize;
            if (requested < INT_MIN) requested = INT_MIN;
            if (requested > INT_MAX) requested = INT_MAX;
            ModernSetSliderPosition(target, (int)requested, TB_THUMBPOSITION);
        }
        return TRUE;
    }

    ModernDateTimeLayout layout = {0};
    POINT clientPoint = screenPoint;
    ScreenToClient(target->hwnd, &clientPoint);
    if (ModernGetDateTimeLayout(target, &layout)) {
        int hit = ModernDateTimeHitTest(&layout, clientPoint);
        if (hit >= MODERN_DATETIME_HOUR && hit <= MODERN_DATETIME_SECOND) {
            ModernSelectDateTimePart(target, hit);
        }
    }
    target->dateTimeWheelDelta += delta;
    int detents = target->dateTimeWheelDelta / WHEEL_DELTA;
    target->dateTimeWheelDelta %= WHEEL_DELTA;
    if (detents != 0) {
        ModernAdjustDateTimePart(target, target->dateTimeSelectedPart, detents);
    }
    return TRUE;
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
            if (control && ModernIsDateTimeControl(control)) {
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                if (hit != control->dateTimeHotPart) {
                    control->dateTimeHotPart = hit;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
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
            if (control && (control->hovered ||
                            control->dateTimeHotPart !=
                                MODERN_DATETIME_HIT_NONE)) {
                control->hovered = FALSE;
                control->dateTimeHotPart = MODERN_DATETIME_HIT_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_LBUTTONDBLCLK:
            if (!control || !ModernIsDateTimeControl(control)) break;
            /* fall through */
        case WM_LBUTTONDOWN:
            if (control && control->kind == MODERN_CONTROL_GROUP) {
                ModernClearFocusedChild(state);
            }
            if (control && ModernIsDateTimeControl(control)) {
                if (!IsWindowEnabled(hwnd)) return 0;
                SetFocus(hwnd);
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                control->dateTimeHotPart = hit;
                if (hit >= MODERN_DATETIME_HOUR &&
                    hit <= MODERN_DATETIME_SECOND) {
                    ModernSelectDateTimePart(control, hit);
                } else if (hit == MODERN_DATETIME_STEP_UP ||
                           hit == MODERN_DATETIME_STEP_DOWN) {
                    control->dateTimePressedPart = hit;
                    SetCapture(hwnd);
                    ModernAdjustDateTimePart(
                        control, control->dateTimeSelectedPart,
                        hit == MODERN_DATETIME_STEP_UP ? 1 : -1);
                    ModernStartDateTimeRepeat(control);
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
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
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
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
                if (ModernIsDateTimeControl(control)) {
                    ModernStopDateTimeRepeat(control);
                    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                }
                control->pressed = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_CANCELMODE:
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_TIMER:
            if (control && ModernIsDateTimeControl(control) &&
                wParam == MODERN_DATETIME_REPEAT_TIMER) {
                if (!control->dateTimeRepeatStarted) {
                    control->dateTimeRepeatStarted = TRUE;
                    SetTimer(hwnd, MODERN_DATETIME_REPEAT_TIMER, 70, NULL);
                }
                if (GetCapture() == hwnd &&
                    control->dateTimeHotPart == control->dateTimePressedPart &&
                    (control->dateTimePressedPart == MODERN_DATETIME_STEP_UP ||
                     control->dateTimePressedPart == MODERN_DATETIME_STEP_DOWN)) {
                    ModernAdjustDateTimePart(
                        control, control->dateTimeSelectedPart,
                        control->dateTimePressedPart == MODERN_DATETIME_STEP_UP
                            ? 1 : -1);
                }
                return 0;
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
                if (ModernIsDateTimeControl(control)) {
                    ModernResetDateTimeInput(control);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_ERASEBKGND:
            if (control && (control->kind == MODERN_CONTROL_SLIDER ||
                            ModernIsDateTimeControl(control))) {
                return 1;
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
        case WM_SIZE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            ModernApplyFieldRegion(control);
            ModernApplyEditLayout(control);
            ModernHideDateTimeSpinner(control);
            return result;
        }
        case WM_MOUSEWHEEL:
            if (state && ModernHandleInteractiveWheel(state, wParam, lParam)) {
                return 0;
            }
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
        case WM_PASTE:
            if (control && ModernIsCompactEdit(control) &&
                ModernPasteCompactEdit(hwnd)) {
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                IsWindowEnabled(hwnd) &&
                (wParam == VK_LEFT || wParam == VK_RIGHT ||
                 wParam == VK_UP || wParam == VK_DOWN ||
                 wParam == VK_HOME || wParam == VK_END ||
                 wParam == VK_PRIOR || wParam == VK_NEXT)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                InvalidateRect(hwnd, NULL, FALSE);
                return result;
            }
            if (control && ModernIsDateTimeControl(control) &&
                IsWindowEnabled(hwnd)) {
                switch (wParam) {
                    case VK_LEFT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart - 1);
                        return 0;
                    case VK_RIGHT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart + 1);
                        return 0;
                    case VK_UP:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, 1);
                        return 0;
                    case VK_DOWN:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, -1);
                        return 0;
                    case VK_HOME:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_HOUR);
                        return 0;
                    case VK_END:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_SECOND);
                        return 0;
                }
            }
            if (control && ModernIsCompactEdit(control) &&
                wParam == VK_RETURN && state && state->hwnd) {
                LRESULT defaultId = SendMessageW(state->hwnd, DM_GETDEFID,
                                                 0, 0);
                if (HIWORD(defaultId) == DC_HASDEFID) {
                    int id = LOWORD(defaultId);
                    HWND button = GetDlgItem(state->hwnd, id);
                    if (button && IsWindowVisible(button) &&
                        IsWindowEnabled(button)) {
                        SendMessageW(state->hwnd, WM_COMMAND,
                                     MAKEWPARAM(id, BN_CLICKED),
                                     (LPARAM)button);
                    }
                }
                return 0;
            }
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
        case WM_CHAR:
            if (control && ModernIsDateTimeControl(control)) {
                if (wParam >= L'0' && wParam <= L'9') {
                    ModernInputDateTimeDigit(control, (int)(wParam - L'0'));
                }
                return 0;
            }
            if (control && ModernIsCompactEdit(control) &&
                (wParam == L'\r' || wParam == L'\n')) {
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (control && ModernIsDateTimeControl(control)) {
                LPCWSTR cursor = !IsWindowEnabled(hwnd)
                    ? IDC_ARROW
                    : (control->dateTimeHotPart == MODERN_DATETIME_STEP_UP ||
                       control->dateTimeHotPart == MODERN_DATETIME_STEP_DOWN
                           ? IDC_HAND : IDC_IBEAM);
                SetCursor(LoadCursorW(NULL, cursor));
                return TRUE;
            }
            if (control && (control->kind == MODERN_CONTROL_CLOSE ||
                            control->kind == MODERN_CONTROL_PUSH)) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_GETDLGCODE:
            if (control && ModernIsDateTimeControl(control)) {
                return DefSubclassProc(hwnd, msg, wParam, lParam) |
                       DLGC_WANTARROWS | DLGC_WANTCHARS;
            }
            break;
        case DTM_SETSYSTEMTIME: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control && ModernIsDateTimeControl(control)) {
                ModernResetDateTimeInput(control);
                ModernHideDateTimeSpinner(control);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return result;
        }
        case WM_ENABLE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control) {
                if (!wParam && ModernIsDateTimeControl(control)) {
                    ModernStopDateTimeRepeat(control);
                    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                }
                ModernHideDateTimeSpinner(control);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return result;
        }
        case WM_NCDESTROY:
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
            }
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
        case MODERN_DIALOG_CLEAR_FOCUS_MESSAGE:
            if (state && state->finalized &&
                ModernWindowOwnsFocus((HWND)lParam, GetFocus())) {
                ModernClearFocusedChild(state);
            }
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
            if (state && lParam) {
                const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
                ModernControl* control = ModernFindControl(state,
                                                            item->hwndItem);
                if (!control && item->CtlType == ODT_COMBOBOX &&
                    item->CtlID != 0) {
                    control = ModernFindControl(
                        state, GetDlgItem(state->hwnd, (int)item->CtlID));
                }
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
            if (state && state->finalized && HIWORD(wParam) == CBN_CLOSEUP &&
                lParam) {
                ModernControl* control = ModernFindControl(state, (HWND)lParam);
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    !ModernIsDateTimeControl(control) &&
                    ModernCursorIsOverPassiveContent(state)) {
                    PostMessageW(hwnd, MODERN_DIALOG_CLEAR_FOCUS_MESSAGE, 0,
                                 (LPARAM)control->hwnd);
                }
            }
            break;
        case WM_PARENTNOTIFY:
            if (state && state->finalized &&
                LOWORD(wParam) == WM_LBUTTONDOWN) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (ModernPointIsPassiveContent(state, point)) {
                    ModernClearFocusedChild(state);
                }
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (state && state->finalized &&
                ModernHandleInteractiveWheel(state, wParam, lParam)) {
                return 0;
            }
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
            if (state && state->finalized) {
                ModernClearFocusedChild(state);
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
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ModernUpdateTitleHover(state, point);
                ModernTrackMouse(hwnd);

                if (state->bodyScrollMax96 <= 0) break;
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
                }
            }
            break;
        case WM_NCMOUSEMOVE:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                ModernUpdateTitleHover(state, point);
                ModernTrackNonClientMouse(hwnd);
            }
            break;
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (state && state->finalized && wParam == HTCAPTION) {
                ModernClearFocusedChild(state);
            }
            break;
        case WM_MOUSELEAVE:
            if (state) {
                BOOL repaint = FALSE;
                ModernRefreshTitleHoverFromCursor(state);
                if (state->scrollBarHovered && !state->scrollBarDragging) {
                    state->scrollBarHovered = FALSE;
                    repaint = TRUE;
                }
                if (repaint) InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_NCMOUSELEAVE:
            if (state) {
                ModernRefreshTitleHoverFromCursor(state);
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
            ModernApplyEditLayout(&state->controls[i]);
            ModernHideDateTimeSpinner(&state->controls[i]);
        }
        RedrawWindow(hwndDlg, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        if (ModernGetState(hwndDlg) != state) return;
        if (!state->refreshPending) break;
    }
    state->refreshing = FALSE;
}
