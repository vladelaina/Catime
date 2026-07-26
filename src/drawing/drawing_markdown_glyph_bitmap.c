/**
 * @file drawing_markdown_glyph_bitmap.c
 * @brief Rasterizes and composites one prepared Markdown glyph.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include "drawing/drawing_text_stb.h"
#include "markdown/markdown_interactive.h"

void MarkdownStbInternal_DrawGlyph(
    MarkdownRenderContext* context, const MarkdownLineState* line,
    size_t index, int currentX, int baselineY,
    const MarkdownGlyphState* glyph) {
    if (!context || !line || !glyph) return;
    if (glyph->metrics.index == 0 ||
        context->text[index] == L' ' || context->text[index] == L'\t') {
        return;
    }

    int w = 0;
    int h = 0;
    int xoff = 0;
    int yoff = 0;
    const stbtt_fontinfo* glyphFontInfo = context->fontInfo;
    float glyphScale = glyph->scale;
    if (glyph->charFontInfo != context->fontInfo &&
        !glyph->metrics.isFallback) {
        glyphFontInfo = glyph->charFontInfo;
        glyphScale = glyph->charScale;
    } else if (glyph->metrics.isFallback) {
        glyphFontInfo = context->fallbackFontInfo;
        glyphScale = glyph->fallbackScale;
    }

    int visibilityMargin = glyph->isItalic ? line->lineMaxHeight : 0;
    if (context->activeEffect != EFFECT_TYPE_NONE) {
        visibilityMargin = MarkdownStbInternal_AddIntClamped(
            visibilityMargin, 24);
    }
    unsigned char* bitmap = CreateVisibleGlyphBitmapSTB(
        glyphFontInfo, glyph->metrics.index, glyphScale, glyphScale,
        currentX, baselineY, context->width, context->height,
        visibilityMargin, &w, &h, &xoff, &yoff);
    if (bitmap) {
        float slant = glyph->isItalic ? 0.35f : 0.0f;
        int drawR = GetRValue(glyph->drawColor);
        int drawG = GetGValue(glyph->drawColor);
        int drawB = GetBValue(glyph->drawColor);
        int glyphX = MarkdownStbInternal_AddIntClamped(currentX, xoff);
        int glyphY = MarkdownStbInternal_AddIntClamped(baselineY, yoff);
        int glyphXBold = MarkdownStbInternal_AddIntClamped(glyphX, 1);
        int glyphYBold = MarkdownStbInternal_AddIntClamped(glyphY, 1);
        BOOL useGlobalGradient =
            context->frameGradientInfo && glyph->drawColor == context->color &&
            !glyph->useColorTagGradient;

        if (useGlobalGradient) {
            if (glyph->isItalic) {
                MarkdownStbInternal_BlendItalicGradient(
                    context->bits, context->width, context->height,
                    glyphX, glyphY, bitmap, w, h, slant,
                    context->frameGradientInfo, context->timeOffset,
                    context->width);
                if (glyph->isBold) {
                    MarkdownStbInternal_BlendItalicGradient(
                        context->bits, context->width, context->height,
                        glyphXBold, glyphY, bitmap, w, h, slant,
                        context->frameGradientInfo, context->timeOffset,
                        context->width);
                }
            } else {
                BlendCharBitmapGradientSTBWithInfo(
                    context->bits, context->width, context->height,
                    glyphX, glyphY, bitmap, w, h, 0, context->width,
                    context->frameGradientInfo, context->timeOffset,
                    context->activeEffect);
                if (glyph->isBold) {
                    BlendCharBitmapGradientSTBWithInfo(
                        context->bits, context->width, context->height,
                        glyphXBold, glyphY, bitmap, w, h, 0,
                        context->width, context->frameGradientInfo,
                        context->timeOffset, context->activeEffect);
                    BlendCharBitmapGradientSTBWithInfo(
                        context->bits, context->width, context->height,
                        glyphX, glyphYBold, bitmap, w, h, 0,
                        context->width, context->frameGradientInfo,
                        context->timeOffset, context->activeEffect);
                }
            }
        } else if (glyph->useColorTagGradient && glyph->activeColorTag) {
            if (glyph->isItalic) {
                MarkdownStbInternal_BlendColorTagGradientItalic(
                    context->bits, context->width, context->height,
                    glyphX, glyphY, bitmap, w, h,
                    glyph->activeColorTag, context->timeOffset,
                    context->width, slant);
                if (glyph->isBold) {
                    MarkdownStbInternal_BlendColorTagGradientItalic(
                        context->bits, context->width, context->height,
                        glyphXBold, glyphY, bitmap, w, h,
                        glyph->activeColorTag, context->timeOffset,
                        context->width, slant);
                }
            } else {
                MarkdownStbInternal_BlendColorTagGradient(
                    context->bits, context->width, context->height,
                    glyphX, glyphY, bitmap, w, h,
                    glyph->activeColorTag, context->timeOffset,
                    context->width);
                if (glyph->isBold) {
                    MarkdownStbInternal_BlendColorTagGradient(
                        context->bits, context->width, context->height,
                        glyphXBold, glyphY, bitmap, w, h,
                        glyph->activeColorTag, context->timeOffset,
                        context->width);
                    MarkdownStbInternal_BlendColorTagGradient(
                        context->bits, context->width, context->height,
                        glyphX, glyphYBold, bitmap, w, h,
                        glyph->activeColorTag, context->timeOffset,
                        context->width);
                }
            }
        } else if (glyph->isItalic) {
            MarkdownStbInternal_BlendItalic(
                context->bits, context->width, context->height,
                glyphX, glyphY, bitmap, w, h,
                drawR, drawG, drawB, slant);
            if (glyph->isBold) {
                MarkdownStbInternal_BlendItalic(
                    context->bits, context->width, context->height,
                    glyphXBold, glyphY, bitmap, w, h,
                    drawR, drawG, drawB, slant);
            }
        } else {
            BlendCharBitmapSTBWithEffect(
                context->bits, context->width, context->height,
                glyphX, glyphY, bitmap, w, h,
                drawR, drawG, drawB,
                context->activeEffect, context->effectTimeOffset);
            if (glyph->isBold) {
                BlendCharBitmapSTBWithEffect(
                    context->bits, context->width, context->height,
                    glyphXBold, glyphY, bitmap, w, h,
                    drawR, drawG, drawB,
                    context->activeEffect, context->effectTimeOffset);
                BlendCharBitmapSTBWithEffect(
                    context->bits, context->width, context->height,
                    glyphX, glyphYBold, bitmap, w, h,
                    drawR, drawG, drawB,
                    context->activeEffect, context->effectTimeOffset);
            }
        }
        stbtt_FreeBitmap(bitmap, NULL);

        if (glyph->isStrikethrough) {
            int lineY = MarkdownStbInternal_AddIntClamped(
                baselineY, -(h / 3));
            DWORD* pixels = (DWORD*)context->bits;
            DWORD lineColor;
            if (context->gradientMode != GRADIENT_NONE &&
                glyph->drawColor == context->color) {
                lineColor = context->globalStrikethroughLineColor;
            } else {
                lineColor = 0xFF000000 |
                            (drawR << 16) | (drawG << 8) | drawB;
            }
            if (lineY >= 0 && lineY < context->height &&
                glyph->metrics.advance > 0) {
                int firstStrikeX = 0;
                int lastStrikeX = 0;
                if (MarkdownStbInternal_CalculateVisibleSpan(
                        currentX, glyph->metrics.advance, context->width,
                        &firstStrikeX, &lastStrikeX)) {
                    DWORD* row = pixels +
                        (size_t)lineY * (size_t)context->width;
                    for (int sx = firstStrikeX; sx < lastStrikeX; sx++) {
                        int x = (int)((long long)currentX + sx);
                        row[x] = lineColor;
                    }
                }
            }
        }
    }

    if (context->text[index] == L'\x25A1' ||
        context->text[index] == L'\x25A0') {
        RECT checkboxRect = {
            currentX, line->currentY,
            MarkdownStbInternal_AddIntClamped(
                currentX, glyph->metrics.advance),
            MarkdownStbInternal_AddIntClamped(
                line->currentY, line->lineMaxHeight)
        };
        AddCheckboxRegion(&checkboxRect, context->checkboxIndex,
                          context->text[index] == L'\x25A0');
        context->checkboxIndex++;
    }
}
