/**
 * @file drawing_markdown_stb.c
 * @brief Implementation of Markdown rendering logic
 */

#include "drawing/drawing_markdown_stb.h"
#include "drawing/drawing_text_stb.h"
#include "menu_preview.h"
#include "markdown/markdown_parser.h"
#include "markdown/markdown_interactive.h"
#include "color/gradient.h"
#include "log.h"
#include <limits.h>
#include <math.h>

/* Helper Functions */

static float GetScaleForHeading(int level, float baseScale) {
    switch (level) {
        case 1: return baseScale * 1.5f;
        case 2: return baseScale * 1.35f;
        case 3: return baseScale * 1.2f;
        case 4: return baseScale * 1.1f;
        case 5: return baseScale * 1.0f;
        case 6: return baseScale * 0.9f;
        default: return baseScale;
    }
}

static int GetLineHeightFromMetric(int fontMetricHeight, float scale) {
    double scaled = (double)fontMetricHeight * (double)scale;
    if (fontMetricHeight <= 0 || scale <= 0.0f || !isfinite(scaled)) return 0;
    if (scaled > (double)INT_MAX) return INT_MAX;
    return (int)scaled;
}

static int ClampMarkdownPos(size_t pos) {
    return (pos > (size_t)INT_MAX) ? INT_MAX : (int)pos;
}

static int ClampMarkdownInt64(long long value) {
    if (value > (long long)INT_MAX) return INT_MAX;
    if (value < (long long)INT_MIN) return INT_MIN;
    return (int)value;
}

static int AddMarkdownIntClamped(int value, int delta) {
    return ClampMarkdownInt64((long long)value + (long long)delta);
}

static BOOL CalculateVisibleSpan(long long start, int length, int limit,
                                 int* outFirst, int* outLast) {
    if (!outFirst || !outLast || length <= 0 || limit <= 0) return FALSE;

    long long spanStart = start;
    long long spanEnd = spanStart + (long long)length;
    if (spanEnd <= 0 || spanStart >= (long long)limit) return FALSE;

    long long first = (spanStart < 0) ? -spanStart : 0;
    long long last = (spanEnd > (long long)limit)
        ? ((long long)limit - spanStart)
        : (long long)length;

    if (first < 0 || last < first || first > (long long)INT_MAX || last > (long long)INT_MAX) {
        return FALSE;
    }

    *outFirst = (int)first;
    *outLast = (int)last;
    return first < last;
}

static BOOL StartsWithLiteralW(const wchar_t* text, const wchar_t* prefix) {
    if (!text || !prefix) return FALSE;
    while (*prefix) {
        if (*text++ != *prefix++) {
            return FALSE;
        }
    }
    return TRUE;
}

#define MARKDOWN_GRADIENT_FIXED_ONE (1LL << 32)

static long long WrapMarkdownGradientFixed(long long position) {
    position %= MARKDOWN_GRADIENT_FIXED_ONE;
    if (position < 0) {
        position += MARKDOWN_GRADIENT_FIXED_ONE;
    }
    return position;
}

static long long MarkdownGradientStepFixed(int totalWidth) {
    return (totalWidth > 0)
        ? (MARKDOWN_GRADIENT_FIXED_ONE / (long long)totalWidth)
        : 0;
}

static long long MarkdownGradientPositionFixed(long long screenX,
                                               int totalWidth,
                                               long long offsetFixed) {
    long long position = 0;
    if (totalWidth > 0) {
        position = ((long long)screenX * MARKDOWN_GRADIENT_FIXED_ONE) /
                   (long long)totalWidth;
    }
    return WrapMarkdownGradientFixed(position - offsetFixed);
}

static void AdvanceMarkdownGradientFixed(long long* position, long long step) {
    if (!position || step == 0) return;
    *position = WrapMarkdownGradientFixed(*position + step);
}

