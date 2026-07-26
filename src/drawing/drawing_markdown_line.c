/**
 * @file drawing_markdown_line.c
 * @brief Measures and decorates individual Markdown lines.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include "drawing/drawing_text_stb.h"
#include "markdown/markdown_interactive.h"

#include <wchar.h>

static const struct AlertColorInfo {
    BlockquoteAlertType type;
    COLORREF color;
} g_alertColors[] = {
    {BLOCKQUOTE_NOTE,      RGB(31, 136, 229)},
    {BLOCKQUOTE_TIP,       RGB(26, 127, 55)},
    {BLOCKQUOTE_IMPORTANT, RGB(130, 80, 223)},
    {BLOCKQUOTE_WARNING,   RGB(191, 135, 0)},
    {BLOCKQUOTE_CAUTION,   RGB(207, 34, 46)},
};

COLORREF MarkdownStbInternal_GetAlertColor(BlockquoteAlertType type) {
    for (size_t i = 0; i < sizeof(g_alertColors) / sizeof(g_alertColors[0]); ++i) {
        if (g_alertColors[i].type == type) return g_alertColors[i].color;
    }
    return RGB(128, 128, 128);  /* Default gray for normal blockquote */
}

void MarkdownStbInternal_MeasureLine(
    const MarkdownRenderContext* context, size_t start, size_t end,
    MarkdownLineState* line) {
    if (!context || !line) return;
    line->start = start;
    line->end = end;
    line->lineMaxHeight = MarkdownStbInternal_GetLineHeightFromMetric(
        context->lineHeightMetric, context->baseScale);
    line->maxAscent = (int)(context->baseAscent * context->baseScale);

    int temporaryHeadingIndex = context->curHeadingIdx;
    for (size_t index = start; index < end; index++) {
        if (context->text[index] == L'\r') continue;

        float scale = context->baseScale;
        int lineCharPos = MarkdownStbInternal_ClampPos(index);
        while (temporaryHeadingIndex < context->headingCount &&
               lineCharPos >= context->headings[temporaryHeadingIndex].endPos) {
            temporaryHeadingIndex++;
        }
        if (temporaryHeadingIndex < context->headingCount &&
            lineCharPos >= context->headings[temporaryHeadingIndex].startPos) {
            scale = MarkdownStbInternal_GetScaleForHeading(
                context->headings[temporaryHeadingIndex].level,
                context->baseScale);
        }

        int height = MarkdownStbInternal_GetLineHeightFromMetric(
            context->lineHeightMetric, scale);
        if (height > line->lineMaxHeight) line->lineMaxHeight = height;
        int ascent = MarkdownStbInternal_GetLineHeightFromMetric(
            context->baseAscent, scale);
        if (ascent > line->maxAscent) line->maxAscent = ascent;
    }
}

static BOOL IsHorizontalRule(const MarkdownRenderContext* context,
                             const MarkdownLineState* line) {
    return line && line->end - line->start >= 3 &&
           context->text[line->start] == L'\x2500' &&
           context->text[line->start + 1] == L'\x2500' &&
           context->text[line->start + 2] == L'\x2500';
}

BOOL MarkdownStbInternal_DrawHorizontalRule(
    const MarkdownRenderContext* context, const MarkdownLineState* line) {
    if (!context || !line || !IsHorizontalRule(context, line)) return FALSE;

    long long lineY64 = (long long)line->currentY +
                        (long long)(line->lineMaxHeight / 2);
    if (lineY64 < 0 || lineY64 >= (long long)context->height ||
        context->width <= 0) {
        return TRUE;
    }

    DWORD* pixels = (DWORD*)context->bits;
    long long hrLeft = (long long)context->blockLeftX;
    int hrWidth = context->maxLineWidth;
    int drawStart = 0;
    int drawEnd = 0;
    if (!MarkdownStbInternal_CalculateVisibleSpan(
            hrLeft, hrWidth, context->width, &drawStart, &drawEnd)) {
        return TRUE;
    }

    const GradientInfo* gradient = context->frameGradientInfo;
    long long animationOffset = (gradient && gradient->palette &&
                                 gradient->paletteCount > 2)
        ? ((long long)context->timeOffset * MARKDOWN_GRADIENT_FIXED_ONE) /
          (long long)(GRADIENT_LUT_SIZE * 2)
        : 0;
    long long gradientStep = MarkdownStbInternal_GradientStepFixed(hrWidth);
    long long gradientPosition = MarkdownStbInternal_GradientPositionFixed(
        drawStart, hrWidth, animationOffset);
    DWORD* row = pixels + (size_t)lineY64 * (size_t)context->width;

    for (int offset = drawStart; offset < drawEnd; offset++) {
        int x = (int)(hrLeft + (long long)offset);
        DWORD lineColor;
        if (gradient) {
            COLORREF sample = MarkdownStbInternal_SampleGlobalGradient(
                gradient, gradientPosition);
            lineColor = 0xFF000000 |
                        (GetRValue(sample) << 16) |
                        (GetGValue(sample) << 8) |
                        GetBValue(sample);
            MarkdownStbInternal_AdvanceGradientFixed(
                &gradientPosition, gradientStep);
        } else {
            lineColor = 0xFF000000 |
                        (GetRValue(context->color) << 16) |
                        (GetGValue(context->color) << 8) |
                        GetBValue(context->color);
        }
        row[x] = lineColor;
    }
    return TRUE;
}

