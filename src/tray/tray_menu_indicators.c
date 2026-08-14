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
static HBITMAP s_hVlainaCheck = NULL;
static int s_vlainaCheckCx = 0;
static int s_vlainaCheckCy = 0;

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
    double centerX = (double)cx * 0.50;
    double centerY = (double)cy * 0.56;
    const int samplesPerAxis = 8;
    const int sampleCount = samplesPerAxis * samplesPerAxis;
    BYTE sourceAlpha = (BYTE)(color >> 24);
    BYTE sourceRed = (BYTE)(color >> 16);
    BYTE sourceGreen = (BYTE)(color >> 8);
    BYTE sourceBlue = (BYTE)color;

    DWORD* pixels = (DWORD*)pBits;
    for (int y = 0; y < cy; y++) {
        for (int x = 0; x < cx; x++) {
            int coveredSamples = 0;
            for (int sampleY = 0; sampleY < samplesPerAxis; ++sampleY) {
                for (int sampleX = 0; sampleX < samplesPerAxis; ++sampleX) {
                    double px = (double)x +
                        ((double)sampleX + 0.5) / samplesPerAxis;
                    double py = (double)y +
                        ((double)sampleY + 0.5) / samplesPerAxis;
                    double nx = (px - centerX) / scale;
                    double ny = (centerY - py) / scale;
                    double q = nx * nx + ny * ny - 1.0;
                    if (q * q * q - nx * nx * ny * ny * ny <= 0.0) {
                        ++coveredSamples;
                    }
                }
            }

            if (coveredSamples == 0) continue;

            BYTE alpha = (BYTE)((sourceAlpha * coveredSamples +
                                 sampleCount / 2) / sampleCount);
            BYTE red = (BYTE)((sourceRed * alpha + 127) / 255);
            BYTE green = (BYTE)((sourceGreen * alpha + 127) / 255);
            BYTE blue = (BYTE)((sourceBlue * alpha + 127) / 255);
            pixels[y * cx + x] = ((DWORD)alpha << 24) |
                                 ((DWORD)red << 16) |
                                 ((DWORD)green << 8) |
                                 blue;
        }
    }

    return hHeart;
}

static double DistanceSquaredToSegment(double px, double py,
                                       double x1, double y1,
                                       double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double lengthSquared = dx * dx + dy * dy;
    double t = lengthSquared > 0.0
        ? ((px - x1) * dx + (py - y1) * dy) / lengthSquared
        : 0.0;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    double nearestX = x1 + t * dx;
    double nearestY = y1 + t * dy;
    double offsetX = px - nearestX;
    double offsetY = py - nearestY;
    return offsetX * offsetX + offsetY * offsetY;
}

static HBITMAP CreateMenuCheckBitmap(int cx, int cy, DWORD color) {
    if (cx <= 0 || cy <= 0) return NULL;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP bitmap = CreateDIBSection(
        NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!bitmap || !pBits) {
        if (bitmap) DeleteObject(bitmap);
        return NULL;
    }

    memset(pBits, 0, (size_t)cx * (size_t)cy * sizeof(DWORD));
    double size = (double)(cx < cy ? cx : cy);
    double offsetX = ((double)cx - size) * 0.5;
    double offsetY = ((double)cy - size) * 0.5;
    double x1 = offsetX + size * 0.18;
    double y1 = offsetY + size * 0.52;
    double x2 = offsetX + size * 0.42;
    double y2 = offsetY + size * 0.72;
    double x3 = offsetX + size * 0.82;
    double y3 = offsetY + size * 0.28;
    double radius = size * 0.085;
    double radiusSquared = radius * radius;
    DWORD* pixels = (DWORD*)pBits;

    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            double px = (double)x + 0.5;
            double py = (double)y + 0.5;
            if (DistanceSquaredToSegment(px, py, x1, y1, x2, y2) <=
                    radiusSquared ||
                DistanceSquaredToSegment(px, py, x2, y2, x3, y3) <=
                    radiusSquared) {
                pixels[y * cx + x] = color;
            }
        }
    }
    return bitmap;
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

HBITMAP TraySubmenu_GetVlainaCheckBitmap(void) {
    int cx = 0;
    int cy = 0;
    GetMenuIndicatorBitmapSize(&cx, &cy);

    if (s_hVlainaCheck &&
        (s_vlainaCheckCx != cx || s_vlainaCheckCy != cy)) {
        DeleteObject(s_hVlainaCheck);
        s_hVlainaCheck = NULL;
        s_vlainaCheckCx = 0;
        s_vlainaCheckCy = 0;
    }

    if (!s_hVlainaCheck) {
        s_hVlainaCheck = CreateMenuCheckBitmap(cx, cy, 0xFFF77DAA);
        if (s_hVlainaCheck) {
            s_vlainaCheckCx = cx;
            s_vlainaCheckCy = cy;
        }
    }
    return s_hVlainaCheck;
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
    if (s_hVlainaCheck) {
        DeleteObject(s_hVlainaCheck);
        s_hVlainaCheck = NULL;
    }
    s_updateDotCx = 0;
    s_updateDotCy = 0;
    s_supportHeartCx = 0;
    s_supportHeartCy = 0;
    s_vlainaCheckCx = 0;
    s_vlainaCheckCy = 0;
}