static int InterpolateMarkdownChannelFixed(int from, int to, long long frac) {
    if (frac <= 0) return from;
    if (frac >= MARKDOWN_GRADIENT_FIXED_ONE) return to;

    long long weighted = (long long)from * (MARKDOWN_GRADIENT_FIXED_ONE - frac) +
                         (long long)to * frac;
    return (int)(weighted / MARKDOWN_GRADIENT_FIXED_ONE);
}

static COLORREF SampleMarkdownGradientFixed(const COLORREF* colors,
                                            int colorCount,
                                            long long position) {
    if (!colors || colorCount <= 0) return RGB(0, 0, 0);
    if (colorCount == 1) return colors[0];

    int segmentCount = colorCount - 1;
    long long scaled = position * (long long)segmentCount;
    int idx1 = (int)(scaled / MARKDOWN_GRADIENT_FIXED_ONE);
    long long frac = scaled % MARKDOWN_GRADIENT_FIXED_ONE;

    if (idx1 >= segmentCount) {
        idx1 = segmentCount - 1;
        frac = MARKDOWN_GRADIENT_FIXED_ONE - 1;
    } else if (idx1 < 0) {
        idx1 = 0;
        frac = 0;
    }

    COLORREF c1 = colors[idx1];
    COLORREF c2 = colors[idx1 + 1];
    int r = InterpolateMarkdownChannelFixed(GetRValue(c1), GetRValue(c2), frac);
    int g = InterpolateMarkdownChannelFixed(GetGValue(c1), GetGValue(c2), frac);
    int b = InterpolateMarkdownChannelFixed(GetBValue(c1), GetBValue(c2), frac);
    return RGB(r, g, b);
}

static COLORREF SampleGlobalGradientFixed(const GradientInfo* info,
                                          long long position) {
    if (!info) return RGB(0, 0, 0);

    if (info->palette && info->paletteCount > 2) {
        return SampleMarkdownGradientFixed(info->palette, info->paletteCount, position);
    }

    int r = InterpolateMarkdownChannelFixed(GetRValue(info->startColor),
                                            GetRValue(info->endColor),
                                            position);
    int g = InterpolateMarkdownChannelFixed(GetGValue(info->startColor),
                                            GetGValue(info->endColor),
                                            position);
    int b = InterpolateMarkdownChannelFixed(GetBValue(info->startColor),
                                            GetBValue(info->endColor),
                                            position);
    return RGB(r, g, b);
}

