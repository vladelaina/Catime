/**
 * @file tray_menu_indicators.c
 * @brief Cached native bitmaps used by tray submenu items.
 */

#include "tray_menu_submenus_internal.h"

static HBITMAP s_hUpdateDot = NULL;
static int s_updateDotCx = 0;
static int s_updateDotCy = 0;
static HBITMAP s_hSupportHeart = NULL;
static int s_supportHeartCx = 0;
static int s_supportHeartCy = 0;

static void GetMenuIndicatorBitmapSize(int* outCx, int* outCy) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;
    if (cx > 256) cx = 256;
    if (cy > 256) cy = 256;
    if (outCx) *outCx = cx;
    if (outCy) *outCy = cy;
}

static HBITMAP CreateMenuDotBitmap(int cx, int cy, DWORD color,
                                   int divisor, int minDotSize, int maxDotSize) {
    if (cx <= 0 || cy <= 0) {
        return NULL;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hDot = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hDot || !pBits) {
        if (hDot) DeleteObject(hDot);
        return NULL;
    }

    memset(pBits, 0, (size_t)cx * (size_t)cy * sizeof(DWORD));

    int dotSize = (cx < cy ? cx : cy) / divisor;
    if (dotSize < minDotSize) dotSize = minDotSize;
    if (dotSize > maxDotSize) dotSize = maxDotSize;
    int centerX = cx / 2;
    int centerY = cy / 2;
    int r = dotSize / 2;

    DWORD* pixels = (DWORD*)pBits;
    for (int y = 0; y < cy; y++) {
        for (int x = 0; x < cx; x++) {
            int dx = x - centerX;
            int dy = y - centerY;
            if (dx * dx + dy * dy <= r * r) {
                pixels[y * cx + x] = color;
            }
        }
    }

    return hDot;
}

static HBITMAP CreateMenuHeartBitmap(int cx, int cy, DWORD color) {
    if (cx <= 0 || cy <= 0) {
        return NULL;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hHeart = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hHeart || !pBits) {
        if (hHeart) DeleteObject(hHeart);
        return NULL;
    }

    memset(pBits, 0, (size_t)cx * (size_t)cy * sizeof(DWORD));

    int size = (cx < cy ? cx : cy);
    double scale = (double)size * 0.30;
    double centerX = ((double)cx - 1.0) * 0.50;
    double centerY = ((double)cy - 1.0) * 0.56;

    DWORD* pixels = (DWORD*)pBits;
    for (int y = 0; y < cy; y++) {
        for (int x = 0; x < cx; x++) {
            double nx = ((double)x - centerX) / scale;
            double ny = (centerY - (double)y) / scale;
            double q = nx * nx + ny * ny - 1.0;
            BOOL inside = q * q * q - nx * nx * ny * ny * ny <= 0.0;
            if (!inside) {
                continue;
            }

            BOOL edge = FALSE;
            const int neighbors[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (int i = 0; i < 4; i++) {
                int sx = x + neighbors[i][0];
                int sy = y + neighbors[i][1];
                if (sx < 0 || sx >= cx || sy < 0 || sy >= cy) {
                    edge = TRUE;
                    break;
                }

                double snx = ((double)sx - centerX) / scale;
                double sny = (centerY - (double)sy) / scale;
                double sq = snx * snx + sny * sny - 1.0;
                if (sq * sq * sq - snx * snx * sny * sny * sny > 0.0) {
                    edge = TRUE;
                    break;
                }
            }

            if (edge) {
                pixels[y * cx + x] = color;
            }
        }
    }

    return hHeart;
}

HBITMAP TraySubmenu_GetUpdateDotBitmap(void) {
    int cx = 0;
    int cy = 0;
    GetMenuIndicatorBitmapSize(&cx, &cy);

    if (s_hUpdateDot && (s_updateDotCx != cx || s_updateDotCy != cy)) {
        DeleteObject(s_hUpdateDot);
        s_hUpdateDot = NULL;
        s_updateDotCx = 0;
        s_updateDotCy = 0;
    }

    if (!s_hUpdateDot) {
        s_hUpdateDot = CreateMenuDotBitmap(cx, cy, 0xFFE51123, 3, 5, 6);
        if (s_hUpdateDot) {
            s_updateDotCx = cx;
            s_updateDotCy = cy;
        }
    }
    return s_hUpdateDot;
}

HBITMAP TraySubmenu_GetSupportHeartBitmap(void) {
    int cx = 0;
    int cy = 0;
    GetMenuIndicatorBitmapSize(&cx, &cy);

    if (s_hSupportHeart && (s_supportHeartCx != cx || s_supportHeartCy != cy)) {
        DeleteObject(s_hSupportHeart);
        s_hSupportHeart = NULL;
        s_supportHeartCx = 0;
        s_supportHeartCy = 0;
    }

    if (!s_hSupportHeart) {
        s_hSupportHeart = CreateMenuHeartBitmap(cx, cy, 0xFFE51123);
        if (s_hSupportHeart) {
            s_supportHeartCx = cx;
            s_supportHeartCy = cy;
        }
    }
    return s_hSupportHeart;
}

void CleanupTraySubmenuResources(void) {
    if (s_hUpdateDot) {
        DeleteObject(s_hUpdateDot);
        s_hUpdateDot = NULL;
    }
    if (s_hSupportHeart) {
        DeleteObject(s_hSupportHeart);
        s_hSupportHeart = NULL;
    }
    s_updateDotCx = 0;
    s_updateDotCy = 0;
    s_supportHeartCx = 0;
    s_supportHeartCy = 0;
}
