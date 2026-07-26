/**
 * @file drawing_render_types.h
 * @brief Private state for the GDI rendering pipeline.
 */

#ifndef DRAWING_RENDER_TYPES_H
#define DRAWING_RENDER_TYPES_H

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <windows.h>
#include <mmsystem.h>
#include "drawing/drawing_render.h"
#include "drawing/drawing_time_format.h"
#include "drawing/drawing_text_stb.h"
#include "drawing/drawing_markdown_stb.h"
#include "drawing.h"
#include "font.h"
#include "color/color.h"
#include "color/gradient.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "window/window_core.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "window/window_visual_effects.h"
#include "drag_scale.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "text_effect.h"
#include "font/font_path_manager.h"
#include "log.h"
#include "plugin/plugin_data.h"
#include "drawing/drawing_image.h"
#include "markdown/markdown_parser.h"
#include "markdown/markdown_image.h"
#include "markdown/markdown_interactive.h"
#include "color/color_parser.h"
#include "utils/string_convert.h"
#include "utils/render_retry.h"
#include "../resource/resource.h"

extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;
extern float PLUGIN_FONT_SCALE_FACTOR;

#define MAX_RENDER_DIB_DIMENSION 4096
#define MAX_RENDER_DIB_PIXELS (4096u * 4096u)
#define RENDER_DIB_SHRINK_THRESHOLD_MULTIPLIER 4u
#define SCALE_SNAPSHOT_MIN_PIXELS 500000u
#define PLUGIN_IMAGE_STACK_CAPACITY 4
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define FONT_PATH_RESOLVE_FAILURE_RETRY_MS 5000u
#define MARKDOWN_IMAGE_FILE_RECHECK_MS 1000u
#define RENDER_TIMER_RESOLUTION_MIN_MS 1u
#define RENDER_TIMER_RESOLUTION_MAX_MS 10u
#define MAIN_RENDER_RETRY_BASE_MS 100u
#define MAIN_RENDER_RETRY_MAX_MS 2000u
#define CATIME_OPEN_TAG L"<catime>"
#define CATIME_CLOSE_TAG L"</catime>"
#define CATIME_OPEN_TAG_LEN 8u
#define CATIME_CLOSE_TAG_LEN 9u

typedef struct {
    wchar_t timeText[TIME_TEXT_MAX_LEN];
    wchar_t timerTextSnapshot[TIME_TEXT_MAX_LEN];
    wchar_t pluginText[TIME_TEXT_MAX_LEN];
    wchar_t pluginResult[TIME_TEXT_MAX_LEN];
    wchar_t measureText[TIME_TEXT_MAX_LEN];
} PaintTextBuffers;

typedef struct {
    BOOL valid;
    HWND hwnd;
    DWORD gestureSerial;
    wchar_t text[TIME_TEXT_MAX_LEN];
} ScaleGestureTextCache;

typedef enum {
    PLUGIN_TEXT_MARKER_NONE = 0,
    PLUGIN_TEXT_MARKER_IMAGE,
    PLUGIN_TEXT_MARKER_CATIME
} PluginTextMarkerKind;

typedef struct {
    BOOL valid;
    BOOL isMarkdown;
    wchar_t sourceText[TIME_TEXT_MAX_LEN];
    size_t sourceTextLen;
    wchar_t* mdText;
    MarkdownLink* links;
    int linkCount;
    MarkdownHeading* headings;
    int headingCount;
    MarkdownStyle* styles;
    int styleCount;
    MarkdownListItem* listItems;
    int listItemCount;
    MarkdownBlockquote* blockquotes;
    int blockquoteCount;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    MarkdownFontTag* fontTags;
    int fontTagCount;
} MarkdownRenderCache;

typedef struct {
    BOOL valid;
    wchar_t sourceText[TIME_TEXT_MAX_LEN];
    size_t sourceTextLen;
    wchar_t renderedText[TIME_TEXT_MAX_LEN];
    MarkdownImage* images;
    int imageCount;
} PluginPaintCache;

typedef struct {
    BOOL valid;
    BOOL isMarkdown;
    int fontSize;
    float fontScaleFactor;
    DWORD headingSignature;
    DWORD fontTagSignature;
    DWORD fontStateGeneration;
    char fontPath[MAX_PATH];
    wchar_t text[TIME_TEXT_MAX_LEN];
    size_t textLen;
    SIZE size;
} TextMeasureCache;

typedef struct {
    BOOL valid;
    char fontFileName[MAX_PATH];
    char absoluteFontPath[MAX_PATH];
    BOOL resolved;
    DWORD retryAfterFailureTick;
} FontPathResolveCache;

typedef struct {
    HDC memDC;
    HBITMAP memBitmap;
    HBITMAP oldBitmap;
    void* bits;
    int width;
    int height;
    BOOL frameValid;
    BOOL frameWasScaleComposite;
    BOOL frameEditMode;
    HWND frameHwnd;
    int frameWidth;
    int frameHeight;
} RenderDibCache;

typedef struct {
    HDC memDC;
    HBITMAP memBitmap;
    HBITMAP oldBitmap;
    void* bits;
    int width;
    int height;
    HWND hwnd;
    DWORD gestureSerial;
} ScaleFrameSnapshot;

typedef struct {
    HWND hwnd;
    HDC hdc;
    RECT rect;
    DWORD activeScaleSerial;
    PaintTextBuffers* paintBuffers;
    MarkdownImage stackImages[PLUGIN_IMAGE_STACK_CAPACITY];
    MarkdownImage* images;
    int imageCount;
    BOOL imagesHeapAllocated;
    BOOL imagesOwnedByCache;
    RenderContext renderContext;
    BOOL isMarkdown;
    const MarkdownHeading* headings;
    int headingCount;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    const MarkdownFontTag* fontTags;
    int fontTagCount;
    const wchar_t* textToRender;
    BOOL hasText;
    BOOL hasContent;
    SIZE measuredTextSize;
    BOOL measuredTextSizeValid;
    HDC memDC;
    HBITMAP memBitmap;
    HBITMAP oldBitmap;
    void* bits;
    BOOL usedScaleComposite;
} PaintFrameContext;

extern BOOL s_renderAnimationTimerActive;
extern UINT s_renderAnimationTimerInterval;
extern HWND s_renderAnimationTimerHwnd;
extern UINT s_renderAnimationTimerResolutionMs;
extern DWORD s_nextMarkdownImageFileCheckTick;
extern RenderRetryController s_mainRenderRetry;
extern HWND s_mainRenderRetryHwnd;
extern PaintTextBuffers g_paintTextBuffers;
extern ScaleGestureTextCache g_scaleGestureTextCache;
extern MarkdownRenderCache g_markdownRenderCache;
extern PluginPaintCache g_pluginPaintCache;
extern TextMeasureCache g_textMeasureCache;
extern FontPathResolveCache g_fontPathResolveCache;
extern RenderDibCache g_renderDibCache;
extern ScaleFrameSnapshot g_scaleFrameSnapshot;

#endif /* DRAWING_RENDER_TYPES_H */
