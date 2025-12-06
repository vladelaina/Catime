/**
 * @file drawing_effect.c
 * @brief Implementation of visual effects for drawing (glow, blur, etc.)
 */

#include "drawing/drawing_effect.h"
#include <stdlib.h>
#include <string.h>

/* Static buffers to reuse memory and avoid reallocation */
static unsigned char* g_effectBuffer1 = NULL; /* For glowMap */
static unsigned char* g_effectBuffer2 = NULL; /* For blurredMap/temp */
static unsigned char* g_effectBuffer3 = NULL; /* For temp */
static size_t g_effectBufferSize = 0;

/**
 * @brief Apply horizontal box blur (Safe sliding window with fixed-point math)
 */
static void BoxBlurH(unsigned char* src, unsigned char* dest, int w, int h, int radius) {
    /* Fixed point scale: 65536 / (2r+1) */
    int scale = 65536 / (radius + radius + 1);
    
    for (int i = 0; i < h; i++) {
        int ti = i * w;
        int val = 0;
        
        /* Initialize window */
        for (int k = -radius; k <= radius; k++) {
            int px = k;
            if (px < 0) px = 0;
            if (px >= w) px = w - 1;
            val += src[ti + px];
        }
        
        int x = 0;
        
        /* Head: x < radius */
        int limit = (radius < w) ? radius : w;
        for (; x < limit; x++) {
            dest[ti + x] = (unsigned char)((val * scale) >> 16);
            
            int leavingIdx = x - radius;
            int enteringIdx = x + radius + 1;
            
            if (leavingIdx < 0) leavingIdx = 0;
            /* enteringIdx can't be >= w here if radius < w */
            
            val = val - src[ti + leavingIdx] + src[ti + enteringIdx];
        }
        
        /* Body: radius <= x < w - radius - 1 */
        /* No boundary checks needed here */
        limit = w - radius - 1;
        if (limit > x) {
            unsigned char* pSrc = src + ti;
            /* Pre-calculate offsets */
            int leaveOffset = -radius;
            int enterOffset = radius + 1;
            
            for (; x < limit; x++) {
                dest[ti + x] = (unsigned char)((val * scale) >> 16);
                val = val - pSrc[x + leaveOffset] + pSrc[x + enterOffset];
            }
        }
        
        /* Tail: x >= w - radius - 1 */
        for (; x < w; x++) {
            dest[ti + x] = (unsigned char)((val * scale) >> 16);
            
            int leavingIdx = x - radius;
            int enteringIdx = x + radius + 1;
            
            if (enteringIdx >= w) enteringIdx = w - 1;
            
            val = val - src[ti + leavingIdx] + src[ti + enteringIdx];
        }
    }
}

/**
 * @brief Apply vertical box blur (Safe sliding window with fixed-point math)
 */
static void BoxBlurV(unsigned char* src, unsigned char* dest, int w, int h, int radius) {
    /* Fixed point scale: 65536 / (2r+1) */
    int scale = 65536 / (radius + radius + 1);
    
    for (int i = 0; i < w; i++) {
        int ti = i;
        int val = 0;
        
        /* Initialize window */
        for (int k = -radius; k <= radius; k++) {
            int py = k;
            if (py < 0) py = 0;
            if (py >= h) py = h - 1;
            val += src[ti + py * w];
        }
        
        int y = 0;
        
        /* Head: y < radius */
        int limit = (radius < h) ? radius : h;
        for (; y < limit; y++) {
            dest[ti + y * w] = (unsigned char)((val * scale) >> 16);
            
            int leavingIdx = y - radius;
            int enteringIdx = y + radius + 1;
            
            if (leavingIdx < 0) leavingIdx = 0;
            
            val = val - src[ti + leavingIdx * w] + src[ti + enteringIdx * w];
        }
        
        /* Body */
        limit = h - radius - 1;
        if (limit > y) {
            int leaveOffset = -radius * w;
            int enterOffset = (radius + 1) * w;
            
            for (; y < limit; y++) {
                dest[ti + y * w] = (unsigned char)((val * scale) >> 16);
                int centerOffset = ti + y * w;
                val = val - src[centerOffset + leaveOffset] + src[centerOffset + enterOffset];
            }
        }
        
        /* Tail */
        for (; y < h; y++) {
            dest[ti + y * w] = (unsigned char)((val * scale) >> 16);
            
            int leavingIdx = y - radius;
            int enteringIdx = y + radius + 1;
            
            if (enteringIdx >= h) enteringIdx = h - 1;
            
            val = val - src[ti + leavingIdx * w] + src[ti + enteringIdx * w];
        }
    }
}

