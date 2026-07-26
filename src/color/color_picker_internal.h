/**
 * @file color_picker_internal.h
 * @brief Private state and cross-module helpers for the modern color picker.
 */

#ifndef COLOR_PICKER_INTERNAL_H
#define COLOR_PICKER_INTERNAL_H

#include "color/color_picker_dialog.h"
#include "dialog/dialog_modern.h"

typedef struct {
    HWND hwnd;
    HWND hwndParent;
    COLORREF selectedColor;
    COLORREF eyedropperOriginalColor;
    COLORREF* customColors;
    size_t customColorCapacity;
    size_t customColorCount;
    double hue;
    double saturation;
    double value;
    BOOL updatingFields;
    BOOL eyedropperActive;
    int savedFocusIndex;
    DWORD* svPixels;
    int svPixelWidth;
    int svPixelHeight;
    int svCachedHue;
    DWORD* huePixels;
    int huePixelWidth;
    int huePixelHeight;
} ModernColorPickerState;

double ColorPickerInternal_ClampUnit(double value);
double ColorPickerInternal_NormalizeHue(double hue);
COLORREF ColorPickerInternal_HsvToColor(
    double hue, double saturation, double value);
void ColorPickerInternal_ColorToHsv(
    COLORREF color, double previousHue, double* hue,
    double* saturation, double* value);

void ColorPickerInternal_DrawOutline(
    HDC hdc, const RECT* rect, int radius, COLORREF color, int width);
void ColorPickerInternal_RefreshPalette(
    ModernColorPickerState* state, DialogModernPalette* palette);
void ColorPickerInternal_PaintSv(
    ModernColorPickerState* state, const DRAWITEMSTRUCT* item);
void ColorPickerInternal_PaintHue(
    ModernColorPickerState* state, const DRAWITEMSTRUCT* item);

void ColorPickerInternal_PaintSavedColors(
    ModernColorPickerState* state, const DRAWITEMSTRUCT* item);
void ColorPickerInternal_PaintPreview(
    ModernColorPickerState* state, const DRAWITEMSTRUCT* item);
void ColorPickerInternal_SaveCurrentColor(ModernColorPickerState* state);
int ColorPickerInternal_SavedIndexFromPoint(
    HWND control, int x, int y);
void ColorPickerInternal_SelectSavedColor(
    ModernColorPickerState* state, int index);

void ColorPickerInternal_UpdateFields(ModernColorPickerState* state);
void ColorPickerInternal_SetColor(
    ModernColorPickerState* state, COLORREF color,
    BOOL updateHsv, BOOL preview);
void ColorPickerInternal_SetColorFromHsv(
    ModernColorPickerState* state, BOOL preview);
void ColorPickerInternal_HandleFieldChange(
    ModernColorPickerState* state, int controlId);

BOOL ColorPickerInternal_SampleScreenColor(COLORREF* color);
void ColorPickerInternal_BeginEyedropper(ModernColorPickerState* state);
void ColorPickerInternal_EndEyedropper(
    ModernColorPickerState* state, BOOL keepColor);
void ColorPickerInternal_LayoutControls(HWND hwndDlg);
void ColorPickerInternal_SetupCanvas(
    HWND hwndDlg, int controlId, ModernColorPickerState* state);

#endif /* COLOR_PICKER_INTERNAL_H */
