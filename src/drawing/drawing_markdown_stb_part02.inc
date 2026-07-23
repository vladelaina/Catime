    // Optimization: Track current range indexes
    int curHeadingIdx = 0;
    int curFontTagIdx = 0;
    int cachedFontTagIdx = -1;
    const stbtt_fontinfo* cachedFontTagInfo = NULL;
    float cachedFontTagScale = 0.0f;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            if (curLineWidth > maxWidth) maxWidth = curLineWidth;
            curLineWidth = 0;
            totalHeight = AddMarkdownIntClamped(totalHeight, curLineMaxHeight);
            curLineMaxHeight = GetLineHeightFromMetric(lineHeightMetric, baseScale); // Reset to base
            continue;
        }
        if (text[i] == L'\r') continue;

        // Skip horizontal rule markers (they span full width, don't affect max width)
        if (text[i] == L'\x2500') continue;

        // Determine style
        float scale = baseScale;
        float fallbackScale = fallbackBaseScale;

        // Check heading
        int charPos = ClampMarkdownPos(i);
        while (curHeadingIdx < headingCount && charPos >= headings[curHeadingIdx].endPos) {
            curHeadingIdx++;
        }
        if (curHeadingIdx < headingCount && charPos >= headings[curHeadingIdx].startPos) {
            scale = GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
            if (fallbackLoaded) {
                fallbackScale = GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
            }
        }

        const stbtt_fontinfo* charFontInfo = fontInfo;
        float charScale = scale;
        while (curFontTagIdx < fontTagCount && charPos >= fontTags[curFontTagIdx].endPos) {
            curFontTagIdx++;
        }
        if (curFontTagIdx < fontTagCount && charPos >= fontTags[curFontTagIdx].startPos) {
            if (cachedFontTagIdx != curFontTagIdx) {
                cachedFontTagIdx = curFontTagIdx;
                cachedFontTagInfo = GetCachedFontSTB(fontTags[curFontTagIdx].fontName);
                cachedFontTagScale = cachedFontTagInfo ?
                    stbtt_ScaleForPixelHeight(cachedFontTagInfo, scaledFontSize) :
                    0.0f;
            }
            if (cachedFontTagInfo) {
                charFontInfo = cachedFontTagInfo;
                charScale = cachedFontTagScale;
            }
        }

        // Update line height if this char is taller
        int h = GetLineHeightFromMetric(lineHeightMetric, scale);
        if (h > curLineMaxHeight) curLineMaxHeight = h;

        GlyphMetrics gm;
        if (charFontInfo != fontInfo) {
            if (!GetCachedFontCharMetricsSTB(charFontInfo, text[i], charScale, &gm) ||
                gm.index == 0) {
                GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
            }
        } else {
            GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
        }
        curLineWidth = AddMarkdownIntClamped(curLineWidth, gm.advance + gm.kern);
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;
    totalHeight = AddMarkdownIntClamped(totalHeight, curLineMaxHeight);

    if (width) *width = maxWidth;
    if (height) *height = totalHeight;
    result = TRUE;

done:
    EndFontUseSTB();
    return result;
}

BOOL MeasureMarkdownSTB(const wchar_t* text,
                        const MarkdownHeading* headings, int headingCount,
                        const MarkdownFontTag* fontTags, int fontTagCount,
                        int fontSize, int* width, int* height) {
    return MeasureMarkdownSTBScaled(text,
                                    headings, headingCount,
                                    fontTags, fontTagCount,
                                    fontSize, 1.0f,
                                    width, height);
}

/* Alert type colors (GitHub style) */
static const struct AlertColorInfo {
    BlockquoteAlertType type;
    COLORREF color;
} g_alertColors[] = {
    {BLOCKQUOTE_NOTE,      RGB(31, 136, 229)},  /* ⓘ Blue */
    {BLOCKQUOTE_TIP,       RGB(26, 127, 55)},   /* 💡 Green */
    {BLOCKQUOTE_IMPORTANT, RGB(130, 80, 223)},  /* ℹ Purple */
    {BLOCKQUOTE_WARNING,   RGB(191, 135, 0)},   /* ⚠ Yellow */
    {BLOCKQUOTE_CAUTION,   RGB(207, 34, 46)},   /* ⛔ Red */
};

static COLORREF GetAlertColor(BlockquoteAlertType type) {
    for (size_t i = 0; i < sizeof(g_alertColors) / sizeof(g_alertColors[0]); ++i) {
        if (g_alertColors[i].type == type) return g_alertColors[i].color;
    }
    return RGB(128, 128, 128);  /* Default gray for normal blockquote */
}

void RenderMarkdownSTB(void* bits, int width, int height, const wchar_t* text,
                       MarkdownLink* links, int linkCount,
                       const MarkdownHeading* headings, int headingCount,
                       MarkdownStyle* styles, int styleCount,
                       MarkdownBlockquote* blockquotes, int blockquoteCount,
                       MarkdownColorTag* colorTags, int colorTagCount,
                       const MarkdownFontTag* fontTags, int fontTagCount,
                       COLORREF color, int fontSize, float fontScale, int gradientMode,
                       const GradientInfo* gradientInfo) {
    int measuredTextWidth = 0;
    int measuredTextHeight = 0;
    if (!MeasureMarkdownSTBScaled(text, headings, headingCount, fontTags, fontTagCount,
                                  fontSize, fontScale,
                                  &measuredTextWidth, &measuredTextHeight)) {
        return;
    }

    RenderMarkdownSTBMeasured(bits, width, height, text,
                              links, linkCount,
                              headings, headingCount,
                              styles, styleCount,
                              blockquotes, blockquoteCount,
                              colorTags, colorTagCount,
                              fontTags, fontTagCount,
                              color, fontSize, fontScale, gradientMode,
                              gradientInfo,
                              measuredTextWidth, measuredTextHeight);
}

