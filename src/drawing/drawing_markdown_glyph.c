/**
 * @file drawing_markdown_glyph.c
 * @brief Resolves Markdown heading, style, color, link, and font-tag state per glyph.
 */

#include "drawing/drawing_markdown_stb_internal.h"
#include "drawing/drawing_text_stb.h"

#include <wchar.h>

void MarkdownStbInternal_PrepareGlyph(
    MarkdownRenderContext* context, const MarkdownLineState* line,
    size_t index, int currentX, MarkdownGlyphState* glyph) {
    if (!context || !line || !glyph) return;

    glyph->scale = context->baseScale;
    glyph->fallbackScale = context->fallbackBaseScale;
    glyph->drawColor = context->color;
    glyph->inLink = FALSE;
    glyph->activeLinkIdx = -1;
    glyph->isBold = line->isAlertTitleLine;
    glyph->isItalic = FALSE;
    glyph->isStrikethrough = FALSE;
    glyph->useColorTagGradient = FALSE;
    glyph->activeColorTag = NULL;

    int renderPos = MarkdownStbInternal_ClampPos(index);
    while (context->curHeadingIdx < context->headingCount &&
           renderPos >= context->headings[context->curHeadingIdx].endPos) {
        context->curHeadingIdx++;
    }
    if (context->curHeadingIdx < context->headingCount &&
        renderPos >= context->headings[context->curHeadingIdx].startPos) {
        glyph->scale = MarkdownStbInternal_GetScaleForHeading(
            context->headings[context->curHeadingIdx].level,
            context->baseScale);
        if (context->fallbackLoaded) {
            glyph->fallbackScale =
                MarkdownStbInternal_GetScaleForHeading(
                    context->headings[context->curHeadingIdx].level,
                    context->fallbackBaseScale);
        }
    }

    if (line->isAlertTitleLine) {
        glyph->drawColor = MarkdownStbInternal_GetAlertColor(
            line->activeAlertType);
    }

    while (context->curLinkIdx < context->linkCount &&
           renderPos >= context->links[context->curLinkIdx].endPos) {
        context->curLinkIdx++;
    }
    if (context->curLinkIdx < context->linkCount &&
        renderPos >= context->links[context->curLinkIdx].startPos) {
        glyph->drawColor = RGB(0, 175, 255);
        glyph->inLink = TRUE;
        glyph->activeLinkIdx = context->curLinkIdx;
        if (renderPos == context->links[context->curLinkIdx].startPos) {
            context->links[context->curLinkIdx].linkRect.left = currentX;
            context->links[context->curLinkIdx].linkRect.top = line->currentY;
            context->links[context->curLinkIdx].linkRect.bottom =
                MarkdownStbInternal_AddIntClamped(
                    line->currentY, line->lineMaxHeight);
        }
        context->links[context->curLinkIdx].linkRect.right = currentX;
    }

    if (line->isCompletedTodo &&
        renderPos > line->currentLineStartPos) {
        glyph->isStrikethrough = TRUE;
    }

    while (context->curStyleIdx < context->styleCount &&
           renderPos >= context->styles[context->curStyleIdx].endPos) {
        context->curStyleIdx++;
    }
    if (context->curStyleIdx < context->styleCount &&
        renderPos >= context->styles[context->curStyleIdx].startPos) {
        MarkdownStyleType styleType =
            context->styles[context->curStyleIdx].type;
        if (styleType == STYLE_CODE) {
            glyph->drawColor = RGB(100, 100, 100);
        } else if (styleType == STYLE_BOLD) {
            glyph->isBold = TRUE;
        } else if (styleType == STYLE_ITALIC) {
            glyph->isItalic = TRUE;
        } else if (styleType == STYLE_BOLD_ITALIC) {
            glyph->isBold = TRUE;
            glyph->isItalic = TRUE;
        } else if (styleType == STYLE_STRIKETHROUGH) {
            glyph->isStrikethrough = TRUE;
        }
    }

    while (context->curColorTagIdx < context->colorTagCount &&
           index >= (size_t)context->colorTags[
               context->curColorTagIdx].endPos) {
        context->curColorTagIdx++;
    }
    if (context->curColorTagIdx < context->colorTagCount &&
        index >= (size_t)context->colorTags[
            context->curColorTagIdx].startPos) {
        const MarkdownColorTag* tag =
            &context->colorTags[context->curColorTagIdx];
        if (tag->colorCount == 1) {
            glyph->drawColor = tag->colors[0];
        } else if (tag->colorCount > 1) {
            glyph->useColorTagGradient = TRUE;
            glyph->activeColorTag = tag;
        }
    }

    glyph->charFontInfo = context->fontInfo;
    glyph->charScale = glyph->scale;
    while (context->curFontTagIdx < context->fontTagCount &&
           index >= (size_t)context->fontTags[
               context->curFontTagIdx].endPos) {
        context->curFontTagIdx++;
    }
    if (context->curFontTagIdx < context->fontTagCount &&
        index >= (size_t)context->fontTags[
            context->curFontTagIdx].startPos) {
        if (context->cachedFontTagIdx != context->curFontTagIdx) {
            context->cachedFontTagIdx = context->curFontTagIdx;
            context->cachedFontTagInfo = GetCachedFontSTB(
                context->fontTags[context->curFontTagIdx].fontName);
            context->cachedFontTagScale = context->cachedFontTagInfo
                ? stbtt_ScaleForPixelHeight(
                      context->cachedFontTagInfo,
                      (float)(context->fontSize * context->fontScale))
                : 0.0f;
        }
        if (context->cachedFontTagInfo) {
            glyph->charFontInfo = context->cachedFontTagInfo;
            glyph->charScale = context->cachedFontTagScale;
        }
    }

    if (glyph->charFontInfo != context->fontInfo) {
        if (!GetCachedFontCharMetricsSTB(
                glyph->charFontInfo, context->text[index],
                glyph->charScale, &glyph->metrics) ||
            glyph->metrics.index == 0) {
            GetCharMetricsSTB(
                context->text[index],
                (index < line->end - 1) ? context->text[index + 1] : 0,
                glyph->scale, glyph->fallbackScale, &glyph->metrics);
            glyph->charFontInfo = context->fontInfo;
            glyph->charScale = glyph->scale;
        }
    } else {
        GetCharMetricsSTB(
            context->text[index],
            (index < line->end - 1) ? context->text[index + 1] : 0,
            glyph->scale, glyph->fallbackScale, &glyph->metrics);
    }
}
