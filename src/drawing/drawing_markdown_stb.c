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
#include "drawing_markdown_stb_part01.inc"
#include "drawing_markdown_stb_part02.inc"
#include "drawing_markdown_stb_part03.inc"
#include "drawing_markdown_stb_part04.inc"
