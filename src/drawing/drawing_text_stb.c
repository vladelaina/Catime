/**
 * @file drawing_text_stb.c
 * @brief Implementation of text rendering using stb_truetype
 */

#include "drawing/drawing_text_stb.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../libs/stb/stb_truetype.h"

/* Global font state */
static unsigned char* g_fontBuffer = NULL;
static stbtt_fontinfo g_fontInfo;
static char g_currentFontPath[MAX_PATH] = {0};
static BOOL g_fontLoaded = FALSE;

/* Fallback font state (Segoe UI Emoji) */
static unsigned char* g_fallbackFontBuffer = NULL;
static stbtt_fontinfo g_fallbackFontInfo;
static BOOL g_fallbackFontLoaded = FALSE;

/* Memory mapping handles */
static HANDLE g_hFontFile = INVALID_HANDLE_VALUE;
static HANDLE g_hFontMapping = NULL;

/* Fallback memory mapping handles */
static HANDLE g_hFallbackFontFile = INVALID_HANDLE_VALUE;
static HANDLE g_hFallbackFontMapping = NULL;

/* Accessors for external modules */
BOOL IsFontLoadedSTB(void) { return g_fontLoaded; }
BOOL IsFallbackFontLoadedSTB(void) { return g_fallbackFontLoaded; }
stbtt_fontinfo* GetMainFontInfoSTB(void) { return &g_fontInfo; }
stbtt_fontinfo* GetFallbackFontInfoSTB(void) { return &g_fallbackFontInfo; }

/* Helper to map file into memory */
static unsigned char* LoadFontMapping(const char* path, HANDLE* phFile, HANDLE* phMapping) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    void* pView = NULL;
    
    /* Convert UTF-8 path to Wide Char for Windows Unicode support */
    wchar_t wPath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH) == 0) {
        /* Fallback if conversion fails */
        return NULL;
    }

    hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    /* Create mapping for the whole file */
    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return NULL;
    }

    /* Map view of the file */
    pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pView) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return NULL;
    }

    *phFile = hFile;
    *phMapping = hMapping;
    return (unsigned char*)pView;
}

void CleanupFontSTB(void) {
    /* Cleanup main font */
    if (g_fontBuffer) {
        UnmapViewOfFile(g_fontBuffer);
        g_fontBuffer = NULL;
    }
    if (g_hFontMapping) {
        CloseHandle(g_hFontMapping);
        g_hFontMapping = NULL;
    }
    if (g_hFontFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hFontFile);
        g_hFontFile = INVALID_HANDLE_VALUE;
    }
    
    /* Cleanup fallback font */
    if (g_fallbackFontBuffer) {
        UnmapViewOfFile(g_fallbackFontBuffer);
        g_fallbackFontBuffer = NULL;
    }
    if (g_hFallbackFontMapping) {
        CloseHandle(g_hFallbackFontMapping);
        g_hFallbackFontMapping = NULL;
    }
    if (g_hFallbackFontFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hFallbackFontFile);
        g_hFallbackFontFile = INVALID_HANDLE_VALUE;
    }
    
    g_fontLoaded = FALSE;
    g_fallbackFontLoaded = FALSE;
    memset(g_currentFontPath, 0, sizeof(g_currentFontPath));
}

BOOL InitFontSTB(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;

    /* If already loaded same font, skip */
    if (g_fontLoaded && strcmp(g_currentFontPath, fontFilePath) == 0) {
        return TRUE;
    }

    HANDLE hNewFile = INVALID_HANDLE_VALUE;
    HANDLE hNewMapping = NULL;
    
    // Load to temp buffer (view) first
    unsigned char* newBuffer = LoadFontMapping(fontFilePath, &hNewFile, &hNewMapping);
    if (!newBuffer) {
        return FALSE;
    }

    stbtt_fontinfo newInfo;
    if (!stbtt_InitFont(&newInfo, newBuffer, stbtt_GetFontOffsetForIndex(newBuffer, 0))) {
        UnmapViewOfFile(newBuffer);
        CloseHandle(hNewMapping);
        CloseHandle(hNewFile);
        return FALSE;
    }

    // Success - now replace the global state
    CleanupFontSTB();
    
    g_fontBuffer = newBuffer;
    g_fontInfo = newInfo;
    g_hFontFile = hNewFile;
    g_hFontMapping = hNewMapping;
    strncpy(g_currentFontPath, fontFilePath, MAX_PATH - 1);
    g_fontLoaded = TRUE;
    
    LOG_INFO("STB Font loaded successfully: %s", fontFilePath);

    /* Load Fallback Font */
    /* Priority:
       1. Microsoft YaHei (msyh.ttc) - Best coverage for CJK, Blocks & BW Emojis
       2. Microsoft YaHei (msyh.ttf) - Legacy
       3. Segoe UI Symbol (seguisym.ttf) - Good for blocks
       4. Segoe UI Emoji (seguiemj.ttf) - Last resort (might render blank in STB)
    */
    const char* fallbackPath = "C:\\Windows\\Fonts\\msyh.ttc";
    HANDLE hFallbackFile = INVALID_HANDLE_VALUE;
    HANDLE hFallbackMapping = NULL;
    unsigned char* fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    
    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\msyh.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguisym.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }
    
    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguiemj.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (fallbackBuffer) {
        if (stbtt_InitFont(&g_fallbackFontInfo, fallbackBuffer, stbtt_GetFontOffsetForIndex(fallbackBuffer, 0))) {
            g_fallbackFontBuffer = fallbackBuffer;
            g_hFallbackFontFile = hFallbackFile;
            g_hFallbackFontMapping = hFallbackMapping;
            g_fallbackFontLoaded = TRUE;
            LOG_INFO("STB Fallback Font loaded: %s", fallbackPath);
        } else {
            UnmapViewOfFile(fallbackBuffer);
            CloseHandle(hFallbackMapping);
            CloseHandle(hFallbackFile);
            LOG_WARNING("Failed to init STB info for fallback font");
        }
    } else {
        LOG_WARNING("Failed to load fallback font (Emoji/Symbol)");
    }
    
    return TRUE;
}

