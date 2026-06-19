#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

static inline int FastIntDist(int dx, int dy) {
    int ax = abs(dx);
    int ay = abs(dy);
    int mx = (ax > ay) ? ax : ay;
    int mn = (ax > ay) ? ay : ax;
    return mx + (mn >> 2) + (mn >> 3);
}

static unsigned char g_specularLUT[256];
static BOOL g_specularLUTInitialized = FALSE;

typedef struct {
    short dR;
    short dG;
    short dB;
    unsigned char alpha;
} SlopeProps;

static SlopeProps g_slopeLUT[256];
static BOOL g_slopeLUTInitialized = FALSE;

static void InitSpecularLUT(void) {
    if (g_specularLUTInitialized) return;
    for (int i = 0; i < 256; i++) {
        float f = i / 255.0f;
        f = f * f * f * f;
        g_specularLUT[i] = (unsigned char)(f * 255.0f);
    }
    g_specularLUTInitialized = TRUE;
}

static void InitSlopeLUT(void) {
    if (g_slopeLUTInitialized) return;

    for (int i = 0; i < 256; i++) {
        int slope_int = i;
        int clearAmt = 0;
        int darkAmt = 0;

        if (slope_int < 102) {
            clearAmt = ((102 - slope_int) * 200) >> 8;
        } else {
            darkAmt = ((slope_int - 102) * 150) >> 8;
        }

        int fresnel = 0;
        if (slope_int > 25) {
            int f = ((slope_int - 25) * 120) / 256;
            if (f > 80) f = 80;
            fresnel = f;
        }

        int dispersion = (slope_int * 40) >> 8;

        g_slopeLUT[i].dR = (short)(clearAmt - darkAmt + fresnel + dispersion);
        g_slopeLUT[i].dG = (short)(clearAmt - darkAmt + fresnel);
        g_slopeLUT[i].dB = (short)(clearAmt - darkAmt + fresnel - dispersion);

        int alpha = 60 + ((slope_int * 210) >> 8);
        if (alpha > 255) alpha = 255;
        g_slopeLUT[i].alpha = (unsigned char)alpha;
    }
    g_slopeLUTInitialized = TRUE;
}