void MarkdownStbInternal_PrepareLineDecorations(
    MarkdownRenderContext* context, MarkdownLineState* line) {
    if (!context || !line) return;

    line->currentLineStartPos = MarkdownStbInternal_ClampPos(line->start);
    while (context->curBlockquoteIdx < context->blockquoteCount &&
           line->currentLineStartPos >=
               context->blockquotes[context->curBlockquoteIdx].endPos) {
        context->curBlockquoteIdx++;
    }

    line->activeAlertType = BLOCKQUOTE_NORMAL;
    line->inBlockquote = FALSE;
    if (context->curBlockquoteIdx < context->blockquoteCount &&
        line->currentLineStartPos >=
            context->blockquotes[context->curBlockquoteIdx].startPos) {
        line->inBlockquote = TRUE;
        line->activeAlertType =
            context->blockquotes[context->curBlockquoteIdx].alertType;
    }

    if (line->inBlockquote &&
        line->activeAlertType != BLOCKQUOTE_NORMAL) {
        COLORREF barColor = MarkdownStbInternal_GetAlertColor(
            line->activeAlertType);
        DWORD barColorDW = 0xFF000000 |
                           (GetRValue(barColor) << 16) |
                           (GetGValue(barColor) << 8) |
                           GetBValue(barColor);
        int barX = context->blockLeftX - 8;
        int barWidth = 3;
        int firstY = 0;
        int lastY = 0;
        int firstX = 0;
        int lastX = 0;
        if (MarkdownStbInternal_CalculateVisibleSpan(
                line->currentY, line->lineMaxHeight, context->height,
                &firstY, &lastY) &&
            MarkdownStbInternal_CalculateVisibleSpan(
                barX, barWidth, context->width, &firstX, &lastX)) {
            DWORD* pixels = (DWORD*)context->bits;
            for (int yOffset = firstY; yOffset < lastY; yOffset++) {
                int y = (int)((long long)line->currentY + yOffset);
                DWORD* row = pixels + (size_t)y * (size_t)context->width;
                for (int xOffset = firstX; xOffset < lastX; xOffset++) {
                    int x = (int)((long long)barX + xOffset);
                    row[x] = barColorDW;
                }
            }
        }
    }

    line->isAlertTitleLine = FALSE;
    if (line->inBlockquote && line->activeAlertType != BLOCKQUOTE_NORMAL) {
        const wchar_t* lineText = &context->text[line->start];
        line->isAlertTitleLine =
            MarkdownStbInternal_StartsWithLiteral(lineText, L"NOTE:") ||
            MarkdownStbInternal_StartsWithLiteral(lineText, L"TIP:") ||
            MarkdownStbInternal_StartsWithLiteral(lineText, L"IMPORTANT:") ||
            MarkdownStbInternal_StartsWithLiteral(lineText, L"WARNING:") ||
            MarkdownStbInternal_StartsWithLiteral(lineText, L"CAUTION:");
    }
    line->isCompletedTodo = context->text[line->start] == L'\x25A0';
}

void MarkdownStbInternal_RenderLine(
    MarkdownRenderContext* context, const MarkdownLineState* line) {
    if (!context || !line) return;
    int currentX = context->blockLeftX;
    int baselineY = MarkdownStbInternal_AddIntClamped(
        line->currentY, line->maxAscent);

    for (size_t index = line->start; index < line->end; index++) {
        if (context->text[index] == L'\r') continue;

        MarkdownGlyphState glyph = {0};
        MarkdownStbInternal_PrepareGlyph(
            context, line, index, currentX, &glyph);
        MarkdownStbInternal_DrawGlyph(
            context, line, index, currentX, baselineY, &glyph);
        currentX = MarkdownStbInternal_AddIntClamped(
            currentX, glyph.metrics.advance + glyph.metrics.kern);
        if (glyph.inLink && glyph.activeLinkIdx >= 0) {
            context->links[glyph.activeLinkIdx].linkRect.right = currentX;
        }
    }
}
