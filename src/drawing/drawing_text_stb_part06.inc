                             int extraMargin) {
    if (!fontInfo || glyphIndex == 0 || destWidth <= 0 || destHeight <= 0) {
        return FALSE;
    }

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(fontInfo, glyphIndex, scaleX, scaleY, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0) {
        return FALSE;
    }

    int margin = extraMargin;
    if (effect != EFFECT_TYPE_NONE) {
        margin += 24;
    }

    int left = AddTextIntClamped(AddTextIntClamped(originX, x0), -margin);
    int top = AddTextIntClamped(AddTextIntClamped(originY, y0), -margin);
    int right = AddTextIntClamped(AddTextIntClamped(originX, x1), margin);
    int bottom = AddTextIntClamped(AddTextIntClamped(originY, y1), margin);

    return right > 0 && bottom > 0 && left < destWidth && top < destHeight;
}

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
        if (vertices) STBTT_free(vertices, fontInfo->userdata);
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
    STBTT_free(vertices, fontInfo->userdata);

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

BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height) {
    if (!BeginFontUseSTB()) return FALSE;
    BOOL result = FALSE;

    if (!g_fontLoaded || !text) goto done;

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
        curLineWidth = AddTextIntClamped(curLineWidth, gm.advance + gm.kern);
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = ScaleTextMetricClamped(ascent - descent + lineGap, scale);

    if (width) *width = maxWidth;
    if (height) *height = MulTextIntClamped(lineCount, lineHeight);
    result = TRUE;

done:
    EndFontUseSTB();
    return result;
}

void RenderTextSTB(void* bits, int width, int height, const wchar_t* text,
                   COLORREF color, int fontSize, float fontScale, BOOL editMode) {
    UNREFERENCED_PARAMETER(editMode);

    if (!BeginFontUseSTB()) return;
    if (!g_fontLoaded || !text || !bits) goto done;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)(fontSize * fontScale)) : 0;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = ScaleTextMetricClamped(ascent - descent + lineGap, scale);
    int baselineOffset = ScaleTextMetricClamped(ascent, scale);

    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    EffectType effect = GetActiveEffect();
    int timeOffset = (int)GetTickCount();

    // Pre-calculate line widths for centering
    // We can do a quick pass or re-use Measure logic per line
    size_t len = wcslen(text);
    int currentY = 0;

    // Calculate total text height to vertically center the whole block.
    // Width metrics are computed per line below, so only line count is needed here.
    int lineCount = 1;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            lineCount++;
        }
    }
    int totalTextHeight = MulTextIntClamped(lineCount, lineHeight);

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
                lineWidth = AddTextIntClamped(lineWidth, gm.advance + gm.kern);
            }

            int currentX = (width - lineWidth) / 2;
            int lineY = AddTextIntClamped(AddTextIntClamped(startY,
                                                            MulTextIntClamped(currentY, lineHeight)),
                                          baselineOffset);

            // Render line
