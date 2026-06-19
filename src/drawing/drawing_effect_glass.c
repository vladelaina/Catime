#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

#define DIV255(x) (((x) + 1 + ((x) >> 8)) >> 8)

void RenderGlassEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      const unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;

    int padding = 4;
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
    unsigned char* shadowMap = buffers.buffer2;
    unsigned char* tempBuffer = buffers.buffer3;

    memset(alphaMap, 0, (size_t)neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, (size_t)w);
    }

    int shadowBlur = 4;
    ApplyGaussianBlur(alphaMap, shadowMap, tempBuffer, gw, gh, shadowBlur);

    int shadowOffset = 3;

    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);

        DWORD* pDestRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* pAlphaRow = alphaMap + j * gw;

        const unsigned char* pShadowRow = NULL;
        int sY = j - shadowOffset;
        if (sY >= 0 && sY < gh) pShadowRow = shadowMap + sY * gw;

        int bevelOffset = 2;
        const unsigned char* pBevelRowUL = (j >= bevelOffset) ? alphaMap + (j - bevelOffset) * gw : NULL;
        const unsigned char* pBevelRowDR = (j < gh - bevelOffset) ? alphaMap + (j + bevelOffset) * gw : NULL;

        int sheen = 0;
        if (h > 0) {
            int localY = j - padding;
            if (localY < 0) localY = 0;
            else if (localY > h) localY = h;
            sheen = (255 - (localY * 255) / h) >> 3;
        }

        for (int i = firstI; i < lastI; i++) {
            int screenX = (int)(startX + (long long)i);

            unsigned char srcAlpha = pAlphaRow[i];

            int shadowVal = 0;
            int sX = i - shadowOffset;
            if (pShadowRow && sX >= 0 && sX < gw) shadowVal = pShadowRow[sX];

            if (srcAlpha == 0 && shadowVal == 0) continue;

            int bodyR = r;
            int bodyG = g;
            int bodyB = b;
            if (colorCb) colorCb(screenX, screenY, &bodyR, &bodyG, &bodyB, userData);

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int finalA = (bgPixel >> 24) & 0xFF;
            int finalR = (bgPixel >> 16) & 0xFF;
            int finalG = (bgPixel >> 8) & 0xFF;
            int finalB = bgPixel & 0xFF;

            if (shadowVal > 0 && srcAlpha < 255) {
                int shadowColor = 0;
                int shadowOpacity = (shadowVal * 100) / 255;
                int invShadow = 255 - shadowOpacity;
                finalR = (finalR * invShadow + shadowColor * shadowOpacity) / 255;
                finalG = (finalG * invShadow + shadowColor * shadowOpacity) / 255;
                finalB = (finalB * invShadow + shadowColor * shadowOpacity) / 255;
                finalA = max(finalA, shadowOpacity);
            }

            if (srcAlpha > 0) {
                int valUL = 0;
                int valDR = 0;
                if (pBevelRowUL && i >= bevelOffset) valUL = pBevelRowUL[i - bevelOffset];
                if (pBevelRowDR && i < gw - bevelOffset) valDR = pBevelRowDR[i + bevelOffset];

                int highlight = (int)srcAlpha - (int)valUL;
                int refraction = (int)srcAlpha - (int)valDR;

                int bodyAlpha = 15 + sheen;
                int invBodyAlpha = 255 - bodyAlpha;

                finalR = DIV255(finalR * invBodyAlpha + bodyR * bodyAlpha);
                finalG = DIV255(finalG * invBodyAlpha + bodyG * bodyAlpha);
                finalB = DIV255(finalB * invBodyAlpha + bodyB * bodyAlpha);
                finalA = max(finalA, bodyAlpha);

                if (refraction > 50) {
                    int rimA = refraction >> 1;
                    int invRimA = 255 - rimA;
                    finalR = DIV255(finalR * invRimA + bodyR * rimA);
                    finalG = DIV255(finalG * invRimA + bodyG * rimA);
                    finalB = DIV255(finalB * invRimA + bodyB * rimA);
                    finalA = max(finalA, rimA);
                }

                if (highlight > 40) {
                    int spec = (highlight * highlight * highlight) / 25500;
                    if (spec > 255) spec = 255;
                    if (highlight > 200) spec += 50;
                    if (spec > 255) spec = 255;

                    if (spec > 0) {
                        int fireR = spec;
                        int fireG = spec;
                        int fireB = spec + (spec / 6);
                        if (fireB > 255) fireB = 255;

                        finalR += fireR;
                        finalG += fireG;
                        finalB += fireB;
                        finalA = max(finalA, spec);
                    }
                }
            }

            if (finalR > 255) finalR = 255;
            if (finalG > 255) finalG = 255;
            if (finalB > 255) finalB = 255;
            if (finalA > 255) finalA = 255;

            *pPixel = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }

    DrawingEffect_EndBufferUse();
}
