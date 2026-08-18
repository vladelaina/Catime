/**
 * @file dialog_modern_types.h
 * @brief Private state and type definitions for modern resource dialogs.
 */

#ifndef DIALOG_MODERN_TYPES_H
#define DIALOG_MODERN_TYPES_H

#include "dialog/dialog_modern.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <limits.h>
#include <stddef.h>
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
#define MODERN_DWM_CLOAK_ATTRIBUTE 13
#define MODERN_FEEDBACK_STATE_PROP L"Catime.ModernDialog.FeedbackState"
#define MODERN_IME_COMPOSITION_PROP L"Catime.ModernDialog.ImeComposition"
#define MODERN_COMBO_VISIBLE_ITEMS 7
#define MODERN_TITLE_HOVER_COLOR RGB(0xF7, 0x7D, 0xAA)
#define MODERN_DATETIME_REPEAT_TIMER 0xD145
#define MODERN_SCROLL_DRAG_TIMER 0xD146
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
    MODERN_CONTROL_SLIDER,
    MODERN_CONTROL_INSTRUCTION,
    MODERN_CONTROL_FEEDBACK
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
    int bodyWheelDelta;
    int scrollDragStartY;
    int scrollDragStartOffset96;
    int scrollPendingOffset96;
    RECT titleFrame;
    BOOL hasFooter;
    BOOL titleHovered;
    BOOL scrollBarHovered;
    BOOL scrollBarDragging;
    BOOL scrollUpdatePending;
    BOOL scrollUpdateTimerActive;
    BOOL attached;
    BOOL finalized;
    BOOL finalizing;
    BOOL firstShowCloaked;
    BOOL firstShowTransparent;
    BOOL refreshing;
    BOOL refreshPending;
};

typedef struct {
    RECT part[3];
    RECT stepper;
    RECT stepUp;
    RECT stepDown;
    RECT content;
} ModernDateTimeLayout;

#endif /* DIALOG_MODERN_TYPES_H */
