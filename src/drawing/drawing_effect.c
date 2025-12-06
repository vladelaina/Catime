#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include "drawing/drawing_effect.h"

/* Static buffers for effect processing to avoid repeated mallocs */
static unsigned char* g_effectBuffer1 = NULL;
static unsigned char* g_effectBuffer2 = NULL;
static unsigned char* g_effectBuffer3 = NULL;
static int g_effectBufferSize = 0;

/**
 * @brief Apply Gaussian blur approximation to a single-channel bitmap
 * Optimized box blur implementation (3 passes approx Gaussian)
 */
void ApplyGaussianBlur(unsigned char* src, unsigned char* dest, unsigned char* tempBuffer, int w, int h, int radius) {
    if (radius < 1) {
        memcpy(dest, src, w * h);
        return;
    }

    /* Use a simplified box blur for performance */
    /* Horizontal Pass */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0;
            int count = 0;
            for (int k = -radius; k <= radius; k++) {
                int px = x + k;
                if (px >= 0 && px < w) {
                    sum += src[y * w + px];
                    count++;
                }
            }
            tempBuffer[y * w + x] = sum / count;
        }
    }

    /* Vertical Pass */
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int sum = 0;
            int count = 0;
            for (int k = -radius; k <= radius; k++) {
                int py = y + k;
                if (py >= 0 && py < h) {
                    sum += tempBuffer[py * w + x];
                    count++;
                }
            }
            dest[y * w + x] = sum / count;
        }
    }
}

/**
 * @brief Render a glow effect around a bitmap using Gaussian blur
 */
void RenderGlowEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    /* 1. Dynamic Buffer Allocation */
    int padding = 12;
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    int neededSize = gw * gh;

    if (neededSize > g_effectBufferSize) {
        int newSize = neededSize * 2;
        if (g_effectBuffer1) free(g_effectBuffer1);
        if (g_effectBuffer2) free(g_effectBuffer2);
        if (g_effectBuffer3) free(g_effectBuffer3);

        g_effectBuffer1 = (unsigned char*)malloc(newSize);
        g_effectBuffer2 = (unsigned char*)malloc(newSize);
        g_effectBuffer3 = (unsigned char*)malloc(newSize);
        
        if (!g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
            /* Cleanup and fail gracefully */
            if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
            if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
            if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
            g_effectBufferSize = 0;
            return;
        }
        g_effectBufferSize = newSize;
    }

    unsigned char* alphaMap = g_effectBuffer1;
    unsigned char* glowMap = g_effectBuffer2;
    unsigned char* tempBuffer = g_effectBuffer3;

    /* 2. Prepare Alpha Map */
    memset(alphaMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }

    /* 3. Generate Glow Map */
    ApplyGaussianBlur(alphaMap, glowMap, tempBuffer, gw, gh, 4);

    /* 4. Render to Destination */
    int startX = x_pos - padding;
    int startY = y_pos - padding;

    for (int j = 0; j < gh; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        unsigned char* pGlowRow = glowMap + j * gw;

        for (int i = 0; i < gw; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;

            int alpha = pGlowRow[i];
            if (alpha == 0) continue;

            int finalR = r, finalG = g, finalB = b;
            if (colorCb) {
                colorCb(screenX, screenY, &finalR, &finalG, &finalB, userData);
            }

            /* Additive blending for glow */
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

            /* Preserve original alpha or maximize it */
            int bgA = (bgPixel >> 24) & 0xFF;
            int outA = max(bgA, alpha);

            *pPixel = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }
}

/**
 * @brief Fast divide by 255 approximation
 */
#define DIV255(x) (((x) + 1 + ((x) >> 8)) >> 8)

/**
 * @brief Render glass/liquid crystal effect
 */
void RenderGlassEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    /* Dynamic Buffer Allocation */
    int padding = 4; /* Small padding for bevel */
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    int neededSize = gw * gh;

    if (neededSize > g_effectBufferSize) {
        int newSize = neededSize * 2;
        if (g_effectBuffer1) free(g_effectBuffer1);
        if (g_effectBuffer2) free(g_effectBuffer2);
        if (g_effectBuffer3) free(g_effectBuffer3);
        
        g_effectBuffer1 = (unsigned char*)malloc(newSize);
        g_effectBuffer2 = (unsigned char*)malloc(newSize);
        g_effectBuffer3 = (unsigned char*)malloc(newSize);
        
        if (!g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
            if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
            if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
            if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
            g_effectBufferSize = 0;
            return;
        }
        g_effectBufferSize = newSize;
    }

    unsigned char* alphaMap = g_effectBuffer1;
    unsigned char* shadowMap = g_effectBuffer2;
    unsigned char* tempBuffer = g_effectBuffer3;

    /* Prepare Alpha Map */
    memset(alphaMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }

    /* Create Soft Shadow Map */
    int shadowBlur = 4;
    ApplyGaussianBlur(alphaMap, shadowMap, tempBuffer, gw, gh, shadowBlur);

    int startX = x_pos - padding;
    int startY = y_pos - padding;
    int shadowOffset = 3; 

    for (int j = 0; j < gh; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        unsigned char* pAlphaRow = alphaMap + j * gw;
        
        unsigned char* pShadowRow = NULL;
        int sY = j - shadowOffset;
        if (sY >= 0 && sY < gh) pShadowRow = shadowMap + sY * gw;

        /* Bevel pointers */
        int bevelOffset = 2;
        unsigned char* pBevelRowUL = (j >= bevelOffset) ? alphaMap + (j - bevelOffset) * gw : NULL;
        unsigned char* pBevelRowDR = (j < gh - bevelOffset) ? alphaMap + (j + bevelOffset) * gw : NULL;

        /* Sheen calculation */
        int sheen = 0;
        if (h > 0) {
            int localY = j - padding;
            if (localY < 0) localY = 0;
            else if (localY > h) localY = h;
            sheen = (255 - (localY * 255) / h) >> 3;
        }

        for (int i = 0; i < gw; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;

            unsigned char srcAlpha = pAlphaRow[i];
            
            /* Shadow */
            int shadowVal = 0;
            int sX = i - shadowOffset;
            if (pShadowRow && sX >= 0 && sX < gw) shadowVal = pShadowRow[sX];

            if (srcAlpha == 0 && shadowVal == 0) continue;

            int bodyR = r, bodyG = g, bodyB = b;
            if (colorCb) colorCb(screenX, screenY, &bodyR, &bodyG, &bodyB, userData);

            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int finalA = (bgPixel >> 24) & 0xFF;
            int finalR = (bgPixel >> 16) & 0xFF;
            int finalG = (bgPixel >> 8) & 0xFF;
            int finalB = bgPixel & 0xFF;

            /* Render Shadow */
            if (shadowVal > 0 && srcAlpha < 255) {
                int shadowColor = 0; 
                int shadowOpacity = (shadowVal * 100) / 255; 
                int invShadow = 255 - shadowOpacity;
                finalR = (finalR * invShadow + shadowColor * shadowOpacity) / 255;
                finalG = (finalG * invShadow + shadowColor * shadowOpacity) / 255;
                finalB = (finalB * invShadow + shadowColor * shadowOpacity) / 255;
                finalA = max(finalA, shadowOpacity);
            }

            /* Render Glass Body */
            if (srcAlpha > 0) {
                /* Refraction / Bevel */
                int valUL = 0, valDR = 0;
                if (pBevelRowUL && i >= bevelOffset) valUL = pBevelRowUL[i - bevelOffset];
                if (pBevelRowDR && i < gw - bevelOffset) valDR = pBevelRowDR[i + bevelOffset];

                int highlight = (int)srcAlpha - (int)valUL;
                int refraction = (int)srcAlpha - (int)valDR;

                /* Body Transparency & Sheen */
                int bodyAlpha = 15 + sheen;
                int invBodyAlpha = 255 - bodyAlpha;
                
                finalR = DIV255(finalR * invBodyAlpha + bodyR * bodyAlpha);
                finalG = DIV255(finalG * invBodyAlpha + bodyG * bodyAlpha);
                finalB = DIV255(finalB * invBodyAlpha + bodyB * bodyAlpha);
                finalA = max(finalA, bodyAlpha);

                /* Refraction Rim */
                if (refraction > 50) {
                    int rimA = refraction >> 1;
                    int invRimA = 255 - rimA;
                    finalR = DIV255(finalR * invRimA + bodyR * rimA);
                    finalG = DIV255(finalG * invRimA + bodyG * rimA);
                    finalB = DIV255(finalB * invRimA + bodyB * rimA);
                    finalA = max(finalA, rimA);
                }

                /* Specular Highlight */
                if (highlight > 40) {
                    int spec = (highlight * highlight * highlight) / 25500;
                    if (spec > 255) spec = 255;
                    if (highlight > 200) spec += 50;
                    if (spec > 255) spec = 255;

                    if (spec > 0) {
                        /* Diamond Fire */
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
}

/**
 * @brief Render Hong Kong style Neon Tube effect (Physically-based)
 * V2.0: Multi-layer rendering (Atmosphere + Tube + Core)
 */
void RenderNeonEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    /* 1. Dynamic Buffer Allocation */
    int padding = 20; /* Sufficient padding for wide glow */
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    int neededSize = gw * gh;

    /* Ensure buffers are large enough */
    if (neededSize > g_effectBufferSize) {
        int newSize = neededSize * 2;
        
        if (g_effectBuffer1) free(g_effectBuffer1);
        if (g_effectBuffer2) free(g_effectBuffer2);
        if (g_effectBuffer3) free(g_effectBuffer3);

        g_effectBuffer1 = (unsigned char*)malloc(newSize);
        g_effectBuffer2 = (unsigned char*)malloc(newSize);
        g_effectBuffer3 = (unsigned char*)malloc(newSize);

        if (!g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
            if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
            if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
            if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
            g_effectBufferSize = 0;
            return;
        }
        g_effectBufferSize = newSize;
    }

    /* 
     * Buffer Strategy:
     * Buf1: Original -> Temp -> Wide Glow (Final)
     * Buf2: Eroded -> Tube Body (Final)
     * Buf3: Sharp Outline -> Temp
     */
    unsigned char* buf1 = g_effectBuffer1;
    unsigned char* buf2 = g_effectBuffer2;
    unsigned char* buf3 = g_effectBuffer3;

    /* Clear buffers */
    memset(buf1, 0, neededSize);
    memset(buf2, 0, neededSize);
    memset(buf3, 0, neededSize);

    /* Step 1: Load Original Bitmap into Buf1 */
    for (int j = 0; j < h; j++) {
        memcpy(buf1 + (j + padding) * gw + padding, bitmap + j * w, w);
    }

    /* Step 2: Erode Buf1 into Buf2 to create inner mask */
    /* Radius 2 erosion for a decent tube thickness */
    int erosion = 2; 
    for (int j = 0; j < gh; j++) {
        for (int i = 0; i < gw; i++) {
            /* Fast erosion: find min value in kernel */
            unsigned char minVal = 255;
            
            /* Center */
            unsigned char val = buf1[j * gw + i];
            if (val < minVal) minVal = val;
            
            /* Left */
            if (i > erosion) {
                val = buf1[j * gw + (i - erosion)];
                if (val < minVal) minVal = val;
            } else minVal = 0;

            /* Right */
            if (i < gw - erosion) {
                val = buf1[j * gw + (i + erosion)];
                if (val < minVal) minVal = val;
            } else minVal = 0;

            /* Up */
            if (j > erosion) {
                val = buf1[(j - erosion) * gw + i];
                if (val < minVal) minVal = val;
            } else minVal = 0;

            /* Down */
            if (j < gh - erosion) {
                val = buf1[(j + erosion) * gw + i];
                if (val < minVal) minVal = val;
            } else minVal = 0;

            buf2[j * gw + i] = minVal;
        }
    }

    /* Step 3: Subtract Eroded (Buf2) from Original (Buf1) to get Outline in Buf3 */
    for (int k = 0; k < neededSize; k++) {
        int diff = (int)buf1[k] - (int)buf2[k];
        if (diff < 0) diff = 0;
        /* Boost alpha to make outline distinct */
        if (diff > 0) diff = (diff * 3) / 2; 
        if (diff > 255) diff = 255;
        buf3[k] = (unsigned char)diff;
    }

    /* Step 4: Create "Tube Body" (Narrow Blur) */
    /* Source: Buf3 (Outline) -> Dest: Buf2 (Tube). Temp: Buf1. */
    /* Radius 2 gives a nice glass curve feel */
    ApplyGaussianBlur(buf3, buf2, buf1, gw, gh, 2);

    /* Step 5: Create "Ambient Glow" (Wide Blur) */
    /* Source: Buf2 (Tube) -> Dest: Buf1 (Glow). Temp: Buf3. */
    /* Radius 12 for atmospheric dispersion */
    ApplyGaussianBlur(buf2, buf1, buf3, gw, gh, 12);

    /* Now:
       Buf1 = Wide Ambient Glow
       Buf2 = Soft Tube Body
    */

    int startX = x_pos - padding;
    int startY = y_pos - padding;

    /* Step 6: Compositing */
    for (int j = 0; j < gh; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        unsigned char* pGlowRow = buf1 + j * gw;
        unsigned char* pTubeRow = buf2 + j * gw;

        for (int i = 0; i < gw; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;

            int glowAlpha = pGlowRow[i];
            int tubeAlpha = pTubeRow[i];

            /* Skip empty pixels */
            if (glowAlpha == 0 && tubeAlpha == 0) continue;

            /* Determine Base Color */
            int baseR = r, baseG = g, baseB = b;
            if (colorCb) {
                colorCb(screenX, screenY, &baseR, &baseG, &baseB, userData);
            }

            /* Layer 1: Ambient Atmosphere */
            int ambR = (baseR * glowAlpha) >> 8; 
            int ambG = (baseG * glowAlpha) >> 8;
            int ambB = (baseB * glowAlpha) >> 8;
            /* Attenuate atmosphere */
            ambR = (ambR * 60) / 100; 
            ambG = (ambG * 60) / 100;
            ambB = (ambB * 60) / 100;

            /* Layer 2: Tube Body (Colored Glass) */
            int tubeR = (baseR * tubeAlpha) >> 8;
            int tubeG = (baseG * tubeAlpha) >> 8;
            int tubeB = (baseB * tubeAlpha) >> 8;
            /* Boost saturation/brightness of the glass itself */
            tubeR = (tubeR * 120) / 100;
            tubeG = (tubeG * 120) / 100;
            tubeB = (tubeB * 120) / 100;

            /* Layer 3: Plasma Core (White Hot) */
            int coreR = 0, coreG = 0, coreB = 0;
            if (tubeAlpha > 160) {
                /* Sharp ramp up above 160 */
                int coreInt = (tubeAlpha - 160) * 3; 
                if (coreInt > 255) coreInt = 255;
                
                coreR = coreInt;
                coreG = coreInt;
                coreB = coreInt;
            }

            /* Additive Synthesis */
            int finalR = ambR + tubeR + coreR;
            int finalG = ambG + tubeG + coreG;
            int finalB = ambB + tubeB + coreB;
            int finalA = (glowAlpha / 2) + tubeAlpha; 

            /* Blend with Background */
            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            /* Add neon light to background */
            int outR = bgR + finalR;
            int outG = bgG + finalG;
            int outB = bgB + finalB;
            int outA = bgA + finalA; 

            /* Clamp */
            if (outR > 255) outR = 255;
            if (outG > 255) outG = 255;
            if (outB > 255) outB = 255;
            if (outA > 255) outA = 255;

            *pPixel = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }
}

/**
 * @brief Free static resources used by drawing effects
 */
void CleanupDrawingEffects(void) {
    if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
    if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
    if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
    g_effectBufferSize = 0;
}
