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
        int count = 0;
        
        /* Initialize window for x=0 */
        for (int k = -radius; k <= radius; k++) {
            int idx = k;
            if (idx < 0) idx = 0;
            if (idx >= w) idx = w - 1;
            sum += rowSrc[idx];
            count++;
        }
        
        /* The count is always 2*radius + 1 for inner pixels, but simplified here */
        /* Actually, we can just use a fixed divisor 2*radius+1 and handle edges by clamping input */
        /* To be precise and match "Average", we usually want sum / (2r+1) */
        
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

/* Helper: HSV to RGB conversion */
static void HSVtoRGB(float h, float s, float v, int* r, int* g, int* b) {
    if (s <= 0.0f) {
        *r = *g = *b = (int)(v * 255.0f);
        return;
    }
    
    h = h - floorf(h); /* 0..1 */
    if (h < 0) h += 1.0f;
    h *= 6.0f;
    int i = (int)h;
    float f = h - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    
    float rf, gf, bf;
    switch(i) {
        case 0: rf = v; gf = t; bf = p; break;
        case 1: rf = q; gf = v; bf = p; break;
        case 2: rf = p; gf = v; bf = t; break;
        case 3: rf = p; gf = q; bf = v; break;
        case 4: rf = t; gf = p; bf = v; break;
        default: rf = v; gf = p; bf = q; break;
    }
    
    *r = (int)(rf * 255.0f);
    *g = (int)(gf * 255.0f);
    *b = (int)(bf * 255.0f);
}

/* Helper: RGB to HSV conversion */
static void RGBtoHSV(int r, int g, int b, float* h, float* s, float* v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;
    
    float cmax = max(rf, max(gf, bf));
    float cmin = min(rf, min(gf, bf));
    float delta = cmax - cmin;
    
    *v = cmax;
    
    if (cmax == 0) {
        *s = 0;
        *h = 0;
        return;
    } else {
        *s = delta / cmax;
    }
    
    if (delta == 0) {
        *h = 0;
    } else {
        if (cmax == rf) {
            *h = (gf - bf) / delta;
            if (gf < bf) *h += 6.0f;
        } else if (cmax == gf) {
            *h = (bf - rf) / delta + 2.0f;
        } else {
            *h = (rf - gf) / delta + 4.0f;
        }
        *h /= 6.0f;
    }
}

/**
 * @brief Render Holographic/Prism Dispersion effect
 * "Phantom Crystal" Style: Extremely transparent, high-quality glass with soft spectral dispersion.
 * Unlike previous versions, this does NOT fill the body with solid color.
 * It looks like a clear crystal casting a rainbow shadow/glow.
 */
