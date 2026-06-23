#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

#define AQUA_GRADIENT_LUT_SIZE 4096
#define AQUA_GRADIENT_LUT_MASK (AQUA_GRADIENT_LUT_SIZE - 1)
#define AQUA_RIPPLE_FLOW_MS 14000U
#define AQUA_RIPPLE_FLOW_CYCLES 7
#define AQUA_NOISE_FIXED_ONE 256
#define AQUA_NOISE_SAMPLE_STEP 2
#define AQUA_MIN_GRADIENT_PERIOD 640
#define AQUA_MAX_GRADIENT_PERIOD 1600

static DWORD g_aquaGradientLUT[AQUA_GRADIENT_LUT_SIZE];
static int g_aquaGradientIndexLUT[AQUA_MAX_GRADIENT_PERIOD];
static int g_aquaGradientIndexPeriod = 0;
static BOOL g_aquaTablesInitialized = FALSE;

static inline int ClampByteInt(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

static inline int ClampInt(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline int LerpByteInt(int a, int b, int t4096) {
    return a + (((b - a) * t4096) >> 12);
}

static inline int LerpByte256(int a, int b, int t256) {
    return a + (((b - a) * t256) >> 8);
}

static inline int SmoothStepByte(int t) {
    return (t * t * (768 - (t << 1))) >> 16;
}

static void InitAquaTables(void) {
    if (g_aquaTablesInitialized) return;

    for (int i = 0; i < AQUA_GRADIENT_LUT_SIZE; i++) {
        int r1, g1, b1, r2, g2, b2, start, end;
        if (i < 1024) {
            start = 0; end = 1024;
            r1 = 255; g1 = 255; b1 = 255;
            r2 = 227; g2 = 250; b2 = 255;
        } else if (i < 2048) {
            start = 1024; end = 2048;
            r1 = 227; g1 = 250; b1 = 255;
            r2 = 139; g2 = 240; b2 = 255;
        } else if (i < 3072) {
            start = 2048; end = 3072;
            r1 = 139; g1 = 240; b1 = 255;
            r2 = 255; g2 = 255; b2 = 255;
        } else {
            start = 3072; end = 4095;
            r1 = 255; g1 = 255; b1 = 255;
            r2 = 174; g2 = 242; b2 = 255;
        }

        int t4096 = ((i - start) << 12) / (end - start);
        int r = LerpByteInt(r1, r2, t4096);
        int g = LerpByteInt(g1, g2, t4096);
        int b = LerpByteInt(b1, b2, t4096);
        g_aquaGradientLUT[i] = ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
    }

    g_aquaTablesInitialized = TRUE;
}

static const int* GetAquaGradientIndexLUT(int period) {
    period = ClampInt(period, AQUA_MIN_GRADIENT_PERIOD, AQUA_MAX_GRADIENT_PERIOD);
    if (g_aquaGradientIndexPeriod != period) {
        for (int i = 0; i < period; i++) {
            g_aquaGradientIndexLUT[i] =
                (int)(((long long)i * AQUA_GRADIENT_LUT_SIZE) / period) &
                AQUA_GRADIENT_LUT_MASK;
        }
        g_aquaGradientIndexPeriod = period;
    }
    return g_aquaGradientIndexLUT;
}

static inline unsigned int AquaHashNoise(int x, int y, unsigned int seed) {
    unsigned int h = (unsigned int)x * 374761393u;
    h += (unsigned int)y * 668265263u;
    h += seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static inline int FloorFixed8(int value) {
    if (value >= 0) {
        return value >> 8;
    }
    return -(((-value) + 255) >> 8);
}

static inline int AquaValueNoiseQ8(int xQ8, int yQ8, unsigned int seed) {
    int xi = FloorFixed8(xQ8);
    int yi = FloorFixed8(yQ8);
    int tx = xQ8 - (xi << 8);
    int ty = yQ8 - (yi << 8);
    int sx = SmoothStepByte(tx);
    int sy = SmoothStepByte(ty);

    int n00 = (int)(AquaHashNoise(xi,     yi,     seed) & 0xFFu);
    int n10 = (int)(AquaHashNoise(xi + 1, yi,     seed) & 0xFFu);
    int n01 = (int)(AquaHashNoise(xi,     yi + 1, seed) & 0xFFu);
    int n11 = (int)(AquaHashNoise(xi + 1, yi + 1, seed) & 0xFFu);

    int nx0 = LerpByte256(n00, n10, sx);
    int nx1 = LerpByte256(n01, n11, sx);
    return LerpByte256(nx0, nx1, sy);
}

static inline int AquaFractalNoise(int screenX, int screenY,
                                   int freqXPpm, int freqYPpm,
                                   int flowQ8, unsigned int seed) {
    long long baseX = ((long long)screenX * (long long)freqXPpm * AQUA_NOISE_FIXED_ONE) / 1000000LL;
    long long baseY = ((long long)screenY * (long long)freqYPpm * AQUA_NOISE_FIXED_ONE) / 1000000LL;
    baseY -= (long long)flowQ8;

    return AquaValueNoiseQ8((int)baseX, (int)baseY, seed);
}

static inline int AquaNoiseAt(const unsigned char* noiseMap, int width, int height, int x, int y) {
    if (x < 0) x = 0; else if (x >= width) x = width - 1;
    if (y < 0) y = 0; else if (y >= height) y = height - 1;
    return noiseMap[(size_t)y * (size_t)width + (size_t)x];
}

static inline int AquaNoiseAtUnchecked(const unsigned char* noiseMap, int width, int x, int y) {
    return noiseMap[(size_t)y * (size_t)width + (size_t)x];
}

static inline int SampleAlphaBilinear(const unsigned char* alphaMap,
                                      int width, int height,
                                      int xQ8, int yQ8) {
    int x = FloorFixed8(xQ8);
    int y = FloorFixed8(yQ8);
    if (x < 0 || y < 0 || x >= width - 1 || y >= height - 1) {
        return 0;
    }

    int tx = xQ8 - (x << 8);
    int ty = yQ8 - (y << 8);
    const unsigned char* row0 = alphaMap + (size_t)y * (size_t)width;
    const unsigned char* row1 = row0 + width;

    int top = LerpByte256(row0[x], row0[x + 1], tx);
    int bottom = LerpByte256(row1[x], row1[x + 1], tx);
    return LerpByte256(top, bottom, ty);
}

static inline int SampleAlphaNearest(const unsigned char* alphaMap,
                                     int width, int height,
                                     int xQ8, int yQ8) {
    int x = (xQ8 + 128) >> 8;
    int y = (yQ8 + 128) >> 8;
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 0;
    }
    return alphaMap[(size_t)y * (size_t)width + (size_t)x];
}

static inline int AquaErodedAlpha(int alpha, int poreNoise, int displacementNoise) {
    if (alpha <= 0) return 0;

    int edgeFactor = (alpha < 250) ? (250 - alpha) : 0;
    int poreAmount = ClampByteInt(poreNoise - 92);
    int displacementAmount = ClampByteInt(displacementNoise - 104);
    int bite = (poreAmount * (18 + edgeFactor) +
                displacementAmount * (edgeFactor >> 1)) >> 8;

    return ClampByteInt(alpha - bite);
}

static void BuildAquaNoiseMap(unsigned char* noiseMap,
                              unsigned char* coarseMap,
                              int gw, int gh,
                              int noiseFirstI, int noiseLastI,
                              int noiseFirstJ, int noiseLastJ,
                              long long startX, long long startY,
                              int freqXPpm, int freqYPpm,
                              int flowQ8,
                              int sampleStep) {
    const int step = ClampInt(sampleStep, AQUA_NOISE_SAMPLE_STEP, 4);
    int coarseFirstI = ClampInt(noiseFirstI / step - 1, 0, (gw + step - 1) / step);
    int coarseLastI = ClampInt((noiseLastI + step - 1) / step + 1, 0, (gw + step - 1) / step);
    int coarseFirstJ = ClampInt(noiseFirstJ / step - 1, 0, (gh + step - 1) / step);
    int coarseLastJ = ClampInt((noiseLastJ + step - 1) / step + 1, 0, (gh + step - 1) / step);
    int coarseW = coarseLastI - coarseFirstI;
    int coarseH = coarseLastJ - coarseFirstJ;

    if (coarseW <= 1 || coarseH <= 1) {
        for (int j = noiseFirstJ; j < noiseLastJ; j++) {
            int screenY = (int)(startY + (long long)j);
            unsigned char* noiseRow = noiseMap + (size_t)j * (size_t)gw;

            for (int i = noiseFirstI; i < noiseLastI; i++) {
                int screenX = (int)(startX + (long long)i);
                noiseRow[i] = (unsigned char)AquaFractalNoise(screenX, screenY,
                                                              freqXPpm, freqYPpm,
                                                              flowQ8, 11u);
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
            coarseRow[cx] = (unsigned char)AquaFractalNoise(screenX, screenY,
                                                            freqXPpm, freqYPpm,
                                                            flowQ8, 11u);
        }
    }

    for (int j = noiseFirstJ; j < noiseLastJ; j++) {
        int coarseY = j / step;
        int y0 = ClampInt(coarseY - coarseFirstJ, 0, coarseH - 1);
        int y1 = ClampInt(y0 + 1, 0, coarseH - 1);
        int ty = ((j - coarseY * step) << 8) / step;
        unsigned char* noiseRow = noiseMap + (size_t)j * (size_t)gw;
        const unsigned char* coarseRow0 = coarseMap + (size_t)y0 * (size_t)coarseW;
        const unsigned char* coarseRow1 = coarseMap + (size_t)y1 * (size_t)coarseW;

        for (int i = noiseFirstI; i < noiseLastI; i++) {
            int coarseX = i / step;
            int x0 = ClampInt(coarseX - coarseFirstI, 0, coarseW - 1);
            int x1 = ClampInt(x0 + 1, 0, coarseW - 1);
            int tx = ((i - coarseX * step) << 8) / step;

            int top = LerpByte256(coarseRow0[x0], coarseRow0[x1], tx);
            int bottom = LerpByte256(coarseRow1[x0], coarseRow1[x1], tx);
            noiseRow[i] = (unsigned char)LerpByte256(top, bottom, ty);
        }
    }
}

static inline void GetAquaPixelColor(int screenX, int screenY,
                                     int aquaPeriod,
                                     const int* aquaIndexLUT,
                                     int baseR, int baseG, int baseB,
                                     GlowColorCallback colorCb, void* userData,
                                     int* outR, int* outG, int* outB) {
    int finalR = baseR;
    int finalG = baseG;
    int finalB = baseB;

    if (colorCb) {
        colorCb(screenX, screenY, &finalR, &finalG, &finalB, userData);
    }

    int coord = screenX + (screenY >> 2);
    int coordMod = coord % aquaPeriod;
    if (coordMod < 0) coordMod += aquaPeriod;
    int aquaIndex = aquaIndexLUT ? aquaIndexLUT[coordMod] : 0;
    DWORD aquaColor = g_aquaGradientLUT[aquaIndex & AQUA_GRADIENT_LUT_MASK];
    int aquaR = (aquaColor >> 16) & 0xFF;
    int aquaG = (aquaColor >> 8) & 0xFF;
    int aquaB = aquaColor & 0xFF;

    finalR = ClampByteInt(finalR);
    finalG = ClampByteInt(finalG);
    finalB = ClampByteInt(finalB);

    if (outR) *outR = ((finalR * 5) + (aquaR * 3)) >> 3;
    if (outG) *outG = ((finalG * 5) + (aquaG * 3)) >> 3;
    if (outB) *outB = ((finalB * 5) + (aquaB * 3)) >> 3;
}

static inline int AquaGlowChannel(int channel) {
    return ClampByteInt(channel + ((255 - channel) >> 2));
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

    *pixel = ((DWORD)ClampByteInt(outA) << 24) |
             ((DWORD)ClampByteInt(outR) << 16) |
             ((DWORD)ClampByteInt(outG) << 8) |
             (DWORD)ClampByteInt(outB);
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

    *pixel = ((DWORD)ClampByteInt(outA) << 24) |
             ((DWORD)ClampByteInt(outR) << 16) |
             ((DWORD)ClampByteInt(outG) << 8) |
             (DWORD)ClampByteInt(outB);
}

void RenderAquaEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      const unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData,
                      int timeOffset) {
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0) return;

    BOOL useLargeGlyphFastPath = (h >= 120);
    int displacementScale = ClampInt((h + 2) / 8, 5, (h >= 160) ? 14 : 22);
    int shadowOffset = ClampInt((h + 9) / 18, 3, 8);
    int glowBlur = ClampInt((h + 5) / 14, useLargeGlyphFastPath ? 3 : 4, (h >= 160) ? 5 : 14);
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
    InitAquaTables();
    int aquaPeriod = ClampInt(destWidth * 3, AQUA_MIN_GRADIENT_PERIOD, AQUA_MAX_GRADIENT_PERIOD);
    const int* aquaIndexLUT = GetAquaGradientIndexLUT(aquaPeriod);

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
    int flowQ8 = (int)(((unsigned long long)(timeMs % AQUA_RIPPLE_FLOW_MS) *
                        AQUA_NOISE_FIXED_ONE * AQUA_RIPPLE_FLOW_CYCLES) /
                       AQUA_RIPPLE_FLOW_MS);

    int displaceFirstI = ClampInt(padding - displacementScale - 2, 1, gw - 1);
    int displaceLastI = ClampInt(padding + w + displacementScale + 2, 1, gw - 1);
    int displaceFirstJ = ClampInt(padding - displacementScale - 2, 1, gh - 1);
    int displaceLastJ = ClampInt(padding + h + displacementScale + 2, 1, gh - 1);
    int noiseFirstI = ClampInt(displaceFirstI - 18, 0, gw);
    int noiseLastI = ClampInt(displaceLastI + 14, 0, gw);
    int noiseFirstJ = ClampInt(displaceFirstJ - 12, 0, gh);
    int noiseLastJ = ClampInt(displaceLastJ + 12, 0, gh);

    BuildAquaNoiseMap(noiseMap, displacedMap, gw, gh,
                      noiseFirstI, noiseLastI,
                      noiseFirstJ, noiseLastJ,
                      startX, startY,
                      freqXPpm, freqYPpm,
                      flowQ8,
                      noiseSampleStep);
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
            int displaced = useLargeGlyphFastPath
                ? SampleAlphaNearest(alphaMap, gw, gh, srcXQ8, srcYQ8)
                : SampleAlphaBilinear(alphaMap, gw, gh, srcXQ8, srcYQ8);
            displacedRow[i] = (unsigned char)AquaErodedAlpha(displaced, poreNoise, displacementNoise);
        }
    }

    ApplyGaussianBlur(displacedMap, glowMap, alphaMap, gw, gh, glowBlur);

    /* CSS reference: drop-shadow(0 8px 20px rgba(0, 206, 235, 0.35)). */
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
            GetAquaPixelColor(screenX, screenY, aquaPeriod, aquaIndexLUT,
                              r, g, b, colorCb, userData, &glowR, &glowG, &glowB);
            AddPremultipliedGlow(destRow + screenX,
                                 AquaGlowChannel(glowR),
                                 AquaGlowChannel(glowG),
                                 AquaGlowChannel(glowB),
                                 alpha);
        }
    }

    /* User color remains dominant; the static Aqua gradient adds water highlights. */
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
            GetAquaPixelColor(screenX, screenY, aquaPeriod, aquaIndexLUT,
                              r, g, b, colorCb, userData, &fillR, &fillG, &fillB);

            BlendPremultipliedBody(destRow + screenX,
                                   fillR,
                                   fillG,
                                   fillB,
                                   mass);
        }
    }

    DrawingEffect_EndBufferUse();
}
