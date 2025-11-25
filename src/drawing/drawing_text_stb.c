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
static void BlendCharBitmap(void* destBits, int destWidth, int destHeight, 
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

void RenderTextSTB(void* bits, int width, int height, const wchar_t* text, 
                   COLORREF color, int fontSize, float fontScale, BOOL editMode) {
    if (!g_fontLoaded || !text || !bits) return;

    /* Main font metrics */
    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int baselineOffset = (int)(ascent * scale);

    /* Fallback font metrics */
    float fallbackScale = 0.0f;
    if (g_fallbackFontLoaded) {
        fallbackScale = stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)(fontSize * fontScale));
    }
    
    /* Calculate total text width to center it */
    int totalWidth = 0;
    size_t len = wcslen(text);
    
    /* We need to store which font is used for each glyph to avoid re-lookup */
    typedef struct {
        int index;
        BOOL isFallback;
    } GlyphInfo;
    
    GlyphInfo* glyphs = (GlyphInfo*)malloc(len * sizeof(GlyphInfo));
    int* advances = (int*)malloc(len * sizeof(int));
    
    if (!glyphs || !advances) {
        free(glyphs); free(advances);
        return;
    }

    for (size_t i = 0; i < len; i++) {
        int codepoint = (int)text[i]; 
        
        /* Try main font first */
        glyphs[i].index = stbtt_FindGlyphIndex(&g_fontInfo, codepoint);
        glyphs[i].isFallback = FALSE;
        
        /* Try fallback if main failed (index 0 usually means missing glyph) */
        /* Note: some fonts return 0 for space, but we handle space separately via advance */
        if (glyphs[i].index == 0 && g_fallbackFontLoaded && codepoint != ' ') {
            int fallbackIndex = stbtt_FindGlyphIndex(&g_fallbackFontInfo, codepoint);
            if (fallbackIndex != 0) {
                glyphs[i].index = fallbackIndex;
                glyphs[i].isFallback = TRUE;
            }
        }
        
        int advance, lsb;
        if (glyphs[i].isFallback) {
            stbtt_GetGlyphHMetrics(&g_fallbackFontInfo, glyphs[i].index, &advance, &lsb);
            advances[i] = (int)(advance * fallbackScale);
        } else {
            stbtt_GetGlyphHMetrics(&g_fontInfo, glyphs[i].index, &advance, &lsb);
            advances[i] = (int)(advance * scale);
        }
        
        totalWidth += advances[i];
        
        /* Kerning only applies if both glyphs are from main font */
        if (i < len - 1 && !glyphs[i].isFallback) {
            /* We don't check next glyph here, just optimistic kern lookup */
            /* Actually we should peek next. But for simplicity, only kern main font pairs */
             int nextCodepoint = (int)text[i+1];
             int nextIndex = stbtt_FindGlyphIndex(&g_fontInfo, nextCodepoint);
             if (nextIndex != 0) {
                 int kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, glyphs[i].index, nextIndex);
                 totalWidth += (int)(kern * scale);
             }
        }
    }

    /* Calculate starting position */
    int x = (width - totalWidth) / 2;
    int y = (height - (int)((ascent - descent) * scale)) / 2 + baselineOffset;
    
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    for (size_t i = 0; i < len; i++) {
        /* Skip rendering space or missing glyphs (if even fallback failed) */
        if (glyphs[i].index == 0 && text[i] != ' ') {
             /* Draw a box for missing glyph? optional. */
        }

        int w, h, xoff, yoff;
        unsigned char* bitmap = NULL;
        
        if (glyphs[i].isFallback) {
            bitmap = stbtt_GetGlyphBitmap(&g_fallbackFontInfo, fallbackScale, fallbackScale, glyphs[i].index, &w, &h, &xoff, &yoff);
        } else {
            bitmap = stbtt_GetGlyphBitmap(&g_fontInfo, scale, scale, glyphs[i].index, &w, &h, &xoff, &yoff);
        }
        
        if (bitmap) {
            BlendCharBitmap(bits, width, height, x + xoff, y + yoff, bitmap, w, h, r, g, b);
            stbtt_FreeBitmap(bitmap, NULL);
        }

        x += advances[i];
        
        /* Apply kerning again for position update */
        if (i < len - 1 && !glyphs[i].isFallback) {
             int nextCodepoint = (int)text[i+1];
             int nextIndex = stbtt_FindGlyphIndex(&g_fontInfo, nextCodepoint);
             if (nextIndex != 0) {
                 int kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, glyphs[i].index, nextIndex);
                 x += (int)(kern * scale);
             }
        }
    }

    free(glyphs);
    free(advances);
}
