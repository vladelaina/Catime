#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

void RenderGlowEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      const unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;

    int padding = 12;
    int gw = 0;
    int gh = 0;
    int neededSize = 0;

    if (!DrawingEffect_CalculateBufferSize(w, h, padding, &gw, &gh, &neededSize)) {
        return;
    }

    long long startX = (long long)x_pos - (long long)padding;
    long long startY = (long long)y_pos - (long long)padding;
    int firstI = 0;
    int lastI = 0;
    int firstJ = 0;
    int lastJ = 0;
    if (!DrawingEffect_CalculateVisibleSpan(startX, gw, destWidth, &firstI, &lastI) ||
        !DrawingEffect_CalculateVisibleSpan(startY, gh, destHeight, &firstJ, &lastJ)) {
        return;
    }

    if (!DrawingEffect_BeginBufferUse()) return;

    DrawingEffectBuffers buffers;
    if (!DrawingEffect_EnsureBuffers(neededSize, &buffers)) {
        DrawingEffect_EndBufferUse();
        return;
    }

    unsigned char* alphaMap = buffers.buffer1;
    unsigned char* glowMap = buffers.buffer2;
    unsigned char* tempBuffer = buffers.buffer3;

    memset(alphaMap, 0, (size_t)neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, (size_t)w);
    }

    ApplyGaussianBlur(alphaMap, glowMap, tempBuffer, gw, gh, 4);

    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);

        DWORD* pDestRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* pGlowRow = glowMap + j * gw;

        for (int i = firstI; i < lastI; i++) {
            int screenX = (int)(startX + (long long)i);

            int alpha = pGlowRow[i];
            if (alpha == 0) continue;

            int finalR = r;
            int finalG = g;
            int finalB = b;
            if (colorCb) {
                colorCb(screenX, screenY, &finalR, &finalG, &finalB, userData);
            }

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;

            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            int glowR = (finalR * alpha) / 255;
            int glowG = (finalG * alpha) / 255;
            int glowB = (finalB * alpha) / 255;

            int outR = bgR + glowR;
            int outG = bgG + glowG;
            int outB = bgB + glowB;

            if (outR > 255) outR = 255;
            if (outG > 255) outG = 255;
            if (outB > 255) outB = 255;

            int bgA = (bgPixel >> 24) & 0xFF;
            int outA = max(bgA, alpha);

            *pPixel = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    DrawingEffect_EndBufferUse();
}