/* Italic blend with per-row shear */
static void BlendCharBitmapItalicSTB(void* destBits, int destWidth, int destHeight,
                                      int x_pos, int y_pos,
                                      const unsigned char* bitmap, int w, int h,
                                      int r, int g, int b, float slant) {
    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int firstJ = 0;
    int lastJ = 0;
    if (!CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int screenY = (int)((long long)y_pos + (long long)j);
        int shear = (int)((h - j) * slant);  // Top rows shift right more
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
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
    if (activeEffect == EFFECT_TYPE_LIQUID) {
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
                if (h > lineMaxHeight) lineMaxHeight = h;

                int asc = GetLineHeightFromMetric(baseAscent, scale); // Approximation
                if (asc > maxAscent) maxAscent = asc;
            }

            int currentX = blockLeftX;  // All lines start from same left edge
            // Baseline align: Y + maxAscent
            int baselineY = AddMarkdownIntClamped(currentY, maxAscent);

            // Check for horizontal rule (─── = \x2500\x2500\x2500)
            if (i - currentLineStart >= 3 && 
                text[currentLineStart] == L'\x2500' && 
                text[currentLineStart + 1] == L'\x2500' && 
                text[currentLineStart + 2] == L'\x2500') {
                // Draw horizontal line within text block area only
                long long lineY64 = (long long)currentY + (long long)(lineMaxHeight / 2);
                DWORD* pixels = (DWORD*)bits;
                
                long long hrLeft = (long long)blockLeftX;
                int hrWidth = maxLineWidth;

                if (lineY64 >= 0 && lineY64 < (long long)height && hrWidth > 0) {
                    int lineY = (int)lineY64;
                    int drawStart = 0;
                    int drawEnd = 0;
                    if (CalculateVisibleSpan(hrLeft, hrWidth, width, &drawStart, &drawEnd)) {
                        const GradientInfo* hrGradInfo = frameGradientInfo;
                        long long animOffsetFixed = (hrGradInfo &&
                                                     hrGradInfo->palette &&
                                                     hrGradInfo->paletteCount > 2)
                            ? ((long long)timeOffset * MARKDOWN_GRADIENT_FIXED_ONE) /
                              (long long)(GRADIENT_LUT_SIZE * 2)
                            : 0;
                        long long gradientStep = MarkdownGradientStepFixed(hrWidth);
                        long long gradientPosition = MarkdownGradientPositionFixed(drawStart,
                                                                                   hrWidth,
                                                                                   animOffsetFixed);
                        DWORD* row = pixels + (size_t)lineY * (size_t)width;

                        for (int offset = drawStart; offset < drawEnd; offset++) {
                            int x = (int)(hrLeft + (long long)offset);
                            DWORD lineColor;
                            if (hrGradInfo) {
                                COLORREF sample = SampleGlobalGradientFixed(hrGradInfo, gradientPosition);
                                lineColor = 0xFF000000 |
                                            (GetRValue(sample) << 16) |
                                            (GetGValue(sample) << 8) |
                                            GetBValue(sample);
                                AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
                            } else {
                                lineColor = 0xFF000000 |
                                            (GetRValue(color) << 16) |
                                            (GetGValue(color) << 8) |
                                            GetBValue(color);
                            }
                            row[x] = lineColor;
                        }
                    }
                }
                currentY = AddMarkdownIntClamped(currentY, lineMaxHeight);
                currentLineStart = i + 1;
                continue;
            }

            // Check if this line is inside a blockquote
            int currentLineStartPos = ClampMarkdownPos(currentLineStart);
            while (curBlockquoteIdx < blockquoteCount &&
                   currentLineStartPos >= blockquotes[curBlockquoteIdx].endPos) {
                curBlockquoteIdx++;
            }
            
            BlockquoteAlertType activeAlertType = BLOCKQUOTE_NORMAL;
            BOOL inBlockquote = FALSE;
            if (curBlockquoteIdx < blockquoteCount && 
                currentLineStartPos >= blockquotes[curBlockquoteIdx].startPos) {
                inBlockquote = TRUE;
                activeAlertType = blockquotes[curBlockquoteIdx].alertType;
            }
            
            // Draw left colored bar for alert blockquotes only (GitHub style)
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                COLORREF barColor = GetAlertColor(activeAlertType);
                DWORD barColorDW = 0xFF000000 | (GetRValue(barColor) << 16) | 
                                   (GetGValue(barColor) << 8) | GetBValue(barColor);
                DWORD* pixels = (DWORD*)bits;
                
                int barX = blockLeftX - 8;  // 8 pixels left of text
                int barWidth = 3;           // 3 pixels wide

                int firstY = 0;
                int lastY = 0;
                int firstX = 0;
                int lastX = 0;
                if (CalculateVisibleSpan(currentY, lineMaxHeight, height, &firstY, &lastY) &&
                    CalculateVisibleSpan(barX, barWidth, width, &firstX, &lastX)) {
                    for (int yOffset = firstY; yOffset < lastY; yOffset++) {
                        int y = (int)((long long)currentY + (long long)yOffset);
                        DWORD* row = pixels + (size_t)y * (size_t)width;
                        for (int xOffset = firstX; xOffset < lastX; xOffset++) {
                            int x = (int)((long long)barX + (long long)xOffset);
                            row[x] = barColorDW;
                        }
                    }
                }
            }
            
            // Check if this is an alert title line (first line with "NOTE:", etc.)
            BOOL isAlertTitleLine = FALSE;
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                const wchar_t* lineText = &text[currentLineStart];
                if (StartsWithLiteralW(lineText, L"NOTE:") ||
                    StartsWithLiteralW(lineText, L"TIP:") ||
                    StartsWithLiteralW(lineText, L"IMPORTANT:") ||
                    StartsWithLiteralW(lineText, L"WARNING:") ||
                    StartsWithLiteralW(lineText, L"CAUTION:")) {
                    isAlertTitleLine = TRUE;
                }
            }
            
            // Check if this is a completed todo line (starts with ■)
            BOOL isCompletedTodo = (text[currentLineStart] == L'\x25A0');
            
            // 2. Render this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                // Determine styles
                float scale = baseScale;
                float fallbackScale = fallbackBaseScale;
                COLORREF drawColor = color;
                int renderPos = ClampMarkdownPos(j);

                // Heading
                while (curHeadingIdx < headingCount && renderPos >= headings[curHeadingIdx].endPos) curHeadingIdx++;
                if (curHeadingIdx < headingCount && renderPos >= headings[curHeadingIdx].startPos) {
                    scale = GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
                    if (fallbackLoaded) fallbackScale = GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
                }
                
                // Apply alert color only to title line (NOTE:, TIP:, etc.)
                if (isAlertTitleLine) {
                    drawColor = GetAlertColor(activeAlertType);
                }

                // Link - track region for click detection
                BOOL inLink = FALSE;
                int activeLinkIdx = -1;
                while (curLinkIdx < linkCount && renderPos >= links[curLinkIdx].endPos) curLinkIdx++;
                if (curLinkIdx < linkCount && renderPos >= links[curLinkIdx].startPos) {
                    drawColor = RGB(0, 175, 255); // Link color #00AFFF
                    inLink = TRUE;
                    activeLinkIdx = curLinkIdx;
                    
                    /* Update link rect for first char */
                    if (renderPos == links[curLinkIdx].startPos) {
                        links[curLinkIdx].linkRect.left = currentX;
                        links[curLinkIdx].linkRect.top = currentY;
                        links[curLinkIdx].linkRect.bottom = AddMarkdownIntClamped(currentY, lineMaxHeight);
                    }
                    links[curLinkIdx].linkRect.right = currentX;
                }
                
                // Style handling
                BOOL isBold = isAlertTitleLine;  // Alert title is bold
                BOOL isItalic = FALSE;
                BOOL isStrikethrough = FALSE;
                
                // Apply strikethrough for completed todo (skip checkbox symbol itself)
                if (isCompletedTodo && renderPos > currentLineStartPos) {
                    isStrikethrough = TRUE;
                }
                
                while (curStyleIdx < styleCount && renderPos >= styles[curStyleIdx].endPos) curStyleIdx++;
                if (curStyleIdx < styleCount && renderPos >= styles[curStyleIdx].startPos) {
                    MarkdownStyleType styleType = styles[curStyleIdx].type;
                    if (styleType == STYLE_CODE) {
                        drawColor = RGB(100, 100, 100);
                    } else if (styleType == STYLE_BOLD) {
                        isBold = TRUE;
                    } else if (styleType == STYLE_ITALIC) {
                        isItalic = TRUE;
                    } else if (styleType == STYLE_BOLD_ITALIC) {
                        isBold = TRUE;
                        isItalic = TRUE;
                    } else if (styleType == STYLE_STRIKETHROUGH) {
                        isStrikethrough = TRUE;
                    }
                }

                /* Color tag handling - override drawColor with tag color or gradient */
                BOOL useColorTagGradient = FALSE;
                const MarkdownColorTag* activeColorTag = NULL;
                while (curColorTagIdx < colorTagCount && j >= (size_t)colorTags[curColorTagIdx].endPos) curColorTagIdx++;
                if (curColorTagIdx < colorTagCount && j >= (size_t)colorTags[curColorTagIdx].startPos) {
                    const MarkdownColorTag* tag = &colorTags[curColorTagIdx];
                    if (tag->colorCount == 1) {
                        /* Single color */
                        drawColor = tag->colors[0];
                    } else if (tag->colorCount > 1) {
                        /* Gradient - will use animated rendering */
                        useColorTagGradient = TRUE;
                        activeColorTag = tag;
                    }
                }

                /* Font tag handling - use cached font if specified */
                const stbtt_fontinfo* charFontInfo = fontInfo;
                float charScale = scale;
                while (curFontTagIdx < fontTagCount && j >= (size_t)fontTags[curFontTagIdx].endPos) curFontTagIdx++;
                if (curFontTagIdx < fontTagCount && j >= (size_t)fontTags[curFontTagIdx].startPos) {
                    if (cachedFontTagIdx != curFontTagIdx) {
                        cachedFontTagIdx = curFontTagIdx;
                        cachedFontTagInfo = GetCachedFontSTB(fontTags[curFontTagIdx].fontName);
                        cachedFontTagScale = cachedFontTagInfo ?
                            stbtt_ScaleForPixelHeight(cachedFontTagInfo, (float)(fontSize * fontScale)) :
                            0.0f;
                    }
                    if (cachedFontTagInfo) {
                        charFontInfo = cachedFontTagInfo;
                        charScale = cachedFontTagScale;
                    }
                }

                GlyphMetrics gm;
                /* Use custom font if in font tag, otherwise use default */
                if (charFontInfo != fontInfo) {
                    if (!GetCachedFontCharMetricsSTB(charFontInfo, text[j], charScale, &gm) ||
                        gm.index == 0) {
                        /* Fallback to main font if glyph not found */
                        GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                        charFontInfo = fontInfo;
                        charScale = scale;
                    }
                } else {
                    GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                }
                
                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;

                    const stbtt_fontinfo* glyphFontInfo = fontInfo;
                    float glyphScale = scale;
                    if (charFontInfo != fontInfo && !gm.isFallback) {
                        glyphFontInfo = charFontInfo;
                        glyphScale = charScale;
                    } else if (gm.isFallback) {
                        glyphFontInfo = fallbackFontInfo;
                        glyphScale = fallbackScale;
                    }

                    int visibilityMargin = isItalic ? lineMaxHeight : 0;
                    if (activeEffect != EFFECT_TYPE_NONE) {
                        visibilityMargin = AddMarkdownIntClamped(visibilityMargin, 24);
                    }
                    bitmap = CreateVisibleGlyphBitmapSTB(glyphFontInfo, gm.index,
                                                         glyphScale, glyphScale,
                                                         currentX, baselineY,
                                                         width, height,
                                                         visibilityMargin,
                                                         &w, &h, &xoff, &yoff);
                    
                    if (bitmap) {
                        float slant = isItalic ? 0.35f : 0.0f;
                        int drawR = GetRValue(drawColor);
                        int drawG = GetGValue(drawColor);
                        int drawB = GetBValue(drawColor);
                        int glyphX = AddMarkdownIntClamped(currentX, xoff);
                        int glyphY = AddMarkdownIntClamped(baselineY, yoff);
                        int glyphXBold = AddMarkdownIntClamped(glyphX, 1);
                        int glyphYBold = AddMarkdownIntClamped(glyphY, 1);

                        /* Use global gradient only if no color tag gradient is active */
                        BOOL useGlobalGradient = (frameGradientInfo && drawColor == color && !useColorTagGradient);

                        if (useGlobalGradient) {
                            if (isItalic) {
                                // Use proper per-row shear for italic with gradient
                                BlendCharBitmapItalicGradientSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, slant, frameGradientInfo, timeOffset, width);
                                if (isBold) {
                                    BlendCharBitmapItalicGradientSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, slant, frameGradientInfo, timeOffset, width);
                                }
                            } else {
                                BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                    activeEffect);
                                if (isBold) {
                                    BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                        activeEffect);
                                    BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                        glyphX, glyphYBold,
                                        bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                        activeEffect);
                                }
                            }
                        } else if (useColorTagGradient && activeColorTag) {
                            /* Color tag gradient with animation */
                            if (isItalic) {
                                BlendCharBitmapColorTagGradientItalicSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, activeColorTag, timeOffset, width, slant);
                                if (isBold) {
                                    BlendCharBitmapColorTagGradientItalicSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, activeColorTag, timeOffset, width, slant);
                                }
                            } else {
                                BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, activeColorTag, timeOffset, width);
                                if (isBold) {
                                    BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, activeColorTag, timeOffset, width);
                                    BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                        glyphX, glyphYBold,
                                        bitmap, w, h, activeColorTag, timeOffset, width);
                                }
                            }
                        } else if (isItalic) {
                            // Use proper per-row shear for italic
                            BlendCharBitmapItalicSTB(bits, width, height,
                                glyphX, glyphY,
                                bitmap, w, h,
                                drawR, drawG, drawB, slant);
                            if (isBold) {
                                BlendCharBitmapItalicSTB(bits, width, height,
                                    glyphXBold, glyphY,
                                    bitmap, w, h,
                                    drawR, drawG, drawB, slant);
                            }
                        } else {
                            BlendCharBitmapSTBWithEffect(bits, width, height,
                                glyphX, glyphY,
                                bitmap, w, h,
                                drawR, drawG, drawB,
                                activeEffect, effectTimeOffset);
                            if (isBold) {
                                BlendCharBitmapSTBWithEffect(bits, width, height,
                                    glyphXBold, glyphY,
                                    bitmap, w, h,
                                    drawR, drawG, drawB,
                                    activeEffect, effectTimeOffset);
                                BlendCharBitmapSTBWithEffect(bits, width, height,
                                    glyphX, glyphYBold,
                                    bitmap, w, h,
                                    drawR, drawG, drawB,
                                    activeEffect, effectTimeOffset);
                            }
                        }
                        stbtt_FreeBitmap(bitmap, NULL);
                        
                        // Draw strikethrough line
                        if (isStrikethrough) {
                            int lineY = AddMarkdownIntClamped(baselineY, -(h / 3));  // Position at ~1/3 from baseline
                            DWORD* pixels = (DWORD*)bits;
                            
                            // Get line color from gradient or solid color
                            DWORD lineColor;
                            if (gradientMode != GRADIENT_NONE && drawColor == color) {
                                lineColor = globalStrikethroughLineColor;
                            } else {
                                lineColor = 0xFF000000 | (drawR << 16) | (drawG << 8) | drawB;
                            }
                            
                            // Draw horizontal line through character
                            if (lineY >= 0 && lineY < height && gm.advance > 0) {
                                int firstStrikeX = 0;
                                int lastStrikeX = 0;
                                if (CalculateVisibleSpan(currentX, gm.advance, width,
                                                         &firstStrikeX, &lastStrikeX)) {
                                    DWORD* row = pixels + (size_t)lineY * (size_t)width;
                                    for (int sx = firstStrikeX; sx < lastStrikeX; sx++) {
                                        int x = (int)((long long)currentX + (long long)sx);
                                        row[x] = lineColor;
                                    }
                                }
                            }
                        }
                    }
                    /* Record checkbox region (□ = 0x25A1, ■ = 0x25A0) */
                    if (text[j] == L'\x25A1' || text[j] == L'\x25A0') {
                        /* Use character advance width for click area */
                        RECT cbRect = {
                            currentX, currentY,
                            AddMarkdownIntClamped(currentX, gm.advance),
                            AddMarkdownIntClamped(currentY, lineMaxHeight)
                        };
                        AddCheckboxRegion(&cbRect, checkboxIndex, text[j] == L'\x25A0');
                        checkboxIndex++;
                    }
                }
                currentX = AddMarkdownIntClamped(currentX, gm.advance + gm.kern);
                
                /* Update link rect right edge after advancing */
                if (inLink && activeLinkIdx >= 0) {
                    links[activeLinkIdx].linkRect.right = currentX;
                }
            }

            currentY = AddMarkdownIntClamped(currentY, lineMaxHeight);
            currentLineStart = i + 1;
        }
    }
    
    /* Register all link regions for click detection */
    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkUrl && links[i].linkRect.right > links[i].linkRect.left) {
            AddLinkRegion(&links[i].linkRect, links[i].linkUrl);
        }
    }

done:
    EndFontUseSTB();
}
