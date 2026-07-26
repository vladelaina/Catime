/**
 * @file drawing_render_context.c
 * @brief Render context creation, markdown measurement, and cache cleanup.
 */

#include "drawing_render_internal.h"

void CreateRenderContext(RenderContext* ctx) {
    char colorStr[COLOR_HEX_BUFFER] = {0};

    if (!ctx) return;
    ZeroMemory(ctx, sizeof(*ctx));

    GetActiveFont(ctx->fontFileName, ctx->fontInternalName, sizeof(ctx->fontFileName));
    GetActiveColor(colorStr, sizeof(colorStr));
    ctx->fontPathResolved = ResolveFontPathFromNameCached(ctx->fontFileName,
                                                          ctx->absoluteFontPath,
                                                          sizeof(ctx->absoluteFontPath));

    GradientType gradType = GetGradientInfoSnapshotByName(colorStr, &ctx->gradientSnapshot);
    ctx->hasGradient = (gradType != GRADIENT_NONE);
    ctx->gradientMode = (int)gradType;
    ctx->textColor = ParseColorString(colorStr, ctx->hasGradient ? &ctx->gradientSnapshot.info : NULL);

    /* Use plugin scale when in plugin mode, otherwise use clock scale */
    ctx->fontScaleFactor = PluginData_IsActive() ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    ctx->renderFontSize = CLOCK_BASE_FONT_SIZE;
}

BOOL MeasureTextMarkdown(const wchar_t* text, const RenderContext* ctx, SIZE* outSize,
                               const MarkdownHeading* headings, int headingCount,
                               const MarkdownFontTag* fontTags, int fontTagCount) {
    if (ctx && ctx->fontPathResolved && text && outSize) {
        int fontSize = ctx->renderFontSize;
        float fontScaleFactor = ctx->fontScaleFactor;
        BOOL isMarkdown = (headings && headingCount > 0) || (fontTags && fontTagCount > 0);
        DWORD headingSignature = ComputeHeadingSignature(headings, headingCount);
        DWORD fontTagSignature = ComputeFontTagSignature(fontTags, fontTagCount);

        if (!InitFontSTB(ctx->absoluteFontPath)) {
            return FALSE;
        }

        BOOL canUseMeasureCache = RefreshMeasureCacheFontTags(fontTags, fontTagCount);

        DWORD fontStateGeneration = GetFontStateGenerationSTB();

        if (canUseMeasureCache &&
            g_textMeasureCache.valid &&
            g_textMeasureCache.isMarkdown == isMarkdown &&
            g_textMeasureCache.fontSize == fontSize &&
            fabsf(g_textMeasureCache.fontScaleFactor - fontScaleFactor) < 0.0001f &&
            g_textMeasureCache.headingSignature == headingSignature &&
            g_textMeasureCache.fontTagSignature == fontTagSignature &&
            g_textMeasureCache.fontStateGeneration == fontStateGeneration &&
            strcmp(g_textMeasureCache.fontPath, ctx->absoluteFontPath) == 0 &&
            CachedWideTextEquals(g_textMeasureCache.text,
                                 _countof(g_textMeasureCache.text),
                                 g_textMeasureCache.textLen,
                                 text)) {
            *outSize = g_textMeasureCache.size;
            return TRUE;
        }

        int w, h;
        if (MeasureMarkdownSTBScaled(text, headings, headingCount, fontTags, fontTagCount,
                                     fontSize, fontScaleFactor, &w, &h)) {
            outSize->cx = w;
            outSize->cy = h;
            g_textMeasureCache.valid = TRUE;
            g_textMeasureCache.isMarkdown = isMarkdown;
            g_textMeasureCache.fontSize = fontSize;
            g_textMeasureCache.fontScaleFactor = fontScaleFactor;
            g_textMeasureCache.headingSignature = headingSignature;
            g_textMeasureCache.fontTagSignature = fontTagSignature;
            g_textMeasureCache.fontStateGeneration = GetFontStateGenerationSTB();
            strcpy_s(g_textMeasureCache.fontPath, sizeof(g_textMeasureCache.fontPath),
                     ctx->absoluteFontPath);
            CopyCachedWideText(g_textMeasureCache.text,
                               _countof(g_textMeasureCache.text),
                               &g_textMeasureCache.textLen,
                               text);
            g_textMeasureCache.size = *outSize;
            return TRUE;
        }
    }

    return FALSE;
}

void ReleaseRenderDibCache(void);
void ReleaseScaleFrameSnapshot(void);

BOOL RenderTextMarkdown(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode, void* bits,
                              MarkdownLink* links, int linkCount,
                              const MarkdownHeading* headings, int headingCount,
                              MarkdownStyle* styles, int styleCount,
                              MarkdownBlockquote* blockquotes, int blockquoteCount,
                              MarkdownColorTag* colorTags, int colorTagCount,
                              const MarkdownFontTag* fontTags, int fontTagCount,
                              const SIZE* measuredSize) {
    UNREFERENCED_PARAMETER(hdc);
    UNREFERENCED_PARAMETER(editMode);

    // Use STB Truetype for high-quality rendering
    if (ctx && ctx->fontPathResolved) {
        if (InitFontSTB(ctx->absoluteFontPath)) {
            int measuredWidth = measuredSize ? measuredSize->cx : 0;
            int measuredHeight = measuredSize ? measuredSize->cy : 0;
            if (measuredWidth > 0 && measuredHeight > 0) {
                RenderMarkdownSTBMeasured(bits, rect->right, rect->bottom, text,
                                          links, linkCount,
                                          headings, headingCount,
                                          styles, styleCount,
                                          blockquotes, blockquoteCount,
                                          colorTags, colorTagCount,
                                          fontTags, fontTagCount,
                                          ctx->textColor,
                                          ctx->renderFontSize,
                                          ctx->fontScaleFactor,
                                          ctx->gradientMode,
                                          ctx->hasGradient ? &ctx->gradientSnapshot.info : NULL,
                                          measuredWidth, measuredHeight);
            } else {
                RenderMarkdownSTB(bits, rect->right, rect->bottom, text,
                                  links, linkCount,
                                  headings, headingCount,
                                  styles, styleCount,
                                  blockquotes, blockquoteCount,
                                  colorTags, colorTagCount,
                                  fontTags, fontTagCount,
                                  ctx->textColor,
                                  ctx->renderFontSize,
                                  ctx->fontScaleFactor,
                                  ctx->gradientMode,
                                  ctx->hasGradient ? &ctx->gradientSnapshot.info : NULL);
            }
            return TRUE;
        }
    }

    return FALSE;
}

void CleanupDrawingRenderCache(void) {
    ClearClickableRegions();
    ClearPluginPaintCache();
    ClearMarkdownRenderCache();
    ClearTextMeasureCache();
    ReleaseScaleFrameSnapshot();
    ReleaseRenderDibCache();
    CleanupFontSTB();
}