/**
 * @brief Simple 3-pass box blur to approximate Gaussian blur
 */
void ApplyGaussianBlur(unsigned char* src, unsigned char* dest, unsigned char* tempBuffer, int w, int h, int radius) {
    /* Pass 1 */
    BoxBlurH(src, tempBuffer, w, h, radius);
    BoxBlurV(tempBuffer, dest, w, h, radius);
    
    /* Pass 2 */
    BoxBlurH(dest, tempBuffer, w, h, radius);
    BoxBlurV(tempBuffer, dest, w, h, radius);
    
    /* Pass 3 (optional, for smoother result) */
    BoxBlurH(dest, tempBuffer, w, h, radius);
    BoxBlurV(tempBuffer, dest, w, h, radius);
}

/**
 * @brief Render glow effect using real blur
 */
void RenderGlowEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    /* Glow parameters - Increased for softer look */
    int padding = 20;        /* Spread of the glow */
    int blurRadius = 10;     /* Softness */
    /* intensity = 1.5f approx via integer math */
    
    /* Robustness check */
    if (w <= 0 || h <= 0 || !bitmap) return;
    
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    size_t neededSize = gw * gh;
    
    /* Check and reallocate buffers if needed */
    if (neededSize > g_effectBufferSize) {
        /* Add some margin to reduce reallocations */
        size_t newSize = neededSize * 2; 
        
        /* Free old buffers if any */
        if (g_effectBuffer1) free(g_effectBuffer1);
        if (g_effectBuffer2) free(g_effectBuffer2);
        if (g_effectBuffer3) free(g_effectBuffer3);
        
        g_effectBuffer1 = (unsigned char*)malloc(newSize);
        g_effectBuffer2 = (unsigned char*)malloc(newSize);
        g_effectBuffer3 = (unsigned char*)malloc(newSize);
        
        if (!g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
            /* Allocation failed, cleanup and return */
            if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
            if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
            if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
            g_effectBufferSize = 0;
            return;
        }
        
        g_effectBufferSize = newSize;
    }
    
    /* Use static buffers */
    unsigned char* glowMap = g_effectBuffer1;
    unsigned char* blurredMap = g_effectBuffer2;
    unsigned char* tempBuffer = g_effectBuffer3;
    
    /* Initialize glowMap (only need to clear used area) */
    memset(glowMap, 0, neededSize);
    
    /* Copy source bitmap into center of glow map */
    for (int j = 0; j < h; j++) {
        memcpy(glowMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }
    
    /* Apply blur using static temp buffer */
    ApplyGaussianBlur(glowMap, blurredMap, tempBuffer, gw, gh, blurRadius);
    
    /* Blend blurred map onto destination */
    /* We use additive blending for light effect */
    int startX = x_pos - padding;
    int startY = y_pos - padding;
    
    /* Pre-declare variables for loop */
    int currentR = r;
    int currentG = g;
    int currentB = b;
    
    unsigned char* pGlow = blurredMap;
    
    for (int j = 0; j < gh; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) {
            pGlow += gw;
            continue;
        }
        
        DWORD* pDestRow = pixels + screenY * destWidth;
        
        for (int i = 0; i < gw; i++) {
            unsigned char glowAlpha = *pGlow++;
            if (glowAlpha == 0) continue;
            
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;
            
            /* Get color from callback if provided */
            if (colorCb) {
                colorCb(screenX, screenY, &currentR, &currentG, &currentB, userData);
            }
            
            /* Boost intensity: 1.5x approx via integer math */
            /* (val * 3) >> 1 is exactly val * 1.5 */
            int boostAlpha = (glowAlpha * 3) >> 1;
            if (boostAlpha > 255) boostAlpha = 255;
            
            DWORD* pPixel = pDestRow + screenX;
            DWORD currentPixel = *pPixel;
            
            /* Additive blend */
            int dr = (currentPixel >> 16) & 0xFF;
            int dg = (currentPixel >> 8) & 0xFF;
            int db = currentPixel & 0xFF;
            int da = (currentPixel >> 24) & 0xFF;
            
            dr += (currentR * boostAlpha) / 255;
            dg += (currentG * boostAlpha) / 255;
            db += (currentB * boostAlpha) / 255;
            da += boostAlpha; /* Add alpha too for transparency support */
            
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;
            if (da > 255) da = 255;
            
            *pPixel = (da << 24) | (dr << 16) | (dg << 8) | db;
        }
    }
    
    /* No free() needed for static buffers */
}

