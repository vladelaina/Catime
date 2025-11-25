/**
 * @file drawing_text_stb.c
 * @brief Implementation of text rendering using stb_truetype
 */

#include "drawing/drawing_text_stb.h"
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

/* Memory mapping handles */
static HANDLE g_hFontFile = INVALID_HANDLE_VALUE;
static HANDLE g_hFontMapping = NULL;

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
    
    g_fontLoaded = FALSE;
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

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    
    int baselineOffset = (int)(ascent * scale);
    
    /* Calculate total text width to center it */
    int totalWidth = 0;
    size_t len = wcslen(text);
    int* glyphIndices = (int*)malloc(len * sizeof(int));
    int* advances = (int*)malloc(len * sizeof(int));
    int* lsbs = (int*)malloc(len * sizeof(int));
    
    if (!glyphIndices || !advances || !lsbs) {
        free(glyphIndices); free(advances); free(lsbs);
        return;
    }

    for (size_t i = 0; i < len; i++) {
        int codepoint = (int)text[i]; 
        glyphIndices[i] = stbtt_FindGlyphIndex(&g_fontInfo, codepoint);
        
        int advance, lsb;
        stbtt_GetGlyphHMetrics(&g_fontInfo, glyphIndices[i], &advance, &lsb);
        advances[i] = (int)(advance * scale);
        lsbs[i] = (int)(lsb * scale);
        
        totalWidth += advances[i];
        
        if (i < len - 1) {
            int kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, glyphIndices[i], glyphIndices[i+1]);
            totalWidth += (int)(kern * scale);
        }
    }

    /* Calculate starting position */
    int x = (width - totalWidth) / 2;
    int y = (height - (int)((ascent - descent) * scale)) / 2 + baselineOffset;
    
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    for (size_t i = 0; i < len; i++) {
        if (glyphIndices[i] == 0 && text[i] != ' ') {
            /* Glyph not found */
        }

        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetGlyphBitmap(&g_fontInfo, scale, scale, glyphIndices[i], &w, &h, &xoff, &yoff);
        
        if (bitmap) {
            BlendCharBitmap(bits, width, height, x + xoff, y + yoff, bitmap, w, h, r, g, b);
            stbtt_FreeBitmap(bitmap, NULL);
        }

        x += advances[i];
        if (i < len - 1) {
            int kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, glyphIndices[i], glyphIndices[i+1]);
            x += (int)(kern * scale);
        }
    }

    free(glyphIndices);
    free(advances);
    free(lsbs);
}
