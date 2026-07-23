        if (!CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
            continue;
        }

        DWORD* dest = pixels + (size_t)screenY * (size_t)destWidth +
                      (size_t)(rowX + (long long)firstI);
        const unsigned char* src = bitmap + (size_t)j * (size_t)w + (size_t)firstI;

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *src++;
            if (alpha == 0) {
                dest++;
                continue;
            }

            DWORD existing = *dest;
            int er = (existing >> 16) & 0xFF;
            int eg = (existing >> 8) & 0xFF;
            int eb = existing & 0xFF;
            int ea = (existing >> 24) & 0xFF;
            int nr = er + ((r - er) * alpha) / 255;
            int ng = eg + ((g - eg) * alpha) / 255;
            int nb = eb + ((b - eb) * alpha) / 255;
            int na = ea + ((255 - ea) * alpha) / 255;
            *dest = (na << 24) | (nr << 16) | (ng << 8) | nb;
            dest++;
        }
    }
}

/* Italic blend with gradient and per-row shear */
static void BlendCharBitmapItalicGradientSTB(void* destBits, int destWidth, int destHeight,
                                              int x_pos, int y_pos,
                                              const unsigned char* bitmap, int w, int h,
                                              float slant, const GradientInfo* info, int timeOffset, int totalWidth) {
    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !info || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int firstJ = 0;
    int lastJ = 0;
    if (!CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    long long animOffsetFixed = info->isAnimated
        ? ((long long)timeOffset * MARKDOWN_GRADIENT_FIXED_ONE) /
          (long long)(GRADIENT_LUT_SIZE * 2)
        : 0;
    long long gradientStep = MarkdownGradientStepFixed(totalWidth);

    for (int j = firstJ; j < lastJ; ++j) {
        int shear = (int)((h - j) * slant);
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
        if (!CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
            continue;
        }

        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = rowX + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownGradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = SampleGlobalGradientFixed(info, gradientPosition);
            AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
            int r = GetRValue(sample);
            int g = GetGValue(sample);
            int b = GetBValue(sample);

            DWORD finalR = (r * alpha) / 255;
            DWORD finalG = (g * alpha) / 255;
            DWORD finalB = (b * alpha) / 255;
            *destRow++ = (alpha << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }
}

/**
 * @brief Blend character bitmap with color tag gradient animation
 *
 * This function renders text with animated gradient using colors from <color:> tag.
 * The gradient flows horizontally across the text, similar to global gradient animation.
 *
 * @param destBits Destination bitmap buffer
 * @param destWidth Destination width
 * @param destHeight Destination height
 * @param x_pos X position
 * @param y_pos Y position
 * @param bitmap Glyph bitmap
 * @param w Glyph width
 * @param h Glyph height
 * @param colorTag Color tag with gradient colors
 * @param timeOffset Time offset for animation (from GetTickCount)
 * @param totalWidth Total width for gradient calculation
 */
static void BlendCharBitmapColorTagGradientSTB(void* destBits, int destWidth, int destHeight,
                                                int x_pos, int y_pos,
                                                const unsigned char* bitmap, int w, int h,
                                                const MarkdownColorTag* colorTag, int timeOffset, int totalWidth) {
    if (!colorTag || colorTag->colorCount < 2) return;

    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int colorCount = colorTag->colorCount;
    int firstI = 0;
    int lastI = 0;
    int firstJ = 0;
    int lastJ = 0;
    long long animOffsetFixed =
        ((long long)(timeOffset % 2000) * MARKDOWN_GRADIENT_FIXED_ONE) / 2000;
    long long gradientStep = MarkdownGradientStepFixed(totalWidth);
    if (!CalculateVisibleSpan(x_pos, w, destWidth, &firstI, &lastI) ||
        !CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = (long long)x_pos + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownGradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = SampleMarkdownGradientFixed(colorTag->colors,
                                                          colorCount,
                                                          gradientPosition);
            AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
            int r = GetRValue(sample);
            int g = GetGValue(sample);
            int b = GetBValue(sample);

            DWORD finalR = (r * alpha) / 255;
            DWORD finalG = (g * alpha) / 255;
            DWORD finalB = (b * alpha) / 255;
            *destRow++ = (alpha << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }
}

/**
 * @brief Blend character bitmap with color tag gradient animation (italic version)
 */
static void BlendCharBitmapColorTagGradientItalicSTB(void* destBits, int destWidth, int destHeight,
                                                      int x_pos, int y_pos,
                                                      const unsigned char* bitmap, int w, int h,
                                                      const MarkdownColorTag* colorTag, int timeOffset, int totalWidth,
                                                      float slant) {
    if (!colorTag || colorTag->colorCount < 2) return;

    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int colorCount = colorTag->colorCount;
    long long animOffsetFixed =
        ((long long)(timeOffset % 2000) * MARKDOWN_GRADIENT_FIXED_ONE) / 2000;
    long long gradientStep = MarkdownGradientStepFixed(totalWidth);
    int firstJ = 0;
    int lastJ = 0;
    if (!CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int shear = (int)((h - j) * slant);
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
        if (!CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
            continue;
        }

        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = rowX + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownGradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = SampleMarkdownGradientFixed(colorTag->colors,
                                                          colorCount,
                                                          gradientPosition);
            AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
            int r = GetRValue(sample);
            int g = GetGValue(sample);
            int b = GetBValue(sample);

            DWORD finalR = (r * alpha) / 255;
            DWORD finalG = (g * alpha) / 255;
            DWORD finalB = (b * alpha) / 255;
            *destRow++ = (alpha << 24) | (finalR << 16) | (finalG << 8) | finalB;
        }
    }
}

/* Public API */

BOOL MeasureMarkdownSTBScaled(const wchar_t* text,
                              const MarkdownHeading* headings, int headingCount,
                              const MarkdownFontTag* fontTags, int fontTagCount,
                              int fontSize, float fontScale,
                              int* width, int* height) {
    if (!BeginFontUseSTB()) return FALSE;
    BOOL result = FALSE;

    if (!IsFontLoadedSTB() || !text) goto done;
    if (!isfinite(fontScale) || fontScale <= 0.0f) fontScale = 1.0f;

    float scaledFontSize = (float)((double)fontSize * (double)fontScale);
    if (!isfinite(scaledFontSize) || scaledFontSize < 1.0f) scaledFontSize = 1.0f;

    const stbtt_fontinfo* fontInfo = GetMainFontInfoSTB();
    const stbtt_fontinfo* fallbackFontInfo = GetFallbackFontInfoSTB();
    BOOL fallbackLoaded = IsFallbackFontLoadedSTB();

    float baseScale = stbtt_ScaleForPixelHeight(fontInfo, scaledFontSize);
    float fallbackBaseScale = fallbackLoaded ? stbtt_ScaleForPixelHeight(fallbackFontInfo, scaledFontSize) : 0;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo, &ascent, &descent, &lineGap);
    int lineHeightMetric = ascent - descent + lineGap;

    int maxWidth = 0;
    int curLineWidth = 0;
    int totalHeight = 0;
    int curLineMaxHeight = GetLineHeightFromMetric(lineHeightMetric, baseScale); // Default to base height

    size_t len = wcslen(text);
