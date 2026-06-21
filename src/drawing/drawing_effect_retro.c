#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"

#define RETRO_SHADOW_OFFSET_PIXELS 3

typedef struct {
    int srcLeft;
    int srcTop;
    int srcRight;
    int srcBottom;
    int destLeft;
    int destTop;
} RetroClip;

static BOOL ClipRetroBitmap(int x, int y,
                            int bitmapWidth, int bitmapHeight,
                            int destWidth, int destHeight,
                            RetroClip* clip) {
    if (!clip || bitmapWidth <= 0 || bitmapHeight <= 0 ||
        destWidth <= 0 || destHeight <= 0) {
        return FALSE;
    }

    long long left = (long long)x;
    long long top = (long long)y;
    long long right = left + (long long)bitmapWidth;
    long long bottom = top + (long long)bitmapHeight;

    if (right <= 0 || bottom <= 0 ||
        left >= (long long)destWidth || top >= (long long)destHeight) {
        return FALSE;
    }

    long long clipLeft = (left < 0) ? 0 : left;
    long long clipTop = (top < 0) ? 0 : top;
    long long clipRight = (right > (long long)destWidth) ? (long long)destWidth : right;
    long long clipBottom = (bottom > (long long)destHeight) ? (long long)destHeight : bottom;

    if (clipLeft >= clipRight || clipTop >= clipBottom) {
        return FALSE;
    }

    clip->srcLeft = (int)(clipLeft - left);
    clip->srcTop = (int)(clipTop - top);
    clip->srcRight = (int)(clipRight - left);
    clip->srcBottom = (int)(clipBottom - top);
    clip->destLeft = (int)clipLeft;
    clip->destTop = (int)clipTop;
    return TRUE;
}

static void BlendSolidColorPixel(DWORD* destPixel, unsigned char alpha,
                                 int r, int g, int b) {
    if (!destPixel || alpha == 0) return;

    DWORD currentPixel = *destPixel;
    int currentA = (int)((currentPixel >> 24) & 0xFF);
    int currentR = (int)((currentPixel >> 16) & 0xFF);
    int currentG = (int)((currentPixel >> 8) & 0xFF);
    int currentB = (int)(currentPixel & 0xFF);
    int invAlpha = 255 - (int)alpha;

    int outA = (int)alpha + (currentA * invAlpha) / 255;
    int outR = (r * (int)alpha + currentR * invAlpha) / 255;
    int outG = (g * (int)alpha + currentG * invAlpha) / 255;
    int outB = (b * (int)alpha + currentB * invAlpha) / 255;

    *destPixel = ((DWORD)outA << 24) |
                 ((DWORD)outR << 16) |
                 ((DWORD)outG << 8) |
                 (DWORD)outB;
}

static void RenderRetroShadowPass(DWORD* pixels, int destWidth, int destHeight,
                                  int x_pos, int y_pos,
                                  const unsigned char* bitmap, int w, int h,
                                  int shadowR, int shadowG, int shadowB) {
    RetroClip shadowClip;
    if (!ClipRetroBitmap(x_pos + RETRO_SHADOW_OFFSET_PIXELS,
                         y_pos + RETRO_SHADOW_OFFSET_PIXELS,
                         w, h, destWidth, destHeight, &shadowClip)) {
        return;
    }

    for (int j = shadowClip.srcTop; j < shadowClip.srcBottom; ++j) {
        int destY = shadowClip.destTop + (j - shadowClip.srcTop);
        DWORD* destRow = pixels + (size_t)destY * (size_t)destWidth + (size_t)shadowClip.destLeft;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)shadowClip.srcLeft;

        for (int i = shadowClip.srcLeft; i < shadowClip.srcRight; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                destRow++;
                continue;
            }

            BlendSolidColorPixel(destRow, alpha, shadowR, shadowG, shadowB);
            destRow++;
        }
    }
}

void RenderRetroShadowEffect(DWORD* pixels, int destWidth, int destHeight,
                             int x_pos, int y_pos,
                             const unsigned char* bitmap, int w, int h,
                             int shadowR, int shadowG, int shadowB) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    RenderRetroShadowPass(pixels, destWidth, destHeight, x_pos, y_pos,
                          bitmap, w, h, shadowR, shadowG, shadowB);
}

void RenderRetroEffect(DWORD* pixels, int destWidth, int destHeight,
                       int x_pos, int y_pos,
                       const unsigned char* bitmap, int w, int h,
                       int r, int g, int b,
                       int shadowR, int shadowG, int shadowB,
                       GlowColorCallback colorCb, void* userData) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    /* Pass 1: render the hard secondary-color shadow. */
    RenderRetroShadowPass(pixels, destWidth, destHeight, x_pos, y_pos,
                          bitmap, w, h, shadowR, shadowG, shadowB);

    /* Pass 2: render the primary foreground text. */
    RetroClip foreClip;
    if (ClipRetroBitmap(x_pos, y_pos, w, h, destWidth, destHeight, &foreClip)) {
        for (int j = foreClip.srcTop; j < foreClip.srcBottom; ++j) {
            int destY = foreClip.destTop + (j - foreClip.srcTop);
            DWORD* destRow = pixels + (size_t)destY * (size_t)destWidth + (size_t)foreClip.destLeft;
            const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)foreClip.srcLeft;

            for (int i = foreClip.srcLeft; i < foreClip.srcRight; ++i) {
                unsigned char alpha = *srcRow++;
                if (alpha == 0) {
                    destRow++;
                    continue;
                }

                int finalR = r;
                int finalG = g;
                int finalB = b;

                if (colorCb) {
                    colorCb(foreClip.destLeft + (i - foreClip.srcLeft),
                            destY, &finalR, &finalG, &finalB, userData);
                }

                BlendSolidColorPixel(destRow, alpha, finalR, finalG, finalB);
                destRow++;
            }
        }
    }
}
