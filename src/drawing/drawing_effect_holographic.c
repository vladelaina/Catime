#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

void RenderHolographicEffect(DWORD* pixels, int destWidth, int destHeight,
                            int x_pos, int y_pos,
                            const unsigned char* bitmap, int w, int h,
                            int r, int g, int b,
                            GlowColorCallback colorCb, void* userData,
                            int timeOffset) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;

    (void)timeOffset;

    int padding = 16;
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
    if (firstI < 1) firstI = 1;
    if (lastI > gw - 1) lastI = gw - 1;
    if (firstJ < 1) firstJ = 1;
    if (lastJ > gh - 1) lastJ = gh - 1;
    if (firstI >= lastI || firstJ >= lastJ) {
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
    unsigned char* tempMap = buffers.buffer3;

    memset(alphaMap, 0, (size_t)neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, (size_t)w);
    }

    ApplyGaussianBlur(alphaMap, glowMap, tempMap, gw, gh, 10);
    ApplyGaussianBlur(glowMap, glowMap, tempMap, gw, gh, 10);

    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);

        DWORD* pDestRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* pAlphaRow = alphaMap + j * gw;
        const unsigned char* pGlowRow = glowMap + j * gw;
        const unsigned char* pRowPrev = alphaMap + (j - 1) * gw;
        const unsigned char* pRowNext = alphaMap + (j + 1) * gw;

        for (int i = firstI; i < lastI; i++) {
            int screenX = (int)(startX + (long long)i);

            int alphaG = pAlphaRow[i];
            unsigned char glow = pGlowRow[i];

            if (alphaG == 0 && glow == 0) {
                if (pAlphaRow[i - 1] == 0 && pAlphaRow[i + 1] == 0) continue;
            }

            int alphaR = pAlphaRow[i - 1];
            int alphaB = pAlphaRow[i + 1];

            if (alphaG == 0 && glow == 0 && alphaR == 0 && alphaB == 0) continue;

            int curR = r;
            int curG = g;
            int curB = b;

            if (colorCb) {
                colorCb(screenX, screenY, &curR, &curG, &curB, userData);
            }

            int bodyR_val = 0;
            int bodyG_val = 0;
            int bodyB_val = 0;
            int bodyA = 0;

            if (alphaR > 0 || alphaG > 0 || alphaB > 0) {
                bodyR_val = (curR * alphaR) >> 8;
                bodyG_val = (curG * alphaG) >> 8;
                bodyB_val = (curB * alphaB) >> 8;

                if (glow > 0) {
                    int volume = glow >> 2;
                    bodyR_val += (curR * volume) >> 8;
                    bodyG_val += (curG * volume) >> 8;
                    bodyB_val += (curB * volume) >> 8;
                }

                bodyA = max(alphaR, max(alphaG, alphaB));
                bodyA = (bodyA * 38) >> 8;
            }

            int rimA = 0;
            if (alphaG > 0 || alphaR > 0 || alphaB > 0) {
                int dzdx = ((int)pAlphaRow[i + 1] - (int)pAlphaRow[i - 1]);
                int dzdy = ((int)pRowNext[i] - (int)pRowPrev[i]);
                int mag = abs(dzdx) + abs(dzdy);

                if (mag > 0) {
                    int lighting = -(dzdx + dzdy);
                    rimA = (mag * 85) >> 8;
                    if (lighting > 0) {
                        rimA += lighting;
                    }
                }
                if (rimA > 255) rimA = 255;
                rimA = (rimA * rimA) >> 8;
            }

            int specR = 0;
            int specG = 0;
            int specB = 0;

            if (glow > 0) {
                specR = (curR * glow * 358) >> 16;
                specG = (curG * glow * 358) >> 16;
                specB = (curB * glow * 358) >> 16;
            }

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            int outR = bgR + specR;
            int outG = bgG + specG;
            int outB = bgB + specB;

            if (bodyA > 0) {
                int invA = 255 - bodyA;
                outR = ((outR * invA) + (bodyR_val * bodyA)) >> 8;
                outG = ((outG * invA) + (bodyG_val * bodyA)) >> 8;
                outB = ((outB * invA) + (bodyB_val * bodyA)) >> 8;
            }

            if (rimA > 0) {
                outR += rimA;
                outG += rimA;
                outB += rimA;
            }

            if (outR > 255) outR = 255;
            if (outG > 255) outG = 255;
            if (outB > 255) outB = 255;

            int glowLum = max(specR, max(specG, specB));
            glowLum = (glowLum * 200) >> 8;

            int finalA = max(bgA, max(bodyA, max(rimA, glowLum)));

            if (finalA > 255) finalA = 255;

            *pPixel = (finalA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    DrawingEffect_EndBufferUse();
}
