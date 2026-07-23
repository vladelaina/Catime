#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"
#include "drawing/drawing_effect_aqua_internal.h"
#define AQUA_RIPPLE_FLOW_MS 14000U
#define AQUA_RIPPLE_FLOW_CYCLES 7
#define AQUA_NOISE_FIXED_ONE 256
#define AQUA_NOISE_SAMPLE_STEP 2
static void BuildAquaNoiseMap(unsigned char* noiseMap, unsigned char* coarseMap, int gw, int gh, int noiseFirstI, int noiseLastI, int noiseFirstJ, int noiseLastJ, long long startX, long long startY, int freqXPpm, int freqYPpm, int flowQ8, int sampleStep) {
    const int step = AquaClampInt(sampleStep, AQUA_NOISE_SAMPLE_STEP, 4);
    int coarseFirstI = AquaClampInt(noiseFirstI / step - 1, 0, (gw + step - 1) / step);
    int coarseLastI = AquaClampInt((noiseLastI + step - 1) / step + 1, 0, (gw + step - 1) / step);
    int coarseFirstJ = AquaClampInt(noiseFirstJ / step - 1, 0, (gh + step - 1) / step);
    int coarseLastJ = AquaClampInt((noiseLastJ + step - 1) / step + 1, 0, (gh + step - 1) / step);
    int coarseW = coarseLastI - coarseFirstI;
    int coarseH = coarseLastJ - coarseFirstJ;
    if (coarseW <= 1 || coarseH <= 1) {
        for (int j = noiseFirstJ; j < noiseLastJ; j++) {
            int screenY = (int)(startY + (long long)j);
            unsigned char* noiseRow = noiseMap + (size_t)j * (size_t)gw;
            for (int i = noiseFirstI; i < noiseLastI; i++) {
                int screenX = (int)(startX + (long long)i);
                noiseRow[i] = (unsigned char)AquaFractalNoise(screenX, screenY, freqXPpm, freqYPpm, flowQ8, 11u);
            }
        }
        return;
    }
    for (int cy = 0; cy < coarseH; cy++) {
        int fullY = (coarseFirstJ + cy) * step;
        int screenY = (int)(startY + (long long)fullY);
        unsigned char* coarseRow = coarseMap + (size_t)cy * (size_t)coarseW;
        for (int cx = 0; cx < coarseW; cx++) {
            int fullX = (coarseFirstI + cx) * step;
            int screenX = (int)(startX + (long long)fullX);
            coarseRow[cx] = (unsigned char)AquaFractalNoise(screenX, screenY, freqXPpm, freqYPpm, flowQ8, 11u);
        }
    }
    for (int j = noiseFirstJ; j < noiseLastJ; j++) {
        int coarseY = j / step;
        int y0 = AquaClampInt(coarseY - coarseFirstJ, 0, coarseH - 1);
        int y1 = AquaClampInt(y0 + 1, 0, coarseH - 1);
        int ty = ((j - coarseY * step) << 8) / step;
        unsigned char* noiseRow = noiseMap + (size_t)j * (size_t)gw;
        const unsigned char* coarseRow0 = coarseMap + (size_t)y0 * (size_t)coarseW;
        const unsigned char* coarseRow1 = coarseMap + (size_t)y1 * (size_t)coarseW;
        for (int i = noiseFirstI; i < noiseLastI; i++) {
            int coarseX = i / step;
            int x0 = AquaClampInt(coarseX - coarseFirstI, 0, coarseW - 1);
            int x1 = AquaClampInt(x0 + 1, 0, coarseW - 1);
            int tx = ((i - coarseX * step) << 8) / step;
            int top = AquaLerpByte256(coarseRow0[x0], coarseRow0[x1], tx);
            int bottom = AquaLerpByte256(coarseRow1[x0], coarseRow1[x1], tx);
            noiseRow[i] = (unsigned char)AquaLerpByte256(top, bottom, ty);
        }
    }
}
static inline void GetAquaPixelColor(int screenX, int screenY, int baseR, int baseG, int baseB, GlowColorCallback colorCb, void* userData, int* outR, int* outG, int* outB) {
    int finalR = baseR;
    int finalG = baseG;
    int finalB = baseB;
    if (colorCb) {
        colorCb(screenX, screenY, &finalR, &finalG, &finalB, userData);
    }
    finalR = AquaClampByte(finalR);
    finalG = AquaClampByte(finalG);
    finalB = AquaClampByte(finalB);
    if (outR) *outR = finalR;
    if (outG) *outG = finalG;
    if (outB) *outB = finalB;
}
static inline void AddPremultipliedGlow(DWORD* pixel, int r, int g, int b, int alpha) {
    if (!pixel || alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    DWORD bgPixel = *pixel;
    int bgA = (bgPixel >> 24) & 0xFF;
    int bgR = (bgPixel >> 16) & 0xFF;
    int bgG = (bgPixel >> 8) & 0xFF;
    int bgB = bgPixel & 0xFF;
    int outR = bgR + ((r * alpha) >> 8);
    int outG = bgG + ((g * alpha) >> 8);
    int outB = bgB + ((b * alpha) >> 8);
    int outA = bgA > alpha ? bgA : alpha;
    *pixel = ((DWORD)AquaClampByte(outA) << 24) |
             ((DWORD)AquaClampByte(outR) << 16) |
             ((DWORD)AquaClampByte(outG) << 8) |
             (DWORD)AquaClampByte(outB);
}
static inline void BlendPremultipliedBody(DWORD* pixel, int r, int g, int b, int alpha) {
    if (!pixel || alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    DWORD bgPixel = *pixel;
    int bgA = (bgPixel >> 24) & 0xFF;
    int bgR = (bgPixel >> 16) & 0xFF;
    int bgG = (bgPixel >> 8) & 0xFF;
    int bgB = bgPixel & 0xFF;
    int invA = 255 - alpha;
    int outA = alpha + ((bgA * invA) >> 8);
    int outR = ((r * alpha) >> 8) + ((bgR * invA) >> 8);
    int outG = ((g * alpha) >> 8) + ((bgG * invA) >> 8);
    int outB = ((b * alpha) >> 8) + ((bgB * invA) >> 8);
    *pixel = ((DWORD)AquaClampByte(outA) << 24) |
             ((DWORD)AquaClampByte(outR) << 16) |
             ((DWORD)AquaClampByte(outG) << 8) |
             (DWORD)AquaClampByte(outB);
}
void RenderAquaEffect(DWORD* pixels, int destWidth, int destHeight, int x_pos, int y_pos, const unsigned char* bitmap, int w, int h, int r, int g, int b, GlowColorCallback colorCb, void* userData, int timeOffset) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;
    int displacementScale = AquaClampInt((h + 2) / 8, 5, (h >= 160) ? 14 : 22);
    int shadowOffset = AquaClampInt((h + 9) / 18, 3, 8);
    int glowBlur = AquaClampInt((h + 5) / 14, (h >= 120) ? 3 : 4, (h >= 160) ? 5 : 14);
    int padding = displacementScale + shadowOffset + glowBlur + 4;
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
    if (!DrawingEffect_CalculateVisibleSpan(startX, gw, destWidth, &firstI, &lastI) || !DrawingEffect_CalculateVisibleSpan(startY, gh, destHeight, &firstJ, &lastJ)) {
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
    unsigned char* noiseMap = buffers.buffer2;
    unsigned char* displacedMap = buffers.buffer3;
    memset(alphaMap, 0, (size_t)neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + (size_t)j * (size_t)w, (size_t)w);
    }
    unsigned int timeMs = (unsigned int)timeOffset;
    int freqXPpm = 8000;
    int freqYPpm = 70000;
    int noiseSampleStep = (h >= 160) ? 4 : ((h >= 80) ? 3 : AQUA_NOISE_SAMPLE_STEP);
    int flowQ8 = (int)(((unsigned long long)(timeMs % AQUA_RIPPLE_FLOW_MS) * AQUA_NOISE_FIXED_ONE * AQUA_RIPPLE_FLOW_CYCLES) / AQUA_RIPPLE_FLOW_MS);
    int displaceFirstI = AquaClampInt(padding - displacementScale - 2, 1, gw - 1);
    int displaceLastI = AquaClampInt(padding + w + displacementScale + 2, 1, gw - 1);
    int displaceFirstJ = AquaClampInt(padding - displacementScale - 2, 1, gh - 1);
    int displaceLastJ = AquaClampInt(padding + h + displacementScale + 2, 1, gh - 1);
    int noiseFirstI = AquaClampInt(displaceFirstI - 18, 0, gw);
    int noiseLastI = AquaClampInt(displaceLastI + 14, 0, gw);
    int noiseFirstJ = AquaClampInt(displaceFirstJ - 12, 0, gh);
    int noiseLastJ = AquaClampInt(displaceLastJ + 12, 0, gh);
    BuildAquaNoiseMap(noiseMap, displacedMap, gw, gh, noiseFirstI, noiseLastI, noiseFirstJ, noiseLastJ, startX, startY, freqXPpm, freqYPpm, flowQ8, noiseSampleStep);
    memset(displacedMap, 0, (size_t)neededSize);
    for (int j = displaceFirstJ; j < displaceLastJ; j++) {
        unsigned char* displacedRow = displacedMap + (size_t)j * (size_t)gw;
        const unsigned char* noiseRow = noiseMap + (size_t)j * (size_t)gw;
        BOOL canReadLowerNoiseRows = (j + 11) < gh;
        for (int i = displaceFirstI; i < displaceLastI; i++) {
            int xNoise = noiseRow[i];
            BOOL canReadNoiseColumns = i >= 17 && (i + 13) < gw;
            int yNoise = (canReadNoiseColumns && canReadLowerNoiseRows)
                ? AquaNoiseAtUnchecked(noiseMap, gw, i + 13, j + 7)
                : AquaNoiseAt(noiseMap, gw, gh, i + 13, j + 7);
            int poreNoise = (canReadNoiseColumns && canReadLowerNoiseRows)
                ? AquaNoiseAtUnchecked(noiseMap, gw, i - 17, j + 11)
                : AquaNoiseAt(noiseMap, gw, gh, i - 17, j + 11);
            int displacementNoise = (xNoise + yNoise) >> 1;
            int waveXQ8 = (xNoise - 128) * displacementScale;
            int waveYQ8 = (yNoise - 128) * displacementScale;
            int srcXQ8 = (i << 8) + waveXQ8;
            int srcYQ8 = (j << 8) + waveYQ8;
            int displaced = AquaSampleAlphaBilinear(alphaMap, gw, gh, srcXQ8, srcYQ8);
            displacedRow[i] = (unsigned char)AquaErodedAlpha(displaced, poreNoise, displacementNoise);
        }
    }
    ApplyGaussianBlur(displacedMap, glowMap, alphaMap, gw, gh, glowBlur);
    for (int j = firstJ; j < lastJ; j++) {
        int shadowJ = j - shadowOffset;
        if (shadowJ < 0 || shadowJ >= gh) continue;
        int screenY = (int)(startY + (long long)j);
        DWORD* destRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* glowRow = glowMap + (size_t)shadowJ * (size_t)gw;
        for (int i = firstI; i < lastI; i++) {
            int glow = glowRow[i];
            if (glow <= 2) continue;
            int alpha = (glow * 90) >> 8;
            if (alpha <= 0) continue;
            int screenX = (int)(startX + (long long)i);
            int glowR = r;
            int glowG = g;
            int glowB = b;
            GetAquaPixelColor(screenX, screenY, r, g, b, colorCb, userData, &glowR, &glowG, &glowB);
            AddPremultipliedGlow(destRow + screenX, glowR, glowG, glowB, alpha);
        }
    }
    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);
        DWORD* destRow = pixels + (size_t)screenY * (size_t)destWidth;
        const unsigned char* bodyRow = displacedMap + (size_t)j * (size_t)gw;
        for (int i = firstI; i < lastI; i++) {
            int mass = bodyRow[i];
            if (mass <= 0) continue;
            int screenX = (int)(startX + (long long)i);
            int fillR = r;
            int fillG = g;
            int fillB = b;
            GetAquaPixelColor(screenX, screenY, r, g, b, colorCb, userData, &fillR, &fillG, &fillB);
            BlendPremultipliedBody(destRow + screenX, fillR, fillG, fillB, mass);
        }
    }
    DrawingEffect_EndBufferUse();
}
