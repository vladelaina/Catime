/**
 * @file drawing_markdown_measure.c
 * @brief Measures Markdown text using STB font metrics.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include "drawing/drawing_text_stb.h"

#include <math.h>
#include <wchar.h>

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
    int curLineMaxHeight = MarkdownStbInternal_GetLineHeightFromMetric(lineHeightMetric, baseScale); // Default to base height

    size_t len = wcslen(text);
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
            totalHeight = MarkdownStbInternal_AddIntClamped(totalHeight, curLineMaxHeight);
            curLineMaxHeight = MarkdownStbInternal_GetLineHeightFromMetric(lineHeightMetric, baseScale); // Reset to base
            continue;
        }
        if (text[i] == L'\r') continue;

        // Skip horizontal rule markers (they span full width, don't affect max width)
        if (text[i] == L'\x2500') continue;

        // Determine style
        float scale = baseScale;
        float fallbackScale = fallbackBaseScale;

        // Check heading
        int charPos = MarkdownStbInternal_ClampPos(i);
        while (curHeadingIdx < headingCount && charPos >= headings[curHeadingIdx].endPos) {
            curHeadingIdx++;
        }
        if (curHeadingIdx < headingCount && charPos >= headings[curHeadingIdx].startPos) {
            scale = MarkdownStbInternal_GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
            if (fallbackLoaded) {
                fallbackScale = MarkdownStbInternal_GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
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
        int h = MarkdownStbInternal_GetLineHeightFromMetric(lineHeightMetric, scale);
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
        curLineWidth = MarkdownStbInternal_AddIntClamped(curLineWidth, gm.advance + gm.kern);
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;
    totalHeight = MarkdownStbInternal_AddIntClamped(totalHeight, curLineMaxHeight);

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
