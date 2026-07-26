/**
 * @file drawing_text_stb_types.h
 * @brief Private data types shared by the STB text-rendering modules.
 */

#ifndef DRAWING_TEXT_STB_TYPES_H
#define DRAWING_TEXT_STB_TYPES_H

#include <stddef.h>

#include "drawing/drawing_text_stb.h"

#define LUT_SIZE GRADIENT_LUT_SIZE
#define GRADIENT_FIXED_ONE (1LL << 32)
#define FONT_TAG_GLYPH_METRICS_CACHE_SIZE 256
#define MAX_FAILED_FONT_CACHE 256
#define FONT_FAILURE_RETRY_MS 5000
#define MAX_MAPPED_FONT_BYTES (64ull * 1024ull * 1024ull)
#define MAIN_FONT_FILE_RECHECK_MS 1000u
#define GLYPH_METRICS_CACHE_SIZE 512
#define GLYPH_BITMAP_CACHE_SIZE 32
#define GLYPH_BITMAP_CACHE_MAX_BYTES (256u * 1024u)

typedef struct {
    BOOL valid;
    wchar_t c;
    int index;
    int advanceUnits;
} FontTagGlyphMetricsCacheEntry;

typedef struct {
    wchar_t fontName[MAX_PATH];
    DWORD retryAfterFailureTick;
} FailedFontCacheEntry;

typedef struct {
    BOOL valid;
    wchar_t c;
    wchar_t nextC;
    int index;
    BOOL isFallback;
    int advanceUnits;
    int kernUnits;
} GlyphMetricsCacheEntry;

typedef struct {
    BOOL valid;
    const stbtt_fontinfo* fontInfo;
    DWORD generation;
    int glyphIndex;
    DWORD scaleXBits;
    DWORD scaleYBits;
    int width;
    int height;
    int xoff;
    int yoff;
    size_t pixelCount;
    unsigned char* pixels;
    DWORD lastUse;
} GlyphBitmapCacheEntry;

typedef struct {
    int srcLeft;
    int srcTop;
    int srcRight;
    int srcBottom;
    int destLeft;
    int destTop;
} TextBitmapClip;

typedef struct {
    const GradientInfo* info;
    int startX;
    int totalWidth;
    int timeOffset;
    BOOL isAnimated;
    int startR;
    int startG;
    int startB;
    int endR;
    int endG;
    int endB;
} GlowGradientContext;

#endif /* DRAWING_TEXT_STB_TYPES_H */
