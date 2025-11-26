/**
 * @file drawing_text_stb.h
 * @brief High-quality text rendering using stb_truetype
 */

#ifndef DRAWING_TEXT_STB_H
#define DRAWING_TEXT_STB_H

#include <windows.h>
#include "../../libs/stb/stb_truetype.h"
#include "color/gradient.h"

/**
 * @brief Initialize STB Truetype with a font file
 * @param fontFilePath Absolute path to the .ttf/.ttc file
 * @return TRUE if successful
 */
BOOL InitFontSTB(const char* fontFilePath);

/**
 * @brief Cleanup STB resources
 */
void CleanupFontSTB(void);

/**
 * @brief Measure single-line text dimensions
 */
BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height);

/**
 * @brief Render single-line text to a 32-bit DIB buffer
 * @param bits Pointer to DIB section bits (ARGB)
 * @param width Width of the buffer
 * @param height Height of the buffer
 * @param text Text to render
 * @param color Text color (COLORREF)
 * @param fontSize Font size in pixels
 * @param fontScale DPI scale factor (usually 1.0 or system DPI scale)
 * @param editMode If TRUE, may render background hints
 */
void RenderTextSTB(void* bits, int width, int height, const wchar_t* text, 
                   COLORREF color, int fontSize, float fontScale, BOOL editMode);

/* ============================================================================
 * Internal API for Markdown Renderer (Shared Helpers)
 * ============================================================================ */

typedef struct {
    int index;
    BOOL isFallback;
    int advance;
    int kern;
} GlyphMetrics;

/* Accessors for Global Font State */
BOOL IsFontLoadedSTB(void);
BOOL IsFallbackFontLoadedSTB(void);
stbtt_fontinfo* GetMainFontInfoSTB(void);
stbtt_fontinfo* GetFallbackFontInfoSTB(void);

/* Shared Helper Functions */
void GetCharMetricsSTB(wchar_t c, wchar_t nextC, float scale, float fallbackScale, GlyphMetrics* out);

void BlendCharBitmapSTB(void* destBits, int destWidth, int destHeight, 
                        int x_pos, int y_pos, 
                        unsigned char* bitmap, int w, int h, 
                        int r, int g, int b);

void BlendCharBitmapGradientSTB(void* destBits, int destWidth, int destHeight, 
                                int x_pos, int y_pos, 
                                unsigned char* bitmap, int w, int h, 
                                int startX, int totalWidth, int gradientType);

#endif // DRAWING_TEXT_STB_H
