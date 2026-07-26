/**
 * @file drawing_markdown_stb_internal.h
 * @brief Private context and helpers for the STB Markdown renderer.
 */

#ifndef DRAWING_MARKDOWN_STB_INTERNAL_H
#define DRAWING_MARKDOWN_STB_INTERNAL_H

#include "drawing/drawing_markdown_stb.h"
#include "color/gradient.h"

#define MARKDOWN_GRADIENT_FIXED_ONE (1LL << 32)

typedef struct {
    void* bits;
    int width;
    int height;
    const wchar_t* text;
    MarkdownLink* links;
    int linkCount;
    const MarkdownHeading* headings;
    int headingCount;
    MarkdownStyle* styles;
    int styleCount;
    MarkdownBlockquote* blockquotes;
    int blockquoteCount;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    const MarkdownFontTag* fontTags;
    int fontTagCount;
    COLORREF color;
    int fontSize;
    float fontScale;
    int gradientMode;
    const GradientInfo* frameGradientInfo;
    GradientInfoSnapshot fallbackGradientSnapshot;
    const stbtt_fontinfo* fontInfo;
    const stbtt_fontinfo* fallbackFontInfo;
    const stbtt_fontinfo* cachedFontTagInfo;
    BOOL fallbackLoaded;
    float baseScale;
    float fallbackBaseScale;
    float cachedFontTagScale;
    int baseAscent;
    int lineHeightMetric;
    size_t len;
    int blockLeftX;
    int maxLineWidth;
    int currentY;
    int curHeadingIdx;
    int curLinkIdx;
    int curStyleIdx;
    int curBlockquoteIdx;
    int curColorTagIdx;
    int curFontTagIdx;
    int cachedFontTagIdx;
    EffectType activeEffect;
    int effectTimeOffset;
    int timeOffset;
    DWORD globalStrikethroughLineColor;
    int checkboxIndex;
} MarkdownRenderContext;

typedef struct {
    size_t start;
    size_t end;
    int currentY;
    int lineMaxHeight;
    int maxAscent;
    int currentLineStartPos;
    BlockquoteAlertType activeAlertType;
    BOOL inBlockquote;
    BOOL isAlertTitleLine;
    BOOL isCompletedTodo;
} MarkdownLineState;

typedef struct {
    float scale;
    float fallbackScale;
    float charScale;
    COLORREF drawColor;
    BOOL inLink;
    int activeLinkIdx;
    BOOL isBold;
    BOOL isItalic;
    BOOL isStrikethrough;
    BOOL useColorTagGradient;
    const MarkdownColorTag* activeColorTag;
    const stbtt_fontinfo* charFontInfo;
    GlyphMetrics metrics;
} MarkdownGlyphState;

float MarkdownStbInternal_GetScaleForHeading(int level, float baseScale);
int MarkdownStbInternal_GetLineHeightFromMetric(int fontMetricHeight, float scale);
int MarkdownStbInternal_ClampPos(size_t pos);
int MarkdownStbInternal_AddIntClamped(int value, int delta);
BOOL MarkdownStbInternal_CalculateVisibleSpan(
    long long start, int length, int limit, int* outFirst, int* outLast);
BOOL MarkdownStbInternal_StartsWithLiteral(
    const wchar_t* text, const wchar_t* prefix);
long long MarkdownStbInternal_GradientStepFixed(int totalWidth);
long long MarkdownStbInternal_GradientPositionFixed(
    long long screenX, int totalWidth, long long offsetFixed);
void MarkdownStbInternal_AdvanceGradientFixed(
    long long* position, long long step);
COLORREF MarkdownStbInternal_SampleGradient(
    const COLORREF* colors, int colorCount, long long position);
COLORREF MarkdownStbInternal_SampleGlobalGradient(
    const GradientInfo* info, long long position);

void MarkdownStbInternal_BlendItalic(
    void* destBits, int destWidth, int destHeight,
    int x_pos, int y_pos, const unsigned char* bitmap, int w, int h,
    int r, int g, int b, float slant);
void MarkdownStbInternal_BlendItalicGradient(
    void* destBits, int destWidth, int destHeight,
    int x_pos, int y_pos, const unsigned char* bitmap, int w, int h,
    float slant, const GradientInfo* info, int timeOffset, int totalWidth);
void MarkdownStbInternal_BlendColorTagGradient(
    void* destBits, int destWidth, int destHeight,
    int x_pos, int y_pos, const unsigned char* bitmap, int w, int h,
    const MarkdownColorTag* colorTag, int timeOffset, int totalWidth);
void MarkdownStbInternal_BlendColorTagGradientItalic(
    void* destBits, int destWidth, int destHeight,
    int x_pos, int y_pos, const unsigned char* bitmap, int w, int h,
    const MarkdownColorTag* colorTag, int timeOffset, int totalWidth,
    float slant);

COLORREF MarkdownStbInternal_GetAlertColor(BlockquoteAlertType type);
void MarkdownStbInternal_MeasureLine(
    const MarkdownRenderContext* context, size_t start, size_t end,
    MarkdownLineState* line);
BOOL MarkdownStbInternal_DrawHorizontalRule(
    const MarkdownRenderContext* context, const MarkdownLineState* line);
void MarkdownStbInternal_PrepareLineDecorations(
    MarkdownRenderContext* context, MarkdownLineState* line);
void MarkdownStbInternal_RenderLine(
    MarkdownRenderContext* context, const MarkdownLineState* line);

void MarkdownStbInternal_PrepareGlyph(
    MarkdownRenderContext* context, const MarkdownLineState* line,
    size_t index, int currentX, MarkdownGlyphState* glyph);
void MarkdownStbInternal_DrawGlyph(
    MarkdownRenderContext* context, const MarkdownLineState* line,
    size_t index, int currentX, int baselineY,
    const MarkdownGlyphState* glyph);

#endif /* DRAWING_MARKDOWN_STB_INTERNAL_H */