void RenderHolographicEffect(DWORD* pixels, int destWidth, int destHeight,
                            int x_pos, int y_pos,
                            unsigned char* bitmap, int w, int h,
                            int r, int g, int b,
                            GlowColorCallback colorCb, void* userData,
                            int timeOffset) {
    /* 1. Dynamic Buffer Allocation */
    int padding = 16; /* Generous padding for soft glow */
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
    unsigned char* glowMap = g_effectBuffer2;
    unsigned char* tempMap = g_effectBuffer3;
    
    /* 2. Prepare Maps */
    memset(alphaMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }
    
    /* Generate wide soft glow for the spectral dispersion */
    /* Iteration 1: Initial Blur */
    ApplyGaussianBlur(alphaMap, glowMap, tempMap, gw, gh, 10);
    
    /* Iteration 2: Smoothing Blur (Double Gaussian) */
    /* FIX: Use glowMap as dest and tempMap as temp to avoid memory overlap hazard */
    /* Previous bug: dest and temp were same buffer, causing V-pass corruption */
    ApplyGaussianBlur(glowMap, glowMap, tempMap, gw, gh, 10);
    
    /* No memcpy needed */

    int startX = x_pos - padding;
    int startY = y_pos - padding;

    /* Get User Color in HSV (Base fallback) */
    float baseH, baseS, baseV;
    RGBtoHSV(r, g, b, &baseH, &baseS, &baseV);

    for (int j = 1; j < gh - 1; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;

        DWORD* pDestRow = pixels + screenY * destWidth;
        unsigned char* pAlphaRow = alphaMap + j * gw;
        unsigned char* pGlowRow = glowMap + j * gw;

        /* Neighbors for edge detection */
        unsigned char* pRowPrev = alphaMap + (j - 1) * gw;
        unsigned char* pRowNext = alphaMap + (j + 1) * gw;

        for (int i = 1; i < gw - 1; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;

            /* --- EARLY EXIT OPTIMIZATION --- */
            /* If there is no content (Body) and no light (Glow) at this pixel, skip EVERYTHING */
            /* This single check saves 90% of CPU cycles on empty space */
            int alphaG = pAlphaRow[i];
            unsigned char glow = pGlowRow[i];

            if (alphaG == 0 && glow == 0) {
                /* Check neighbors for Chromatic Aberration before giving up completely */
                /* If neighbors are also empty, we are truly in void space */
                if (pAlphaRow[i-1] == 0 && pAlphaRow[i+1] == 0) continue;
            }

            /* --- PHYSICAL OPTICS: Chromatic Aberration --- */
            /* Simulate thick glass refraction by sampling channels at offsets */
            /* R: -1px, G: 0px, B: +1px */
            int alphaR = pAlphaRow[i - 1];
            int alphaB = pAlphaRow[i + 1];
            
            /* Re-verify visibility after fetching neighbors (redundant but safe) */
            if (alphaG == 0 && glow == 0 && alphaR == 0 && alphaB == 0) continue;

            /* Resolve Pixel Color (Support Gradients/Flowing Colors) */
            int curR = r, curG = g, curB = b;
            
            /* OPTIMIZATION: Only call color callback if we actually have visible content */
            if (colorCb) {
                colorCb(screenX, screenY, &curR, &curG, &curB, userData);
                /* Recalculate HSV if color changes per pixel */
                RGBtoHSV(curR, curG, curB, &baseH, &baseS, &baseV);
            }

            /* --- Layer 1: The "Prismatic" Glass Body --- */
            /* Instead of a flat tint, we map the aberrated alphas to the user's color components */
            /* This creates natural fringing at the edges */
            
            /* OPTIMIZATION: Use integer math for channel scaling */
            int bodyR_val = 0, bodyG_val = 0, bodyB_val = 0;
            int bodyA = 0;

            if (alphaR > 0 || alphaG > 0 || alphaB > 0) {
                 bodyR_val = (curR * alphaR) >> 8;
                 bodyG_val = (curG * alphaG) >> 8;
                 bodyB_val = (curB * alphaB) >> 8;
                 
                 /* Add Volumetric Scattering only if glow is present */
                 if (glow > 0) {
                     int volume = glow >> 2; /* glow / 4 */
                     bodyR_val += (curR * volume) >> 8;
                     bodyG_val += (curG * volume) >> 8;
                     bodyB_val += (curB * volume) >> 8;
                 }
                 
                 /* Body Alpha is determined by the strongest channel (Max pooling) */
                 bodyA = max(alphaR, max(alphaG, alphaB));
                 /* Make it highly transparent (Crystal) - Multiply by ~0.15 (38/256) */
                 bodyA = (bodyA * 38) >> 8; 
            }

            /* --- Layer 2: White Rim (Edge Highlight) --- */
            int rimA = 0;
            /* Only calculate rim if we are on a potential edge (alphaG > 0 or neighbors > 0) */
            if (alphaG > 0 || alphaR > 0 || alphaB > 0) {
                /* Calculate gradient magnitude on the center channel (Green) */
                int dzdx = ((int)pAlphaRow[i+1] - (int)pAlphaRow[i-1]);
                int dzdy = ((int)pRowNext[i] - (int)pRowPrev[i]);
                
                /* Directional Lighting (Top-Left Source) */
                int lighting = -(dzdx + dzdy);
                int mag = abs(dzdx) + abs(dzdy);
                
                /* Sculpted Rim */
                if (mag > 0) {
                    rimA = mag / 3; /* Softer base rim */
                    if (lighting > 0) {
                        rimA += lighting; /* Strong specular */
                    }
                }
                if (rimA > 255) rimA = 255;
                rimA = (rimA * rimA) >> 8; 
            }

            /* --- Layer 3: Atmospheric Glow (Dispersion) --- */
            int specR = 0, specG = 0, specB = 0;
            
            if (glow > 0) {
                float posFactor = (float)(i + j) / (float)(gw + gh);
                
                /* USER COLOR DOMINANCE LOGIC */
                float finalH, finalS;
                
                if (baseS < 0.1f) {
                    /* Diamond White Prism */
                    finalH = posFactor; 
                    finalS = 0.25f; /* Very subtle pastel */
                } else {
                    /* Color Dominant Dispersion */
                    /* Use the dynamic baseH from the gradient */
                    float shift = (posFactor - 0.5f) * 0.2f; /* Reduced shift range for realism */
                    finalH = baseH + shift;
                    if (finalH < 0) finalH += 1.0f;
                    if (finalH > 1.0f) finalH -= 1.0f;
                    
                    finalS = baseS;
                    if (finalS < 0.4f) finalS = 0.4f; 
                }

                int gr, gg, gb;
                HSVtoRGB(finalH, finalS, 1.0f, &gr, &gg, &gb);
                
                /* Soft Atmospheric Light */
                specR = (gr * glow) >> 8;
                specG = (gg * glow) >> 8;
                specB = (gb * glow) >> 8;
                
                /* Boost but keep smooth (Multiply by 1.4 -> ~358/256) */
                specR = (specR * 358) >> 8;
                specG = (specG * 358) >> 8;
                specB = (specB * 358) >> 8;
            }

            /* --- Composition --- */
            DWORD* pPixel = pDestRow + screenX;
            DWORD bgPixel = *pPixel;
            int bgA = (bgPixel >> 24) & 0xFF;
            int bgR = (bgPixel >> 16) & 0xFF;
            int bgG = (bgPixel >> 8) & 0xFF;
            int bgB = bgPixel & 0xFF;

            /* 1. Add Glow (Atmosphere) */
            int outR = bgR + specR;
            int outG = bgG + specG;
            int outB = bgB + specB;

            /* 2. Composite Prismatic Body (Alpha Blend) */
            if (bodyA > 0) {
                outR = (outR * (255 - bodyA) + bodyR_val * bodyA) / 255;
                outG = (outG * (255 - bodyA) + bodyG_val * bodyA) / 255;
                outB = (outB * (255 - bodyA) + bodyB_val * bodyA) / 255;
            }

            /* 3. Add Rim (Specular) */
            if (rimA > 0) {
                outR += rimA;
                outG += rimA;
                outB += rimA;
            }

            /* Final Clamping */
            if (outR > 255) outR = 255;
            if (outG > 255) outG = 255;
            if (outB > 255) outB = 255;

            /* Final Alpha Calculation */
            /* FIX: Use max RGB brightness for glow alpha contribution to avoid clipping colors */
            int glowLum = max(specR, max(specG, specB));
            /* Ensure glowLum doesn't dominate if it's just atmosphere */
            glowLum = (glowLum * 200) / 255; 
            
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

/* End of file */

/* End of file */
