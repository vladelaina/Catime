/**
 * @file drawing_markdown_stb.c
 * @brief Coordinates Markdown measurement and frame rendering.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include "drawing/drawing_text_stb.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "menu_preview.h"

#include <math.h>
#include <wchar.h>

static BOOL InitializeMarkdownRenderContext(
    MarkdownRenderContext* context, void* bits, int width, int height,
    const wchar_t* text, MarkdownLink* links, int linkCount,
    const MarkdownHeading* headings, int headingCount,
    MarkdownStyle* styles, int styleCount,
    MarkdownBlockquote* blockquotes, int blockquoteCount,
    MarkdownColorTag* colorTags, int colorTagCount,
    const MarkdownFontTag* fontTags, int fontTagCount,
    COLORREF color, int fontSize, float fontScale, int gradientMode,
    const GradientInfo* gradientInfo, int measuredTextWidth,
    int measuredTextHeight) {
    if (!context || !IsFontLoadedSTB() || !text || !bits ||
        width <= 0 || height <= 0 ||
        (size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD) ||
        measuredTextWidth < 0 || measuredTextHeight <= 0) {
        return FALSE;
    }

    *context = (MarkdownRenderContext){0};
    context->bits = bits;
    context->width = width;
    context->height = height;
    context->text = text;
    context->links = links;
    context->linkCount = linkCount;
    context->headings = headings;
    context->headingCount = headingCount;
    context->styles = styles;
    context->styleCount = styleCount;
    context->blockquotes = blockquotes;
    context->blockquoteCount = blockquoteCount;
    context->colorTags = colorTags;
    context->colorTagCount = colorTagCount;
    context->fontTags = fontTags;
    context->fontTagCount = fontTagCount;
    context->color = color;
    context->fontSize = fontSize;
    context->fontScale = fontScale;
    context->gradientMode = gradientMode;
    context->frameGradientInfo = gradientInfo;

    if (!context->frameGradientInfo &&
        gradientMode != GRADIENT_NONE &&
        GetGradientInfoSnapshot(
            (GradientType)gradientMode,
            &context->fallbackGradientSnapshot)) {
        context->frameGradientInfo =
            &context->fallbackGradientSnapshot.info;
    }

    ClearClickableRegions();
    context->fontInfo = GetMainFontInfoSTB();
    context->fallbackFontInfo = GetFallbackFontInfoSTB();
    context->fallbackLoaded = IsFallbackFontLoadedSTB();
    context->baseScale = stbtt_ScaleForPixelHeight(
        context->fontInfo, (float)(fontSize * fontScale));
    context->fallbackBaseScale = context->fallbackLoaded
        ? stbtt_ScaleForPixelHeight(
              context->fallbackFontInfo, (float)(fontSize * fontScale))
        : 0.0f;

    int baseDescent = 0;
    int baseLineGap = 0;
    stbtt_GetFontVMetrics(context->fontInfo, &context->baseAscent,
                           &baseDescent, &baseLineGap);
    context->lineHeightMetric =
        context->baseAscent - baseDescent + baseLineGap;
    context->len = wcslen(text);
    context->currentY = (height - measuredTextHeight) / 2;
    context->blockLeftX = (width - measuredTextWidth) / 2;
    context->maxLineWidth = measuredTextWidth;
    context->cachedFontTagIdx = -1;

    EffectType activeEffect = GetActiveEffect();
    DWORD frameTick = GetTickCount();
    context->effectTimeOffset = (int)frameTick;
    context->timeOffset = 0;
    context->activeEffect = activeEffect;
    if (activeEffect == EFFECT_TYPE_LIQUID ||
        activeEffect == EFFECT_TYPE_AQUA) {
        context->timeOffset = (int)frameTick;
    } else if (context->frameGradientInfo &&
               context->frameGradientInfo->isAnimated) {
        float progress = (float)(frameTick % 2000) / 2000.0f;
        context->timeOffset =
            (int)(progress * GRADIENT_LUT_SIZE * 2);
    } else if (colorTagCount > 0) {
        context->timeOffset = (int)frameTick;
    }

    context->globalStrikethroughLineColor =
        0xFF000000 | (GetRValue(color) << 16) |
        (GetGValue(color) << 8) | GetBValue(color);
    if (context->frameGradientInfo) {
        if (context->frameGradientInfo->palette &&
            context->frameGradientInfo->paletteCount > 0) {
            COLORREF first = context->frameGradientInfo->palette[0];
            context->globalStrikethroughLineColor =
                0xFF000000 | (GetRValue(first) << 16) |
                (GetGValue(first) << 8) | GetBValue(first);
        } else {
            COLORREF start = context->frameGradientInfo->startColor;
            context->globalStrikethroughLineColor =
                0xFF000000 | (GetRValue(start) << 16) |
                (GetGValue(start) << 8) | GetBValue(start);
        }
    }
    return TRUE;
}

void RenderMarkdownSTBMeasured(void* bits, int width, int height,
                               const wchar_t* text, MarkdownLink* links,
                               int linkCount,
                               const MarkdownHeading* headings,
                               int headingCount, MarkdownStyle* styles,
                               int styleCount,
                               MarkdownBlockquote* blockquotes,
                               int blockquoteCount,
                               MarkdownColorTag* colorTags,
                               int colorTagCount,
                               const MarkdownFontTag* fontTags,
                               int fontTagCount, COLORREF color, int fontSize,
                               float fontScale, int gradientMode,
                               const GradientInfo* gradientInfo,
                               int measuredTextWidth,
                               int measuredTextHeight) {
    if (!BeginFontUseSTB()) return;

    MarkdownRenderContext context = {0};
    if (!InitializeMarkdownRenderContext(
            &context, bits, width, height, text, links, linkCount,
            headings, headingCount, styles, styleCount, blockquotes,
            blockquoteCount, colorTags, colorTagCount, fontTags,
            fontTagCount, color, fontSize, fontScale, gradientMode,
            gradientInfo, measuredTextWidth, measuredTextHeight)) {
        EndFontUseSTB();
        return;
    }

    size_t currentLineStart = 0;
    for (size_t index = 0; index <= context.len; index++) {
        if (context.text[index] != L'\n' &&
            context.text[index] != L'\0') {
            continue;
        }

        MarkdownLineState line = {
            .start = currentLineStart,
            .end = index,
            .currentY = context.currentY
        };
        MarkdownStbInternal_MeasureLine(
            &context, currentLineStart, index, &line);
        if (MarkdownStbInternal_DrawHorizontalRule(&context, &line)) {
            context.currentY = MarkdownStbInternal_AddIntClamped(
                context.currentY, line.lineMaxHeight);
            currentLineStart = index + 1;
            continue;
        }

        MarkdownStbInternal_PrepareLineDecorations(&context, &line);
        MarkdownStbInternal_RenderLine(&context, &line);
        context.currentY = MarkdownStbInternal_AddIntClamped(
            context.currentY, line.lineMaxHeight);
        currentLineStart = index + 1;
    }

    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkUrl &&
            links[i].linkRect.right > links[i].linkRect.left) {
            AddLinkRegion(&links[i].linkRect, links[i].linkUrl);
        }
    }
    EndFontUseSTB();
}

void RenderMarkdownSTB(void* bits, int width, int height,
                       const wchar_t* text, MarkdownLink* links,
                       int linkCount, const MarkdownHeading* headings,
                       int headingCount, MarkdownStyle* styles, int styleCount,
                       MarkdownBlockquote* blockquotes, int blockquoteCount,
                       MarkdownColorTag* colorTags, int colorTagCount,
                       const MarkdownFontTag* fontTags, int fontTagCount,
                       COLORREF color, int fontSize, float fontScale,
                       int gradientMode, const GradientInfo* gradientInfo) {
    int measuredTextWidth = 0;
    int measuredTextHeight = 0;
    if (!MeasureMarkdownSTBScaled(
            text, headings, headingCount, fontTags, fontTagCount,
            fontSize, fontScale, &measuredTextWidth, &measuredTextHeight)) {
        return;
    }

    RenderMarkdownSTBMeasured(
        bits, width, height, text, links, linkCount, headings, headingCount,
        styles, styleCount, blockquotes, blockquoteCount, colorTags,
        colorTagCount, fontTags, fontTagCount, color, fontSize, fontScale,
        gradientMode, gradientInfo, measuredTextWidth, measuredTextHeight);
}