void RenderLiquidEffect(DWORD* pixels, int destWidth, int destHeight,
                       int x_pos, int y_pos,
                       const unsigned char* bitmap, int w, int h,
                       int r, int g, int b,
                       GlowColorCallback colorCb, void* userData,
                       int timeOffset) {
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
    if (firstI < 2) firstI = 2;
    if (lastI > gw - 2) lastI = gw - 2;
    if (firstJ < 2) firstJ = 2;
    if (lastJ > gh - 2) lastJ = gh - 2;
    if (firstI >= lastI || firstJ >= lastJ) {
        return;
    }

    if (!DrawingEffect_BeginBufferUse()) return;
    InitSpecularLUT();
    InitSlopeLUT();

    DrawingEffectBuffers buffers;
    if (!DrawingEffect_EnsureBuffers(neededSize, &buffers)) {
        DrawingEffect_EndBufferUse();
        return;
    }

    unsigned char* heightMap = buffers.buffer1;
    unsigned char* tempMap = buffers.buffer2;

    memset(heightMap, 0, (size_t)neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(heightMap + (j + padding) * gw + padding, bitmap + j * w, (size_t)w);
    }

    ApplyGaussianBlur(heightMap, heightMap, tempMap, gw, gh, 4);

#define FLOW_LUT_SIZE 2048
#define FLOW_LUT_MASK 2047
    static int flowLUT[FLOW_LUT_SIZE];
    static BOOL flowLUTInitialized = FALSE;

    if (!flowLUTInitialized) {
        for (int i = 0; i < FLOW_LUT_SIZE; i++) {
            float angle = (i * 6.28318f) / FLOW_LUT_SIZE;
            float val = sinf(angle) * 4.0f;
            flowLUT[i] = (int)(val);
        }
        flowLUTInitialized = TRUE;
    }

    int t_idx = (int)((double)timeOffset * 0.815) & FLOW_LUT_MASK;

    int t1 = t_idx;
    int t2 = (t_idx + 682) & FLOW_LUT_MASK;
    int t3 = (t_idx + 1365) & FLOW_LUT_MASK;

    for (int j = firstJ; j < lastJ; j++) {
        int screenY = (int)(startY + (long long)j);

        DWORD* pDestRow = pixels + (size_t)screenY * (size_t)destWidth;

        int yComp = (screenY * 222) >> 8;

        for (int i = firstI; i < lastI; i++) {
            int screenX = (int)(startX + (long long)i);

            int idx1 = (screenX * 2 + t1) & FLOW_LUT_MASK;

            int xComp2 = i >> 1;
            int idx2 = (xComp2 + yComp + t2) & FLOW_LUT_MASK;

            int idx3 = (-xComp2 + yComp + t3) & FLOW_LUT_MASK;

            int w1 = flowLUT[idx1];
            int w2 = flowLUT[idx2];
            int w3 = flowLUT[idx3];

            int warpX = (w1 + w2 + w3) >> 1;
            int idx1_swirl = (idx1 + 500) & FLOW_LUT_MASK;
            int warpY = (flowLUT[idx1_swirl] + w2 - w3) >> 1;

            int srcX = i + warpX;
            int srcY = j + warpY;

            if (srcX < 1) srcX = 1; else if (srcX >= gw - 1) srcX = gw - 2;
            if (srcY < 1) srcY = 1; else if (srcY >= gh - 1) srcY = gh - 2;

            int centerIdx = srcY * gw + srcX;
            int mass = heightMap[centerIdx];

            if (mass < 24) continue;

            int edgeAA = 256;
            if (mass < 56) {
                edgeAA = (mass - 24) << 3;
            }

            int hL = heightMap[centerIdx - 1];
            int hR = heightMap[centerIdx + 1];
            int hU = heightMap[centerIdx - gw];
            int hD = heightMap[centerIdx + gw];

            int dx = hR - hL;
            int dy = hD - hU;

            int len = FastIntDist(dx, dy);

            int slope_int = (len * 5) >> 1;
            if (slope_int > 255) slope_int = 255;

            int sumNormal = dx + dy;
            int specular = 0;

            if (sumNormal < 0) {
                int idx = -sumNormal;
                idx = (idx * 5) >> 1;
                if (idx > 255) idx = 255;

                specular = g_specularLUT[idx];
            }

            if (edgeAA < 256) {
                specular = (specular * edgeAA) >> 8;
            }

            int curR = r;
            int curG = g;
            int curB = b;

            if (colorCb) {
                int sampleX = (int)(startX + (long long)srcX);
                int sampleY = (int)(startY + (long long)srcY);
                colorCb(sampleX, sampleY, &curR, &curG, &curB, userData);
            }

            SlopeProps p = g_slopeLUT[slope_int];
            int fR = curR + p.dR;
            int fG = curG + p.dG;
            int fB = curB + p.dB;

            if (fR < 0) fR = 0; else if (fR > 255) fR = 255;
            if (fG < 0) fG = 0; else if (fG > 255) fG = 255;
            if (fB < 0) fB = 0; else if (fB > 255) fB = 255;

            int alpha = p.alpha;
            if (edgeAA < 256) {
                alpha = (alpha * edgeAA) >> 8;
            }

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            int invA = 255 - alpha;
            int finalR = (bgR * invA + fR * alpha) >> 8;
            int finalG = (bgG * invA + fG * alpha) >> 8;
            int finalB = (bgB * invA + fB * alpha) >> 8;

            finalR += specular;
            finalG += specular;
            finalB += specular;

            if (finalR > 255) finalR = 255;
            if (finalG > 255) finalG = 255;
            if (finalB > 255) finalB = 255;

            int finalA = (bgA > alpha) ? bgA : alpha;
            if (specular > finalA) finalA = specular;
            if (finalA > 255) finalA = 255;

            *pPixel = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }

    DrawingEffect_EndBufferUse();
}
