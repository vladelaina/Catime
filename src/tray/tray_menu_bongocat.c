/** @file tray_menu_bongocat.c @brief BongoCat menu bitmap and cache lifecycle. */

#include "tray_menu_submenus_internal.h"

static HBITMAP s_hBongoCat = NULL;
static int s_bongoCatCx = 0;
static int s_bongoCatCy = 0;

void TraySubmenu_CleanupBongoCatBitmap(void) {
    if (s_hBongoCat) {
        DeleteObject(s_hBongoCat);
        s_hBongoCat = NULL;
    }
    s_bongoCatCx = 0;
    s_bongoCatCy = 0;
}

HBITMAP TraySubmenu_GetBongoCatBitmap(void) {
    int cx = 0, cy = 0;
    TraySubmenu_GetIndicatorBitmapSize(&cx, &cy);
    if (s_hBongoCat && (s_bongoCatCx != cx || s_bongoCatCy != cy)) {
        DeleteObject(s_hBongoCat);
        s_hBongoCat = NULL;
    }
    if (s_hBongoCat) return s_hBongoCat;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        return NULL;
    }

    /* Supersample a small cat-face badge into a premultiplied menu bitmap. */
    DWORD* pixels = (DWORD*)bits;
    double size = (double)(cx < cy ? cx : cy);
    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            unsigned a = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    double u = (x + (sx + 0.5) / 4.0 - (cx - size) / 2.0) / size;
                    double v = (y + (sy + 0.5) / 4.0 - (cy - size) / 2.0) / size;
                    double dx = (u - 0.5) / 0.39, dy = (v - 0.57) / 0.30;
                    BOOL face = dx * dx + dy * dy <= 1.0;
                    BOOL ears = v >= 0.13 && v <= 0.51 &&
                        ((u >= 0.16 && u <= 0.16 + (v - 0.13) * 0.8) ||
                         (u <= 0.84 && u >= 0.84 - (v - 0.13) * 0.8));
                    if (!face && !ears) continue;
                    BOOL eyes = v >= 0.47 && v <= 0.60 &&
                        ((u >= 0.31 && u <= 0.38) || (u >= 0.62 && u <= 0.69));
                    BOOL nose = v >= 0.65 && v <= 0.72 && u >= 0.46 && u <= 0.54;
                    a += 255;
                    r += eyes || nose ? 255 : 84;
                    g += eyes || nose ? 255 : 174;
                    b += eyes || nose ? 255 : 255;
                }
            }
            pixels[y * cx + x] = ((DWORD)(a / 16) << 24) |
                ((DWORD)(r / 16) << 16) | ((DWORD)(g / 16) << 8) | (b / 16);
        }
    }
    s_hBongoCat = bitmap;
    s_bongoCatCx = cx;
    s_bongoCatCy = cy;
    return bitmap;
}
