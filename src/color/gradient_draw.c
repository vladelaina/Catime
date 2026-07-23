#include "color/gradient.h"
#include <stdlib.h>

#define GRADIENT_STACK_PIXELS 256
#define GRADIENT_MAX_DRAW_WIDTH 16384

static DWORD ToDibRgb(COLORREF color) {
    return (DWORD)GetBValue(color) | ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

void DrawGradientRect(HDC hdc, const RECT* rect, const GradientInfo* info) {
    if (!hdc || !rect || !info) return;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0 || width > GRADIENT_MAX_DRAW_WIDTH) return;
    DWORD stack[GRADIENT_STACK_PIXELS];
    DWORD* pixels = width <= GRADIENT_STACK_PIXELS
        ? stack : (DWORD*)malloc((size_t)width * sizeof(*pixels));
    if (!pixels) return;
    for (int x = 0; x < width; ++x) {
        float t = width > 1 ? (float)x / (float)(width - 1) : 0.0f;
        pixels[x] = ToDibRgb(GetGradientColorAt(info, t));
    }
    BITMAPINFO bitmap = {0};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = 1;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, rect->left, rect->top, width, height, 0, 0, width, 1,
                  pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
    if (pixels != stack) free(pixels);
}
