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
 * @brief Apply Optimized Box Blur (Sliding Window)
 * O(1) per pixel regardless of radius
 */
void ApplyGaussianBlur(unsigned char* src, unsigned char* dest, unsigned char* tempBuffer, int w, int h, int radius) {
    if (w <= 0 || h <= 0) return;
    
    if (radius < 1) {
        memcpy(dest, src, w * h);
        return;
    }

    /* Horizontal Pass: src -> tempBuffer */
    /* Using sliding window sum */
    for (int y = 0; y < h; y++) {
        int rowOffset = y * w;
        unsigned char* rowSrc = src + rowOffset;
        unsigned char* rowDest = tempBuffer + rowOffset;
        
        int sum = 0;
        
        /* Initialize window for x=0 */
        for (int k = -radius; k <= radius; k++) {
            int idx = k;
            if (idx < 0) idx = 0;
            if (idx >= w) idx = w - 1;
            sum += rowSrc[idx];
        }
        
        int div = radius * 2 + 1;
        int reciprocal = (1 << 20) / div;
        
        for (int x = 0; x < w; x++) {
            rowDest[x] = (sum * reciprocal) >> 20;
            
            /* Slide window: remove (x - radius), add (x + radius + 1) */
            int outIdx = x - radius;
            int inIdx = x + radius + 1;
            
            int outVal, inVal;
            
            if (outIdx < 0) outVal = rowSrc[0];
            else if (outIdx >= w) outVal = rowSrc[w-1];
            else outVal = rowSrc[outIdx];
            
            if (inIdx < 0) inVal = rowSrc[0]; /* Should not happen */
            else if (inIdx >= w) inVal = rowSrc[w-1];
            else inVal = rowSrc[inIdx];
            
            sum -= outVal;
            sum += inVal;
        }
    }

    /* Vertical Pass: tempBuffer -> dest */
    /* This is harder to cache-optimize because column traversal is stride*w */
    /* We can transpose or just do it. Sliding window still helps. */
    for (int x = 0; x < w; x++) {
        int sum = 0;
        int div = radius * 2 + 1;
        int reciprocal = (1 << 20) / div;
        
        /* Initialize window for y=0 */
        for (int k = -radius; k <= radius; k++) {
            int idx = k;
            if (idx < 0) idx = 0;
            if (idx >= h) idx = h - 1;
            sum += tempBuffer[idx * w + x];
        }
        
        for (int y = 0; y < h; y++) {
            dest[y * w + x] = (sum * reciprocal) >> 20;
            
            /* Slide window */
            int outIdx = y - radius;
            int inIdx = y + radius + 1;
            
            int outVal, inVal;
            
            if (outIdx < 0) outVal = tempBuffer[0 * w + x];
            else if (outIdx >= h) outVal = tempBuffer[(h-1) * w + x];
            else outVal = tempBuffer[outIdx * w + x];
            
            if (inIdx >= h) inVal = tempBuffer[(h-1) * w + x];
            else inVal = tempBuffer[inIdx * w + x];
            
            sum -= outVal;
            sum += inVal;
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

    if (neededSize > g_effectBufferSize || !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
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

    if (neededSize > g_effectBufferSize || !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
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
 * Multi-layer rendering (Atmosphere + Tube + Core)
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
    if (neededSize > g_effectBufferSize || !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
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
 * @brief Render Holographic/Crystal effect
 * "Phantom Crystal" Style: Transparent glass with soft colored glow.
 * Uses user's selected color (supports gradients) for the atmospheric glow.
 * 
 * Features:
 * 1. Double Gaussian Blur for smooth, wide dispersion.
 * 2. User color respected - pure colors stay pure, gradients flow.
 */
void RenderHolographicEffect(DWORD* pixels, int destWidth, int destHeight,
                            int x_pos, int y_pos,
                            unsigned char* bitmap, int w, int h,
                            int r, int g, int b,
                            GlowColorCallback colorCb, void* userData,
                            int timeOffset) {
    (void)timeOffset;
    
    /* 1. Dynamic Buffer Allocation */
    int padding = 16;
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    int neededSize = gw * gh;

    if (neededSize > g_effectBufferSize || !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
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
    unsigned char* glowMap = g_effectBuffer2;
    unsigned char* tempMap = g_effectBuffer3;
    
    /* 2. Prepare Maps */
    memset(alphaMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }
    
    /* Generate wide soft glow */
    /* Double Gaussian for smooth, wide dispersion - critical for visual quality */
    ApplyGaussianBlur(alphaMap, glowMap, tempMap, gw, gh, 10);
    ApplyGaussianBlur(glowMap, glowMap, tempMap, gw, gh, 10);

    int startX = x_pos - padding;
    int startY = y_pos - padding;

    for (int j = 1; j < gh - 1; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        unsigned char* pAlphaRow = alphaMap + j * gw;
        unsigned char* pGlowRow = glowMap + j * gw;
        unsigned char* pRowPrev = alphaMap + (j - 1) * gw;
        unsigned char* pRowNext = alphaMap + (j + 1) * gw;

        /* Calculate Loop Bounds to remove per-pixel boundary checks */
        /* Range of i is [1, gw - 1) normally */
        int minI = 1;
        int maxI = gw - 1;
        
        /* screenX = startX + i */
        /* Constraint 1: screenX >= 0  =>  startX + i >= 0  =>  i >= -startX */
        if (minI < -startX) minI = -startX;
        
        /* Constraint 2: screenX < destWidth  =>  startX + i < destWidth  =>  i < destWidth - startX */
        if (maxI > destWidth - startX) maxI = destWidth - startX;

        for (int i = minI; i < maxI; i++) {
            int screenX = startX + i;
            /* Removed per-pixel boundary check: if (screenX < 0 || screenX >= destWidth) continue; */

            /* Early exit: skip completely empty regions */
            int alphaG = pAlphaRow[i];
            unsigned char glow = pGlowRow[i];
            
            if (alphaG == 0 && glow == 0) {
                if (pAlphaRow[i-1] == 0 && pAlphaRow[i+1] == 0) continue;
            }

            int alphaR = pAlphaRow[i - 1];
            int alphaB = pAlphaRow[i + 1];
            
            if (alphaG == 0 && glow == 0 && alphaR == 0 && alphaB == 0) continue;

            /* Resolve Pixel Color (Support Gradients) */
            int curR = r, curG = g, curB = b;
            
            if (colorCb) {
                /* Get user's color at this position (supports gradients) */
                colorCb(screenX, screenY, &curR, &curG, &curB, userData);
            }

            /* --- Layer 1: The "Prismatic" Glass Body --- */
            int bodyR_val = 0, bodyG_val = 0, bodyB_val = 0;
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

            /* --- Layer 2: White Rim (Edge Highlight) --- */
            int rimA = 0;
            if (alphaG > 0 || alphaR > 0 || alphaB > 0) {
                int dzdx = ((int)pAlphaRow[i+1] - (int)pAlphaRow[i-1]);
                int dzdy = ((int)pRowNext[i] - (int)pRowPrev[i]);
                int lighting = -(dzdx + dzdy);
                int mag = abs(dzdx) + abs(dzdy);
                
                if (mag > 0) {
                    rimA = (mag * 85) >> 8;
                    if (lighting > 0) {
                        rimA += lighting;
                    }
                }
                if (rimA > 255) rimA = 255;
                rimA = (rimA * rimA) >> 8; 
            }

            /* --- Layer 3: Atmospheric Glow (User Color) --- */
            int specR = 0, specG = 0, specB = 0;
            
            if (glow > 0) {
                /* Use user's selected color for the glow */
                /* Apply glow intensity with boost for visibility */
                specR = (curR * glow * 358) >> 16;
                specG = (curG * glow * 358) >> 16;
                specB = (curB * glow * 358) >> 16;
            }

            /* --- Composition --- */
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

/* --- OPTIMIZATION HELPERS --- */

/* Fast integer square root approximation for small numbers (0-255 range gradients) */
/* "Octagonal" Approximation: max + 0.375 * min */
/* Error < 3%, much rounder than max + 0.5 * min */
static inline int FastIntDist(int dx, int dy) {
    int ax = abs(dx);
    int ay = abs(dy);
    int mx = (ax > ay) ? ax : ay;
    int mn = (ax > ay) ? ay : ax;
    /* mx + (mn * 3/8) -> mx + 0.25mn + 0.125mn */
    return mx + (mn >> 2) + (mn >> 3);
}

/* Static LUT for specular power curve (Power 4) */
static unsigned char g_specularLUT[256];
static BOOL g_lutInitialized = FALSE;

/* Static LUT for slope-based color/alpha modifiers (Branchless Optimization) */
typedef struct {
    short dR;
    short dG;
    short dB;
    unsigned char alpha;
} SlopeProps;
static SlopeProps g_slopeLUT[256];
static BOOL g_slopeLUTInit = FALSE;

static void InitSpecularLUT(void) {
    if (g_lutInitialized) return;
    for (int i = 0; i < 256; i++) {
        float f = i / 255.0f;
        f = f * f * f * f; /* Power 4 */
        g_specularLUT[i] = (unsigned char)(f * 255.0f);
    }
    g_lutInitialized = TRUE;
}

static void InitSlopeLUT(void) {
    if (g_slopeLUTInit) return;
    
    for (int i = 0; i < 256; i++) {
        int slope_int = i;
        int clearAmt = 0, darkAmt = 0;
        
        /* Match Logic */
        if (slope_int < 102) {
            clearAmt = ((102 - slope_int) * 200) >> 8;
        } else {
            darkAmt = ((slope_int - 102) * 150) >> 8;
        }
        
        int fresnel = 0;
        if (slope_int > 25) {
            int f = ((slope_int - 25) * 120) >> 8;
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
    g_slopeLUTInit = TRUE;
}

/**
 * @brief Render Liquid Flow/Caustics Effect
 * CPU Usage: True Zero (Branchless)
 * Visuals: High-Bright Mercury
 */
void RenderLiquidEffect(DWORD* pixels, int destWidth, int destHeight,
                       int x_pos, int y_pos,
                       unsigned char* bitmap, int w, int h,
                       int r, int g, int b,
                       GlowColorCallback colorCb, void* userData,
                       int timeOffset) {
    
    if (!g_lutInitialized) InitSpecularLUT();
    if (!g_slopeLUTInit) InitSlopeLUT();

    /* 1. Dynamic Buffer Allocation (Extreme Memory Optimization) */
    int padding = 12;
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    int neededSize = gw * gh;

    /* Check if resize needed */
    if (neededSize > g_effectBufferSize || !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
        /* Revert to standard 3-buffer allocation to maintain compatibility with other effects */
        /* Other effects assume if g_effectBufferSize is sufficient, ALL 3 buffers exist */
        
        int newSize = neededSize; 
        
        /* Use realloc for efficiency */
        unsigned char* new1 = (unsigned char*)realloc(g_effectBuffer1, newSize);
        unsigned char* new2 = (unsigned char*)realloc(g_effectBuffer2, newSize);
        unsigned char* new3 = (unsigned char*)realloc(g_effectBuffer3, newSize);
        
        if (!new1 || !new2 || !new3) {
            /* Allocation failed - emergency cleanup */
            if (new1) free(new1); else if (g_effectBuffer1) free(g_effectBuffer1);
            if (new2) free(new2); else if (g_effectBuffer2) free(g_effectBuffer2);
            if (new3) free(new3); else if (g_effectBuffer3) free(g_effectBuffer3);
            g_effectBuffer1 = NULL;
            g_effectBuffer2 = NULL;
            g_effectBuffer3 = NULL;
            g_effectBufferSize = 0;
            return;
        }
        
        g_effectBuffer1 = new1;
        g_effectBuffer2 = new2;
        g_effectBuffer3 = new3;
        g_effectBufferSize = newSize;
    }

    /* 
     * 2-Buffer Strategy (still used for processing efficiency)
     * Buffer1: Stores Bitmap initially, then stores the Final HeightMap
     * Buffer2: Used as Temp buffer for Gaussian Blur
     * Buffer3: Unused by Liquid, but kept alive for other effects
     */
    unsigned char* heightMap = g_effectBuffer1; 
    unsigned char* tempMap = g_effectBuffer2;   
    
    /* 2. Prepare Alpha Map (directly into what will be HeightMap) */
    memset(heightMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(heightMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }

    /* 3. Create Height Map (Thick Volume) */
    /* In-Place Blur: src(heightMap) -> temp -> dest(heightMap) */
    /* This works because ApplyGaussianBlur's passes are separated */
    ApplyGaussianBlur(heightMap, heightMap, tempMap, gw, gh, 4);

    /* 4. Precompute Flow/Warp LUT (Tri-Planar Waves) */
    /* 
     * OPTIMIZATION: Keep 't' as a wrapped integer index [0-2047]
     * No float math in the frame logic needed if we map time correctly.
     * 1 cycle = 2048 units.
     * Speed factor 0.0025 * 2048 ~= 5 units per ms?
     * Let's stick to float for 't' calculation ONCE per frame, but use INT for everything else.
     */
    #define FLOW_LUT_SIZE 2048
    #define FLOW_LUT_MASK 2047
    static int flowLUT[FLOW_LUT_SIZE];
    static BOOL flowLUTInit = FALSE;
    
    /* Only compute the base sine wave ONCE ever, or if we want to animate frequency?
       Actually, the phase changes, not the wave shape. So we compute the wave ONCE.
    */
    if (!flowLUTInit) {
        for (int i = 0; i < FLOW_LUT_SIZE; i++) {
            float angle = (i * 6.28318f) / FLOW_LUT_SIZE;
            float val = sinf(angle) * 4.0f; /* Amplitude 4.0 */
            flowLUT[i] = (int)(val); 
        }
        flowLUTInit = TRUE;
    }
    
    /* Calculate Phase Offsets */
    /* t scales 2*PI to roughly ~125.6 range in previous code.
       Here we map it to LUT index [0-2047].
       Previous: t = time * 0.0025. 
       2PI ~ 6.28.
       LUT covers 2PI.
       So LUT_index = (time * 0.0025 / 2PI) * 2048
                    = time * 0.0025 * 326
                    = time * 0.815
    */
    int t_idx = (int)((double)timeOffset * 0.815) & FLOW_LUT_MASK;

    int startX = x_pos - padding;
    int startY = y_pos - padding;
    
    /* Precalc 60 degree vector constants (sin(60) ~= 0.866 ~= 222/256) */
    /* t1, t2, t3 offsets */
    int t1 = t_idx;
    int t2 = (t_idx + 682) & FLOW_LUT_MASK; /* +1/3 cycle */
    int t3 = (t_idx + 1365) & FLOW_LUT_MASK; /* +2/3 cycle */

    /* 5. Main Optical Loop (INT OPTIMIZED) */
    for (int j = 2; j < gh - 2; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        
        /* 
         * Integer Tri-Planar Setup 
         * Direction 2 Y-comp: Y * 0.87
         */
        int yComp = (screenY * 222) >> 8; 

        for (int i = 2; i < gw - 2; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;

            /* --- FAST TRI-PLANAR INTERFERENCE --- */
            /* Wave 1: X (0 deg) */
            int idx1 = (screenX * 2 + t1) & FLOW_LUT_MASK;
            
            /* Wave 2: 0.5X + 0.87Y (60 deg) */
            int xComp2 = i >> 1; /* i / 2 */
            int idx2 = (xComp2 + yComp + t2) & FLOW_LUT_MASK;
            
            /* Wave 3: -0.5X + 0.87Y (120 deg) */
            int idx3 = (-xComp2 + yComp + t3) & FLOW_LUT_MASK;

            /* Sum the waves from LUT */
            int w1 = flowLUT[idx1];
            int w2 = flowLUT[idx2];
            int w3 = flowLUT[idx3];
            
            /* Apply Warp */
            int warpX = (w1 + w2 + w3) >> 1;
            /* Y warp uses offset lookup for swirl */
            int idx1_swirl = (idx1 + 500) & FLOW_LUT_MASK;
            int warpY = (flowLUT[idx1_swirl] + w2 - w3) >> 1;
            
            /* Coordinate Displacement */
            int srcX = i + warpX;
            int srcY = j + warpY;
            
            /* Fast Clamp */
            if (srcX < 1) srcX = 1; else if (srcX >= gw - 1) srcX = gw - 2;
            if (srcY < 1) srcY = 1; else if (srcY >= gh - 1) srcY = gh - 2;

            /* Read Mass */
            int centerIdx = srcY * gw + srcX;
            int mass = heightMap[centerIdx];
            
            /* Soft Edge Threshold (Antialiasing) */
            /* Lower threshold to capture the fading edge */
            if (mass < 24) continue; 
            
            int edgeAA = 256;
            if (mass < 56) {
                edgeAA = (mass - 24) << 3; /* * 8 */
            }
            
            /* --- FAST TOPOLOGY --- */
            /* Direct neighbor access */
            int hL = heightMap[centerIdx - 1];
            int hR = heightMap[centerIdx + 1];
            int hU = heightMap[centerIdx - gw];
            int hD = heightMap[centerIdx + gw];
            
            int dx = hR - hL;
            int dy = hD - hU;
            
            /* Integer Slope Approximation */
            /* Max slope is around 255+255=510. Scaled down. */
            /* slope float = len / 100.0f. len = sqrt(dx*dx+dy*dy) */
            /* We use FastIntDist. */
            int len = FastIntDist(dx, dy);
            
            /* 'slope' as 0-255 fixed point */
            /* float slope = len / 100.0f -> if len=100, slope=1.0 */
            /* We want to map len 0..100 to slope_int 0..255 */
            /* slope_int = len * 2.55 ~= len * 5 / 2 */
            int slope_int = (len * 5) >> 1; 
            if (slope_int > 255) slope_int = 255;

            /* --- FAST LIGHTING --- */
            /* lightDot = -(nx + ny) / 100.0f */
            /* We need lightDot > 0 for specular */
            /* So -(dx + dy) > 0  => dx + dy < 0 */
            int sumNormal = dx + dy;
            int specular = 0;
            
            if (sumNormal < 0) {
                /* Normalize sumNormal to 0-255 range for LUT lookup */
                /* Max negative sum is roughly -510. We map -100 to index 255 (max brightness) */
                int idx = -sumNormal; 
                /* Scale: if sum is -100, we want index 255 */
                /* index = (-sum * 255) / 100 = -sum * 2.55 */
                idx = (idx * 5) >> 1;
                if (idx > 255) idx = 255;
                
                specular = g_specularLUT[idx];
            }

            /* Apply Edge AA to Specular to prevent sparkling artifacts at boundaries */
            if (edgeAA < 256) {
                specular = (specular * edgeAA) >> 8;
            }

            /* --- COLOR (BRANCHLESS LUT) --- */
            int curR = r, curG = g, curB = b;
            
            if (colorCb) {
                int sampleX = x_pos - padding + srcX;
                int sampleY = y_pos - padding + srcY;
                colorCb(sampleX, sampleY, &curR, &curG, &curB, userData);
            }

            /* Slope LUT Lookup: Replaces clear/dark/fresnel/dispersion logic */
            SlopeProps p = g_slopeLUT[slope_int];
            int fR = curR + p.dR;
            int fG = curG + p.dG;
            int fB = curB + p.dB;

            /* Clamp before specular */
            if (fR < 0) fR = 0; else if (fR > 255) fR = 255;
            if (fG < 0) fG = 0; else if (fG > 255) fG = 255;
            if (fB < 0) fB = 0; else if (fB > 255) fB = 255;

            /* --- ALPHA (BRANCHLESS LUT) --- */
            /* alpha = 60 + slope * 210 (Precomputed) */
            int alpha = p.alpha;
            if (edgeAA < 256) {
                alpha = (alpha * edgeAA) >> 8;
            }

            /* BLEND */
            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            /* Fast Blend: (bg * invA + fg * alpha) >> 8 */
            int invA = 255 - alpha;
            int finalR = (bgR * invA + fR * alpha) >> 8;
            int finalG = (bgG * invA + fG * alpha) >> 8;
            int finalB = (bgB * invA + fB * alpha) >> 8;
            
            /* Additive Specular */
            finalR += specular;
            finalG += specular;
            finalB += specular;
            
            /* Re-clamp */
            if (finalR > 255) finalR = 255;
            if (finalG > 255) finalG = 255;
            if (finalB > 255) finalB = 255;

            int finalA = (bgA > alpha) ? bgA : alpha;
            if (specular > finalA) finalA = specular;
            if (finalA > 255) finalA = 255;

            *pPixel = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }
}

