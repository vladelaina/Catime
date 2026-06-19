#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

void RenderNeonEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      const unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;

    int padding = 20;
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

    unsigned char* buf1 = buffers.buffer1;
    unsigned char* buf2 = buffers.buffer2;
    unsigned char* buf3 = buffers.buffer3;

    memset(buf1, 0, (size_t)neededSize);
    memset(buf2, 0, (size_t)neededSize);
    memset(buf3, 0, (size_t)neededSize);

    for (int j = 0; j < h; j++) {
        memcpy(buf1 + (j + padding) * gw + padding, bitmap + j * w, (size_t)w);
    }

    int erosion = 2;
    for (int j = 0; j < gh; j++) {
        for (int i = 0; i < gw; i++) {
            unsigned char minVal = 255;

            unsigned char val = buf1[j * gw + i];
            if (val < minVal) minVal = val;

            if (i > erosion) {
                val = buf1[j * gw + (i - erosion)];
                if (val < minVal) minVal = val;
            } else {
                minVal = 0;
            }

            if (i < gw - erosion) {
                val = buf1[j * gw + (i + erosion)];
                if (val < minVal) minVal = val;
            } else {
                minVal = 0;
            }

            if (j > erosion) {
                val = buf1[(j - erosion) * gw + i];
                if (val < minVal) minVal = val;
            } else {
                minVal = 0;
            }

            if (j < gh - erosion) {
                val = buf1[(j + erosion) * gw + i];
                if (val < minVal) minVal = val;
            } else {
                minVal = 0;
            }

            buf2[j * gw + i] = minVal;
        }
    }

    for (int k = 0; k < neededSize; k++) {
        int diff = (int)buf1[k] - (int)buf2[k];
        if (diff < 0) diff = 0;
        if (diff > 0) diff = (diff * 3) / 2;
        if (diff > 255) diff = 255;
        buf3[k] = (unsigned char)diff;
    }

    ApplyGaussianBlur(buf3, buf2, buf1, gw, gh, 2);
    ApplyGaussianBlur(buf2, buf1, buf3, gw, gh, 12);

    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);

        DWORD* pDestRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* pGlowRow = buf1 + j * gw;
        const unsigned char* pTubeRow = buf2 + j * gw;

        for (int i = firstI; i < lastI; i++) {
            int screenX = (int)(startX + (long long)i);

            int glowAlpha = pGlowRow[i];
            int tubeAlpha = pTubeRow[i];
            if (glowAlpha == 0 && tubeAlpha == 0) continue;

            int baseR = r;
            int baseG = g;
            int baseB = b;
            if (colorCb) {
                colorCb(screenX, screenY, &baseR, &baseG, &baseB, userData);
            }

            int ambR = (baseR * glowAlpha) >> 8;
            int ambG = (baseG * glowAlpha) >> 8;
            int ambB = (baseB * glowAlpha) >> 8;
            ambR = (ambR * 60) / 100;
            ambG = (ambG * 60) / 100;
            ambB = (ambB * 60) / 100;

            int tubeR = (baseR * tubeAlpha) >> 8;
            int tubeG = (baseG * tubeAlpha) >> 8;
            int tubeB = (baseB * tubeAlpha) >> 8;
            tubeR = (tubeR * 120) / 100;
            tubeG = (tubeG * 120) / 100;
            tubeB = (tubeB * 120) / 100;

            int coreR = 0;
            int coreG = 0;
            int coreB = 0;
            if (tubeAlpha > 160) {
                int coreInt = (tubeAlpha - 160) * 3;
                if (coreInt > 255) coreInt = 255;

                coreR = coreInt;
                coreG = coreInt;
                coreB = coreInt;
            }

            int finalR = ambR + tubeR + coreR;
            int finalG = ambG + tubeG + coreG;
            int finalB = ambB + tubeB + coreB;
            int finalA = (glowAlpha / 2) + tubeAlpha;

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            int outR = bgR + finalR;
            int outG = bgG + finalG;
            int outB = bgB + finalB;
            int outA = bgA + finalA;

            if (outR > 255) outR = 255;
            if (outG > 255) outG = 255;
            if (outB > 255) outB = 255;
            if (outA > 255) outA = 255;

            *pPixel = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    DrawingEffect_EndBufferUse();
}