void RenderMarkdownSTBMeasured(void* bits, int width, int height, const wchar_t* text,
                               MarkdownLink* links, int linkCount,
                               const MarkdownHeading* headings, int headingCount,
                               MarkdownStyle* styles, int styleCount,
                               MarkdownBlockquote* blockquotes, int blockquoteCount,
                               MarkdownColorTag* colorTags, int colorTagCount,
                               const MarkdownFontTag* fontTags, int fontTagCount,
                               COLORREF color, int fontSize, float fontScale, int gradientMode,
                               const GradientInfo* gradientInfo,
                               int measuredTextWidth, int measuredTextHeight) {
    if (!BeginFontUseSTB()) return;

    if (!IsFontLoadedSTB() || !text || !bits) goto done;
    if (width <= 0 || height <= 0 ||
        (size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD)) goto done;
    if (measuredTextWidth < 0 || measuredTextHeight <= 0) goto done;

    GradientInfoSnapshot fallbackGradientSnapshot;
    const GradientInfo* frameGradientInfo = gradientInfo;
    if (!frameGradientInfo && gradientMode != GRADIENT_NONE &&
        GetGradientInfoSnapshot((GradientType)gradientMode, &fallbackGradientSnapshot)) {
        frameGradientInfo = &fallbackGradientSnapshot.info;
    }

    /* Clear previous clickable regions before rendering */
    ClearClickableRegions();

    const stbtt_fontinfo* fontInfo = GetMainFontInfoSTB();
    const stbtt_fontinfo* fallbackFontInfo = GetFallbackFontInfoSTB();
    BOOL fallbackLoaded = IsFallbackFontLoadedSTB();

    float baseScale = stbtt_ScaleForPixelHeight(fontInfo, (float)(fontSize * fontScale));
    float fallbackBaseScale = fallbackLoaded ? stbtt_ScaleForPixelHeight(fallbackFontInfo, (float)(fontSize * fontScale)) : 0;

    int baseAscent, baseDescent, baseLineGap;
    stbtt_GetFontVMetrics(fontInfo, &baseAscent, &baseDescent, &baseLineGap);
    int lineHeightMetric = baseAscent - baseDescent + baseLineGap;

    /* Checkbox tracking */
    int checkboxIndex = 0;

    int totalTextHeight = measuredTextHeight;
    int maxLineWidth = measuredTextWidth;

    int currentY = (height - totalTextHeight) / 2;
    int blockLeftX = (width - maxLineWidth) / 2;  // Left edge of centered text block

    size_t len = wcslen(text);
    size_t currentLineStart = 0;

    // State trackers
    int curHeadingIdx = 0;
    int curLinkIdx = 0;
    int curStyleIdx = 0;
    int curBlockquoteIdx = 0;
    int curColorTagIdx = 0;
    int curFontTagIdx = 0;
    int cachedFontTagIdx = -1;
    const stbtt_fontinfo* cachedFontTagInfo = NULL;
    float cachedFontTagScale = 0.0f;

    /* Calculate frame-local effect/timing state once instead of per glyph. */
    EffectType activeEffect = GetActiveEffect();
    DWORD frameTick = GetTickCount();
    int effectTimeOffset = (int)frameTick;
    int timeOffset = 0;

    /*
     * FIX: Use continuous time for Liquid Effect to prevent "popping/shrinking" artifacts.
     * Liquid simulation needs smooth monotonically increasing time (t), not a sawtooth wave.
     * For normal animated gradients, we can use the modulo cycle.
     */
    if (activeEffect == EFFECT_TYPE_LIQUID || activeEffect == EFFECT_TYPE_AQUA) {
        /* Raw continuous time for physics simulation */
        timeOffset = (int)frameTick;
    } else if (frameGradientInfo && frameGradientInfo->isAnimated) {
        /* Use a consistent cycle for gradients (2s loop for normal) */
        float progress = (float)(frameTick % 2000) / 2000.0f;
        timeOffset = (int)(progress * GRADIENT_LUT_SIZE * 2);
    } else if (colorTagCount > 0) {
        /* Color tag gradients also need time offset for animation */
        timeOffset = (int)frameTick;
    }

    DWORD globalStrikethroughLineColor =
        0xFF000000 | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
    if (frameGradientInfo) {
        if (frameGradientInfo->palette && frameGradientInfo->paletteCount > 0) {
            COLORREF c = frameGradientInfo->palette[0];
            globalStrikethroughLineColor =
                0xFF000000 | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
        } else {
            globalStrikethroughLineColor =
                0xFF000000 | (GetRValue(frameGradientInfo->startColor) << 16) |
                (GetGValue(frameGradientInfo->startColor) << 8) | GetBValue(frameGradientInfo->startColor);
        }
    }

    for (size_t i = 0; i <= len; i++) {
        if (text[i] == L'\n' || text[i] == L'\0') {
            // 1. Measure this line to center horizontally AND find max height
            int lineMaxHeight = GetLineHeightFromMetric(lineHeightMetric, baseScale);
            int maxAscent = (int)(baseAscent * baseScale);

            // Temp indices for measurement pass
            int tmpHeadingIdx = curHeadingIdx;

            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                float scale = baseScale;
                int lineCharPos = ClampMarkdownPos(j);

                while (tmpHeadingIdx < headingCount && lineCharPos >= headings[tmpHeadingIdx].endPos) tmpHeadingIdx++;
                if (tmpHeadingIdx < headingCount && lineCharPos >= headings[tmpHeadingIdx].startPos) {
                    scale = GetScaleForHeading(headings[tmpHeadingIdx].level, baseScale);
                }

                int h = GetLineHeightFromMetric(lineHeightMetric, scale);