/**
 * @brief Blend a single character bitmap into the destination buffer
 */
void BlendCharBitmapSTB(void* destBits, int destWidth, int destHeight, 
                          int x_pos, int y_pos, 
                          unsigned char* bitmap, int w, int h, 
                          int r, int g, int b) {
    DWORD* pixels = (DWORD*)destBits;

    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int screen_x = x_pos + i;
            int screen_y = y_pos + j;

            if (screen_x >= 0 && screen_x < destWidth && screen_y >= 0 && screen_y < destHeight) {
                unsigned char alpha = bitmap[j * w + i];
                if (alpha == 0) continue;

                /* Calculate premultiplied color values for UpdateLayeredWindow */
                DWORD finalR = (r * alpha) / 255;
                DWORD finalG = (g * alpha) / 255;
                DWORD finalB = (b * alpha) / 255;
                DWORD finalA = (DWORD)alpha;

                DWORD currentPixel = pixels[screen_y * destWidth + screen_x];
                DWORD currentA = (currentPixel >> 24) & 0xFF;
                
                /* If new pixel is more opaque, overwrite */
                if (alpha > currentA) {
                    pixels[screen_y * destWidth + screen_x] = 
                        (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
                }
            }
        }
    }
}

void BlendCharBitmapGradientSTB(void* destBits, int destWidth, int destHeight, 
                                int x_pos, int y_pos, 
                                unsigned char* bitmap, int w, int h, 
                                int startX, int totalWidth, int gradientType) {
    DWORD* pixels = (DWORD*)destBits;
    
    const GradientInfo* info = GetGradientInfo((GradientType)gradientType);
    if (!info) return; // Should not happen if checked before

    int r1 = GetRValue(info->startColor);
    int g1 = GetGValue(info->startColor);
    int b1 = GetBValue(info->startColor);
    
    int r2 = GetRValue(info->endColor);
    int g2 = GetGValue(info->endColor);
    int b2 = GetBValue(info->endColor);

    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int screen_x = x_pos + i;
            int screen_y = y_pos + j;

            if (screen_x >= 0 && screen_x < destWidth && screen_y >= 0 && screen_y < destHeight) {
                unsigned char alpha = bitmap[j * w + i];
                if (alpha == 0) continue;

                /* Calculate gradient based on X position relative to startX */
                float t = 0.0f;
                if (totalWidth > 0) {
                    t = (float)(screen_x - startX) / (float)totalWidth;
                }
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;

                int r = (int)(r1 + (r2 - r1) * t);
                int g = (int)(g1 + (g2 - g1) * t);
                int b = (int)(b1 + (b2 - b1) * t);

                /* Premultiplied alpha */
                DWORD finalR = (r * alpha) / 255;
                DWORD finalG = (g * alpha) / 255;
                DWORD finalB = (b * alpha) / 255;
                DWORD finalA = (DWORD)alpha;

                DWORD currentPixel = pixels[screen_y * destWidth + screen_x];
                DWORD currentA = (currentPixel >> 24) & 0xFF;
                
                /* Simple alpha blending with destination */
                /* Since we are drawing stroke (background), we might be overwritten by fill later. */
                /* Standard over operator: Src + Dst*(1-SrcA) */
                /* But here we just use max alpha for simple layering if we draw back-to-front */
                
                /* For stroke, we want to overwrite the background (usually transparent). */
                if (alpha > currentA) {
                    pixels[screen_y * destWidth + screen_x] = 
                        (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
                }
            }
        }
    }
}