/**
 * @brief Fast divide by 255 approximation: (x + 1 + (x >> 8)) >> 8
 * This is bit-perfect for 0-65535 range (which covers 255*255)
 */
#define DIV255(x) (((x) + 1 + ((x) >> 8)) >> 8)

/**
 * @brief Render glass/liquid crystal effect using Normal Mapping from Blur Field
 * V6.1 Optimized: Zero-Overhead, Pointer Hoisting, Fast Math
 */
void RenderGlassEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData) {
    /* Parameters for "Optical Prism" Glass look */
    int shadowOffset = 5;      /* Distance for light transmission */
    int shadowBlur = 8;        /* Soft diffusion for the light pool */
    int padding = shadowBlur + shadowOffset + 2;
    
    if (w <= 0 || h <= 0 || !bitmap) return;
    
    int gw = w + padding * 2;
    int gh = h + padding * 2;
    size_t neededSize = gw * gh;
    
    /* Reallocate buffers if needed */
    if (neededSize > g_effectBufferSize) {
        size_t newSize = neededSize * 2; 
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
    
    /* 1. Prepare Alpha Map (Source Image) */
    memset(alphaMap, 0, neededSize);
    for (int j = 0; j < h; j++) {
        memcpy(alphaMap + (j + padding) * gw + padding, bitmap + j * w, w);
    }
    
    /* 2. Create Soft Shadow Map (Blurred version of Alpha) */
    ApplyGaussianBlur(alphaMap, shadowMap, tempBuffer, gw, gh, shadowBlur);
    
    int startX = x_pos - padding;
    int startY = y_pos - padding;
    
    /* Iterate over the destination area */
    for (int j = 0; j < gh; j++) {
        int screenY = startY + j;
        if (screenY < 0 || screenY >= destHeight) continue;
        
        DWORD* pDestRow = pixels + screenY * destWidth;
        
        /* Optimization: Pre-calculate row pointers */
        unsigned char* pAlphaRow = alphaMap + j * gw;
        
        /* Shadow row pointer (handle Y-boundary) */
        unsigned char* pShadowRow = NULL;
        int sY = j - shadowOffset;
        if (sY >= 0 && sY < gh) {
            pShadowRow = shadowMap + sY * gw;
        }

        /* Bevel row pointers (Hoisted for performance) */
        int bevelOffset = 2;
        unsigned char* pBevelRowUL = NULL;
        unsigned char* pBevelRowDR = NULL;
        
        if (j >= bevelOffset) 
            pBevelRowUL = alphaMap + (j - bevelOffset) * gw;
        
        if (j < gh - bevelOffset)
            pBevelRowDR = alphaMap + (j + bevelOffset) * gw;
        
        /* Pre-calc Sheen for this row */
        int sheen = 0;
        if (h > 0) {
            int localY = j - padding;
            if (localY < 0) localY = 0;
            else if (localY > h) localY = h;
            /* (255 - (localY * 255 / h)) / 8 */
            sheen = (255 - (localY * 255) / h) >> 3;
        }
        
        for (int i = 0; i < gw; i++) {
            int screenX = startX + i;
            if (screenX < 0 || screenX >= destWidth) continue;
            
            /* --- 1. Fetch Source Data --- */
            /* Optimization: Pointer access */
            unsigned char srcAlpha = pAlphaRow[i];
            
            int shadowVal = 0;
            /* Check X-boundary for shadow */
            int sX = i - shadowOffset;
            if (pShadowRow && sX >= 0 && sX < gw) {
                shadowVal = pShadowRow[sX];
            }
            
            /* Optimization: Skip transparent pixels */
            if (shadowVal == 0 && srcAlpha == 0) continue;
            
            DWORD* pPixel = pDestRow + screenX;
            DWORD currentPixel = *pPixel;
            
            int bgR = (currentPixel >> 16) & 0xFF;
            int bgG = (currentPixel >> 8) & 0xFF;
            int bgB = currentPixel & 0xFF;
            int bgA = (currentPixel >> 24) & 0xFF;
            
            int finalR = bgR;
            int finalG = bgG;
            int finalB = bgB;
            int finalA = bgA;
            
            /* --- 2. Colored Transmission Shadow (Caustic Glow) --- */
            if (shadowVal > 0) {
                /* Optimization: Lazy fetch transmission colors ONLY if shadow is visible */
                int trR = r, trG = g, trB = b;
                int tgR = r, tgG = g, tgB = b;
                int tbR = r, tbG = g, tbB = b;
                
                if (colorCb) {
                    /* Dispersion Offsets */
                    int offR = shadowOffset - 1;
                    int offG = shadowOffset;
                    int offB = shadowOffset + 2;
                    
                    colorCb(screenX - offR, screenY - offR, &trR, &trG, &trB, userData);
                    colorCb(screenX - offG, screenY - offG, &tgR, &tgG, &tgB, userData);
                    colorCb(screenX - offB, screenY - offB, &tbR, &tbG, &tbB, userData);
                }
                
                int sAlpha = (shadowVal * 110) / 255;
                
                /* AO Logic: Contact Shadows */
                int aoFactor = 0;
                if (shadowVal > 200) {
                    aoFactor = (shadowVal - 200) >> 1; /* / 2 */
                }
                
                /* Composite Colors */
                int tintR = (trR * 220) / 255; 
                int tintG = (tgG * 220) / 255; 
                int tintB = (tbB * 220) / 255; 
                
                if (aoFactor > 0) {
                    /* Fast blend for AO */
                    tintR = DIV255(tintR * (255 - aoFactor));
                    tintG = DIV255(tintG * (255 - aoFactor));
                    tintB = DIV255(tintB * (255 - aoFactor));
                    sAlpha = min(255, sAlpha + aoFactor);
                }
                
                /* Apply tint using Fast Divide */
                int invAlpha = 255 - sAlpha;
                finalR = DIV255(finalR * invAlpha + tintR * sAlpha);
                finalG = DIV255(finalG * invAlpha + tintG * sAlpha);
                finalB = DIV255(finalB * invAlpha + tintB * sAlpha);
                finalA = max(finalA, sAlpha);
            }
            
            /* --- 3. Glass Body (Sharp) --- */
            if (srcAlpha > 0) {
                /* Optimization: Lazy fetch body color ONLY if body is visible */
                int bodyR = r, bodyG = g, bodyB = b;
                if (colorCb) {
                    colorCb(screenX, screenY, &bodyR, &bodyG, &bodyB, userData);
                }
                
                /* A. Bevel/Edge Detection (Pointer Based) */
                unsigned char valUL = 0;
                unsigned char valDR = 0;
                
                if (pBevelRowUL && i >= bevelOffset) valUL = pBevelRowUL[i - bevelOffset];
                if (pBevelRowDR && i < gw - bevelOffset) valDR = pBevelRowDR[i + bevelOffset];
                
                int highlight = (int)srcAlpha - (int)valUL;
                int refraction = (int)srcAlpha - (int)valDR;
                
                /* B. Body Transparency & Sheen */
                /* Real glass is mostly invisible except for reflections */
                int bodyAlpha = 15 + sheen;
                
                int invBodyAlpha = 255 - bodyAlpha;
                finalR = DIV255(finalR * invBodyAlpha + bodyR * bodyAlpha);
                finalG = DIV255(finalG * invBodyAlpha + bodyG * bodyAlpha);
                finalB = DIV255(finalB * invBodyAlpha + bodyB * bodyAlpha);
                finalA = max(finalA, bodyAlpha);
                
                /* C. Refraction Rim (Bottom-Right) */
                if (refraction > 50) {
                    int rimA = refraction >> 1; /* / 2 */
                    int invRimA = 255 - rimA;
                    
                    finalR = DIV255(finalR * invRimA + bodyR * rimA);
                    finalG = DIV255(finalG * invRimA + bodyG * rimA);
                    finalB = DIV255(finalB * invRimA + bodyB * rimA);
                    finalA = max(finalA, rimA);
                }
                
                /* D. Solar Specular Highlight (Top-Left) */
                if (highlight > 40) {
                    /* Solar Curve: Non-linear boost for "Hot" light */
                    /* Power 3 curve */
                    int spec = (highlight * highlight * highlight) / 25500; /* / (255*100) */
                    if (spec > 255) spec = 255;
                    
                    if (highlight > 200) spec += 50;
                    if (spec > 255) spec = 255;
                    
                    if (spec > 0) {
                        /* Diamond Fire: Add a slight Blue/Violet halo */
                        int fireR = spec;
                        int fireG = spec;
                        int fireB = spec + (spec / 6);
                        if (fireB > 255) fireB = 255;
                        
                        /* Additive */
                        finalR += fireR;
                        finalG += fireG;
                        finalB += fireB;
                        finalA = max(finalA, spec);
                    }
                }
            }
            
            /* Clamp */
            if (finalR > 255) finalR = 255;
            if (finalG > 255) finalG = 255;
            if (finalB > 255) finalB = 255;
            if (finalA > 255) finalA = 255;
            
            *pPixel = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
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
