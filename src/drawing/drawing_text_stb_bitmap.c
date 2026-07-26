/**
 * @file drawing_text_stb_bitmap.c
 * @brief Visible glyph bitmap creation and bitmap-cache integration.
 */

#include "drawing_text_stb_internal.h"

unsigned char* CreateVisibleGlyphBitmapSTB(const stbtt_fontinfo* fontInfo,
                                           int glyphIndex,
                                           float scaleX,
                                           float scaleY,
                                           int originX,
                                           int originY,
                                           int destWidth,
                                           int destHeight,
                                           int extraMargin,
                                           int* width,
                                           int* height,
                                           int* xoff,
                                           int* yoff) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (xoff) *xoff = 0;
    if (yoff) *yoff = 0;

    if (!fontInfo || glyphIndex == 0 || destWidth <= 0 || destHeight <= 0 ||
        !isfinite(scaleX) || !isfinite(scaleY) || scaleX <= 0.0f || scaleY <= 0.0f) {
        return NULL;
    }

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(fontInfo, glyphIndex, scaleX, scaleY, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0) {
        return NULL;
    }

    if (extraMargin < 0) extraMargin = 0;

    long long glyphLeft = (long long)originX + (long long)x0;
    long long glyphTop = (long long)originY + (long long)y0;
    long long glyphRight = (long long)originX + (long long)x1;
    long long glyphBottom = (long long)originY + (long long)y1;
    long long clipLeft = glyphLeft < -(long long)extraMargin ? -(long long)extraMargin : glyphLeft;
    long long clipTop = glyphTop < -(long long)extraMargin ? -(long long)extraMargin : glyphTop;
    long long clipRightLimit = (long long)destWidth + (long long)extraMargin;
    long long clipBottomLimit = (long long)destHeight + (long long)extraMargin;
    long long clipRight = glyphRight > clipRightLimit ? clipRightLimit : glyphRight;
    long long clipBottom = glyphBottom > clipBottomLimit ? clipBottomLimit : glyphBottom;

    if (clipLeft >= clipRight || clipTop >= clipBottom) {
        return NULL;
    }

    long long srcLeft = clipLeft - glyphLeft;
    long long srcTop = clipTop - glyphTop;
    long long outW64 = clipRight - clipLeft;
    long long outH64 = clipBottom - clipTop;
    long long xoff64 = (long long)x0 + srcLeft;
    long long yoff64 = (long long)y0 + srcTop;

    if (outW64 > (long long)INT_MAX || outH64 > (long long)INT_MAX ||
        xoff64 < (long long)INT_MIN || xoff64 > (long long)INT_MAX ||
        yoff64 < (long long)INT_MIN || yoff64 > (long long)INT_MAX) {
        return NULL;
    }

    int outW = (int)outW64;
    int outH = (int)outH64;
    int outXoff = (int)xoff64;
    int outYoff = (int)yoff64;
    size_t pixelCount = 0;
    if (!CalculateBitmapPixelCount(outW, outH, &pixelCount)) {
        return NULL;
    }

    BOOL cacheable = (fontInfo == &g_fontInfo || fontInfo == &g_fallbackFontInfo);
    DWORD fontGeneration = cacheable ? GetFontStateGenerationSTB() : 0;
    DWORD scaleXBits = cacheable ? FloatBitsForGlyphCache(scaleX) : 0;
    DWORD scaleYBits = cacheable ? FloatBitsForGlyphCache(scaleY) : 0;
    if (cacheable) {
        unsigned char* cachedBitmap = CopyCachedGlyphBitmapLocked(fontInfo,
                                                                  fontGeneration,
                                                                  glyphIndex,
                                                                  scaleXBits,
                                                                  scaleYBits,
                                                                  outW,
                                                                  outH,
                                                                  outXoff,
                                                                  outYoff,
                                                                  pixelCount);
        if (cachedBitmap) {
            if (width) *width = outW;
            if (height) *height = outH;
            if (xoff) *xoff = outXoff;
            if (yoff) *yoff = outYoff;
            return cachedBitmap;
        }
    }

    unsigned char* bitmap = (unsigned char*)malloc(pixelCount);
    if (!bitmap) {
        return NULL;
    }
    memset(bitmap, 0, pixelCount);

    stbtt_vertex* vertices = NULL;
    int numVerts = stbtt_GetGlyphShape(fontInfo, glyphIndex, &vertices);
    if (!vertices || numVerts <= 0) {
        if (vertices) stbtt_FreeShape(fontInfo, vertices);
        free(bitmap);
        return NULL;
    }

    stbtt__bitmap gbm;
    gbm.w = outW;
    gbm.h = outH;
    gbm.stride = outW;
    gbm.pixels = bitmap;
    stbtt_Rasterize(&gbm, 0.35f, vertices, numVerts,
                    scaleX, scaleY, 0.0f, 0.0f,
                    outXoff, outYoff, 1, fontInfo->userdata);
    stbtt_FreeShape(fontInfo, vertices);

    if (cacheable) {
        StoreGlyphBitmapCacheLocked(fontInfo,
                                    fontGeneration,
                                    glyphIndex,
                                    scaleXBits,
                                    scaleYBits,
                                    outW,
                                    outH,
                                    outXoff,
                                    outYoff,
                                    bitmap,
                                    pixelCount);
    }

    if (width) *width = outW;
    if (height) *height = outH;
    if (xoff) *xoff = outXoff;
    if (yoff) *yoff = outYoff;
    return bitmap;
}
