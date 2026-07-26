/**
 * @file color_picker_math.c
 * @brief Converts and normalizes RGB and HSV color values.
 */

#include "color/color_picker_internal.h"
double ColorPickerInternal_ClampUnit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static int PickerClampByte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

double ColorPickerInternal_NormalizeHue(double hue) {
    while (hue < 0.0) hue += 360.0;
    while (hue >= 360.0) hue -= 360.0;
    return hue;
}

COLORREF ColorPickerInternal_HsvToColor(double hue, double saturation, double value) {
    hue = ColorPickerInternal_NormalizeHue(hue);
    saturation = ColorPickerInternal_ClampUnit(saturation);
    value = ColorPickerInternal_ClampUnit(value);

    double chroma = value * saturation;
    double hueSector = hue / 60.0;
    int sector = (int)hueSector;
    double fraction = hueSector - sector;
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    switch (sector) {
        case 0:
            red = chroma;
            green = chroma * fraction;
            break;
        case 1:
            red = chroma * (1.0 - fraction);
            green = chroma;
            break;
        case 2:
            green = chroma;
            blue = chroma * fraction;
            break;
        case 3:
            green = chroma * (1.0 - fraction);
            blue = chroma;
            break;
        case 4:
            red = chroma * fraction;
            blue = chroma;
            break;
        default:
            red = chroma;
            blue = chroma * (1.0 - fraction);
            break;
    }

    double match = value - chroma;
    int redByte = PickerClampByte((int)((red + match) * 255.0 + 0.5));
    int greenByte = PickerClampByte((int)((green + match) * 255.0 + 0.5));
    int blueByte = PickerClampByte((int)((blue + match) * 255.0 + 0.5));
    return RGB(redByte, greenByte, blueByte);
}

void ColorPickerInternal_ColorToHsv(COLORREF color, double previousHue,
                             double* hue, double* saturation, double* value) {
    double red = GetRValue(color) / 255.0;
    double green = GetGValue(color) / 255.0;
    double blue = GetBValue(color) / 255.0;
    double maximum = red;
    if (green > maximum) maximum = green;
    if (blue > maximum) maximum = blue;
    double minimum = red;
    if (green < minimum) minimum = green;
    if (blue < minimum) minimum = blue;
    double delta = maximum - minimum;
    double resolvedHue = previousHue;

    if (delta > 0.0) {
        if (maximum == red) {
            resolvedHue = 60.0 * ((green - blue) / delta);
            if (resolvedHue < 0.0) resolvedHue += 360.0;
        } else if (maximum == green) {
            resolvedHue = 60.0 * (((blue - red) / delta) + 2.0);
        } else {
            resolvedHue = 60.0 * (((red - green) / delta) + 4.0);
        }
    }

    if (hue) *hue = ColorPickerInternal_NormalizeHue(resolvedHue);
    if (saturation) *saturation = maximum > 0.0 ? delta / maximum : 0.0;
    if (value) *value = maximum;
}
