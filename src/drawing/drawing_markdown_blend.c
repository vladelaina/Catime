/**
 * @file drawing_markdown_blend.c
 * @brief Blends solid and animated Markdown glyph bitmaps.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include <stddef.h>

void MarkdownStbInternal_BlendItalic(void* destBits, int destWidth, int destHeight,
                                      int x_pos, int y_pos,
                                      const unsigned char* bitmap, int w, int h,
                                      int r, int g, int b, float slant) {
    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int firstJ = 0;
    int lastJ = 0;
    if (!MarkdownStbInternal_CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int screenY = (int)((long long)y_pos + (long long)j);
        int shear = (int)((h - j) * slant);  // Top rows shift right more
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
        if (!MarkdownStbInternal_CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
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

void MarkdownStbInternal_BlendItalicGradient(void* destBits, int destWidth, int destHeight,
                                              int x_pos, int y_pos,
                                              const unsigned char* bitmap, int w, int h,
                                              float slant, const GradientInfo* info, int timeOffset, int totalWidth) {
    DWORD* pixels = (DWORD*)destBits;
    if (!pixels || !info || !bitmap || destWidth <= 0 || destHeight <= 0 || w <= 0 || h <= 0) return;

    int firstJ = 0;
    int lastJ = 0;
    if (!MarkdownStbInternal_CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    long long animOffsetFixed = info->isAnimated
        ? ((long long)timeOffset * MARKDOWN_GRADIENT_FIXED_ONE) /
          (long long)(GRADIENT_LUT_SIZE * 2)
        : 0;
    long long gradientStep = MarkdownStbInternal_GradientStepFixed(totalWidth);

    for (int j = firstJ; j < lastJ; ++j) {
        int shear = (int)((h - j) * slant);
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
        if (!MarkdownStbInternal_CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
            continue;
        }

        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = rowX + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownStbInternal_GradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = MarkdownStbInternal_SampleGlobalGradient(info, gradientPosition);
            MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
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

void MarkdownStbInternal_BlendColorTagGradient(void* destBits, int destWidth, int destHeight,
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
    long long gradientStep = MarkdownStbInternal_GradientStepFixed(totalWidth);
    if (!MarkdownStbInternal_CalculateVisibleSpan(x_pos, w, destWidth, &firstI, &lastI) ||
        !MarkdownStbInternal_CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = (long long)x_pos + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownStbInternal_GradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = MarkdownStbInternal_SampleGradient(colorTag->colors,
                                                          colorCount,
                                                          gradientPosition);
            MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
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

void MarkdownStbInternal_BlendColorTagGradientItalic(void* destBits, int destWidth, int destHeight,
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
    long long gradientStep = MarkdownStbInternal_GradientStepFixed(totalWidth);
    int firstJ = 0;
    int lastJ = 0;
    if (!MarkdownStbInternal_CalculateVisibleSpan(y_pos, h, destHeight, &firstJ, &lastJ)) {
        return;
    }

    for (int j = firstJ; j < lastJ; ++j) {
        int shear = (int)((h - j) * slant);
        long long rowX = (long long)x_pos + (long long)shear;
        int firstI = 0;
        int lastI = 0;
        if (!MarkdownStbInternal_CalculateVisibleSpan(rowX, w, destWidth, &firstI, &lastI)) {
            continue;
        }

        int screen_y = (int)((long long)y_pos + (long long)j);
        long long destX = rowX + (long long)firstI;
        DWORD* destRow = pixels + (size_t)screen_y * (size_t)destWidth + (size_t)destX;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)firstI;
        long long gradientPosition = MarkdownStbInternal_GradientPositionFixed(destX,
                                                                   totalWidth,
                                                                   animOffsetFixed);

        for (int i = firstI; i < lastI; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
                destRow++;
                continue;
            }

            COLORREF sample = MarkdownStbInternal_SampleGradient(colorTag->colors,
                                                          colorCount,
                                                          gradientPosition);
            MarkdownStbInternal_AdvanceGradientFixed(&gradientPosition, gradientStep);
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
