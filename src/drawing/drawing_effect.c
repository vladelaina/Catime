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
 * @brief Free static resources used by drawing effects
 */
void CleanupDrawingEffects(void) {
    if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
    if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
    if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
    g_effectBufferSize = 0;
}