void GetCharMetricsSTB(wchar_t c, wchar_t nextC, float scale, float fallbackScale, GlyphMetrics* out) {
    out->index = 0;
    out->isFallback = FALSE;
    out->advance = 0;
    out->kern = 0;

    if (c == L'\n' || c == L'\r') return;
    
    if (c == L'\t') {
        // Tab = 4 spaces
        int spaceIdx = stbtt_FindGlyphIndex(&g_fontInfo, ' ');
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&g_fontInfo, spaceIdx, &adv, &lsb);
        out->advance = (int)(adv * scale * 4);
        return;
    }

    out->index = stbtt_FindGlyphIndex(&g_fontInfo, (int)c);
    
    if (out->index == 0 && g_fallbackFontLoaded && c != L' ') {
        int fallbackIndex = stbtt_FindGlyphIndex(&g_fallbackFontInfo, (int)c);
        if (fallbackIndex != 0) {
            out->index = fallbackIndex;
            out->isFallback = TRUE;
        }
    }

    int adv, lsb;
    if (out->isFallback) {
        stbtt_GetGlyphHMetrics(&g_fallbackFontInfo, out->index, &adv, &lsb);
        out->advance = (int)(adv * fallbackScale);
    } else {
        stbtt_GetGlyphHMetrics(&g_fontInfo, out->index, &adv, &lsb);
        out->advance = (int)(adv * scale);
        
        // Kerning
        if (nextC && nextC != L'\n' && nextC != L'\r') {
            int nextIdx = stbtt_FindGlyphIndex(&g_fontInfo, (int)nextC);
            if (nextIdx != 0) {
                out->kern = (int)(stbtt_GetGlyphKernAdvance(&g_fontInfo, out->index, nextIdx) * scale);
            }
        }
    }
}

BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height) {
    if (!g_fontLoaded || !text) return FALSE;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)fontSize);
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)fontSize) : 0;

    int maxWidth = 0;
    int curLineWidth = 0;
    int lineCount = 1;
    size_t len = wcslen(text);

    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            if (curLineWidth > maxWidth) maxWidth = curLineWidth;
            curLineWidth = 0;
            lineCount++;
            continue;
        }
        if (text[i] == L'\r') continue;

        GlyphMetrics gm;
        GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
        curLineWidth += gm.advance + gm.kern;
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = (int)((ascent - descent + lineGap) * scale);

    if (width) *width = maxWidth;
    if (height) *height = lineCount * lineHeight;
    
    return TRUE;
}

void RenderTextSTB(void* bits, int width, int height, const wchar_t* text, 
                   COLORREF color, int fontSize, float fontScale, BOOL editMode) {
    if (!g_fontLoaded || !text || !bits) return;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)(fontSize * fontScale)) : 0;
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = (int)((ascent - descent + lineGap) * scale);
    int baselineOffset = (int)(ascent * scale);
    
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    // Pre-calculate line widths for centering
    // We can do a quick pass or re-use Measure logic per line
    size_t len = wcslen(text);
    int currentY = 0;
    
    // Calculate total text height to vertically center the whole block
    int totalTextHeight = 0;
    int numLines = 0;
    {
        int w, h;
        MeasureTextSTB(text, (int)(fontSize * fontScale), &w, &h);
        totalTextHeight = h;
        numLines = h / lineHeight;
    }
    
    int startY = (height - totalTextHeight) / 2;
    int currentLineStart = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (text[i] == L'\n' || text[i] == L'\0') {
            // Line complete, render it
            int lineWidth = 0;
            // Calculate width of this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                lineWidth += gm.advance + gm.kern;
            }
            
            int currentX = (width - lineWidth) / 2;
            int lineY = startY + currentY * lineHeight + baselineOffset;

            // Render line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                
                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                
                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;
                    
                    if (gm.isFallback) {
                        bitmap = stbtt_GetGlyphBitmap(&g_fallbackFontInfo, fallbackScale, fallbackScale, gm.index, &w, &h, &xoff, &yoff);
                    } else {
                        bitmap = stbtt_GetGlyphBitmap(&g_fontInfo, scale, scale, gm.index, &w, &h, &xoff, &yoff);
                    }
                    
                    if (bitmap) {
                        BlendCharBitmapSTB(bits, width, height, currentX + xoff, lineY + yoff, bitmap, w, h, r, g, b);
                        stbtt_FreeBitmap(bitmap, NULL);
                    }
                }
                currentX += gm.advance + gm.kern;
            }
            
            currentY++;
            currentLineStart = i + 1;
        }
    }
}
