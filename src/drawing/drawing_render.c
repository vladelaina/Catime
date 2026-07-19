/**
 * @file drawing_render.c
 * @brief GDI rendering pipeline with double-buffering
 */

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

static const wchar_t CATIME_OPEN_TAG[] = L"<catime>";
static const wchar_t CATIME_CLOSE_TAG[] = L"</catime>";

#define CATIME_OPEN_TAG_LEN (_countof(CATIME_OPEN_TAG) - 1)
#define CATIME_CLOSE_TAG_LEN (_countof(CATIME_CLOSE_TAG) - 1)

static BOOL s_renderAnimationTimerActive = FALSE;
static UINT s_renderAnimationTimerInterval = 0;
static HWND s_renderAnimationTimerHwnd = NULL;
static UINT s_renderAnimationTimerResolutionMs = 0;
static DWORD s_nextMarkdownImageFileCheckTick = 0;
static RenderRetryController s_mainRenderRetry = {0};
static HWND s_mainRenderRetryHwnd = NULL;

typedef struct {
    wchar_t timeText[TIME_TEXT_MAX_LEN];
    wchar_t timerTextSnapshot[TIME_TEXT_MAX_LEN];
    wchar_t pluginText[TIME_TEXT_MAX_LEN];
    wchar_t pluginResult[TIME_TEXT_MAX_LEN];
    wchar_t measureText[TIME_TEXT_MAX_LEN];
} PaintTextBuffers;

/* Paint runs on the UI thread; keeping these off the stack avoids large frames
 * during animation/plugin redraw bursts without adding per-frame heap churn.
 */
static PaintTextBuffers g_paintTextBuffers = {0};

typedef struct {
    BOOL valid;
    HWND hwnd;
    DWORD gestureSerial;
    wchar_t text[TIME_TEXT_MAX_LEN];
} ScaleGestureTextCache;

static ScaleGestureTextCache g_scaleGestureTextCache = {0};

static BOOL IsValidRenderAnimationWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static void ResetMainWindowRenderRetry(HWND hwnd) {
    HWND trackedHwnd = s_mainRenderRetryHwnd;
    if (IsValidRenderAnimationWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
    }
    if (trackedHwnd != hwnd && IsValidRenderAnimationWindow(trackedHwnd)) {
        KillTimer(trackedHwnd, TIMER_ID_FORCE_REDRAW);
    }
    s_mainRenderRetryHwnd = NULL;
    RenderRetry_Reset(&s_mainRenderRetry);
}

static BOOL ArmMainWindowRenderRetry(HWND hwnd, UINT delayMs) {
    if (!IsValidRenderAnimationWindow(hwnd)) return FALSE;
    if (RenderRetry_IsTimerArmed(&s_mainRenderRetry) &&
        s_mainRenderRetryHwnd == hwnd) {
        return TRUE;
    }

    if (s_mainRenderRetryHwnd && s_mainRenderRetryHwnd != hwnd) {
        ResetMainWindowRenderRetry(s_mainRenderRetryHwnd);
    }

    if (SetTimer(hwnd, TIMER_ID_FORCE_REDRAW, delayMs > 0 ? delayMs : 1u, NULL) == 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to schedule main render retry (delay=%u, error=%lu)",
                 delayMs, GetLastError());
        return FALSE;
    }

    s_mainRenderRetryHwnd = hwnd;
    RenderRetry_MarkTimerArmed(&s_mainRenderRetry);
    return TRUE;
}

static void RecordMainWindowRenderFailure(HWND hwnd) {
    if (!IsValidRenderAnimationWindow(hwnd)) return;
    if (s_mainRenderRetryHwnd && s_mainRenderRetryHwnd != hwnd) {
        ResetMainWindowRenderRetry(s_mainRenderRetryHwnd);
    }

    UINT delay = RenderRetry_RecordFailure(&s_mainRenderRetry,
                                           MAIN_RENDER_RETRY_BASE_MS,
                                           MAIN_RENDER_RETRY_MAX_MS);
    ArmMainWindowRenderRetry(hwnd, delay);
}

static BOOL ShouldLogMainWindowRenderFailure(void) {
    UINT failures = s_mainRenderRetry.consecutiveFailures;
    return failures == 0 || (failures & (failures - 1u)) == 0;
}

BOOL HandleDrawingRenderRetryTimer(HWND hwnd) {
    if (!IsValidRenderAnimationWindow(hwnd)) return FALSE;

    KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
    RenderRetry_MarkTimerFired(&s_mainRenderRetry);
    s_mainRenderRetryHwnd = hwnd;

    if (!RenderRetry_IsActive(&s_mainRenderRetry)) {
        ResetMainWindowRenderRetry(hwnd);
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        ResetMainWindowRenderRetry(hwnd);
        return TRUE;
    }

    if (CLOCK_IS_DRAGGING) {
        ArmMainWindowRenderRetry(hwnd, MAIN_RENDER_RETRY_BASE_MS);
        return TRUE;
    }

    RedrawWindow(hwnd, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

    /* A successful paint resets the controller synchronously. If Windows did
     * not dispatch WM_PAINT, keep probing at the current bounded delay. */
    if (RenderRetry_IsActive(&s_mainRenderRetry) &&
        !RenderRetry_IsTimerArmed(&s_mainRenderRetry)) {
        ArmMainWindowRenderRetry(
            hwnd,
            RenderRetry_GetDelay(&s_mainRenderRetry,
                                 MAIN_RENDER_RETRY_BASE_MS,
                                 MAIN_RENDER_RETRY_MAX_MS));
    }
    return TRUE;
}

typedef enum {
    PLUGIN_TEXT_MARKER_NONE = 0,
    PLUGIN_TEXT_MARKER_IMAGE,
    PLUGIN_TEXT_MARKER_CATIME
} PluginTextMarkerKind;

static void AppendWideSpan(wchar_t** dst, size_t* remaining,
                           const wchar_t* text, size_t textLen) {
    if (!dst || !*dst || !remaining || !text || *remaining == 0 || textLen == 0) {
        return;
    }

    if (textLen > *remaining) {
        textLen = *remaining;
    }

    memcpy(*dst, text, textLen * sizeof(wchar_t));
    *dst += textLen;
    *remaining -= textLen;
}

static const wchar_t* FindNextPluginTextMarker(const wchar_t* src,
                                               BOOL includeImages,
                                               PluginTextMarkerKind* markerKind) {
    if (markerKind) {
        *markerKind = PLUGIN_TEXT_MARKER_NONE;
    }
    if (!src) {
        return NULL;
    }

    for (const wchar_t* p = src; *p; ++p) {
        if (includeImages && p[0] == L'!' && p[1] == L'[') {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_IMAGE;
            }
            return p;
        }

        if (p[0] == L'<' &&
            wcsncmp(p, CATIME_OPEN_TAG, CATIME_OPEN_TAG_LEN) == 0) {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_CATIME;
            }
            return p;
        }
    }

    return NULL;
}

static COLORREF ParseColorString(const char* colorStr, const GradientInfo* gradientInfo) {
    if (!colorStr) {
        return RGB(255, 255, 255);
    }

    size_t colorLen = strlen(colorStr);
    if (colorLen == 0) {
        return RGB(255, 255, 255);
    }

    /* Use gradient start color for fallback GDI drawing paths. */
    if (gradientInfo) {
        return gradientInfo->startColor;
    }

    COLORREF parsed = RGB(255, 255, 255);
    return ColorStringToColorRef(colorStr, &parsed) ? parsed :
           RGB(255, 255, 255);
}

static int CalculateRenderFontSize(int baseFontSize, float scaleFactor) {
    double scaled = (double)baseFontSize * (double)scaleFactor;
    if (!isfinite(scaled) || scaled < 1.0) {
        return 1;
    }
    if (scaled > (double)INT_MAX) {
        return INT_MAX;
    }
    return (int)scaled;
}

static BOOL HasPotentialMarkdownSyntax(const wchar_t* text) {
    if (!text) return FALSE;

    for (const wchar_t* p = text; *p; ++p) {
        switch (*p) {
            case L'!':
            case L'[':
            case L']':
            case L'(':
            case L')':
            case L'<':
            case L'>':
            case L'*':
            case L'_':
            case L'`':
            case L'#':
                return TRUE;
            default:
                break;
        }
    }

    return FALSE;
}

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

static MarkdownRenderCache g_markdownRenderCache = {0};

typedef struct {
    BOOL valid;
    wchar_t sourceText[TIME_TEXT_MAX_LEN];
    size_t sourceTextLen;
    wchar_t renderedText[TIME_TEXT_MAX_LEN];
    MarkdownImage* images;
    int imageCount;
} PluginPaintCache;

static PluginPaintCache g_pluginPaintCache = {0};

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

static TextMeasureCache g_textMeasureCache = {0};

static void ClearTextMeasureCache(void) {
    ZeroMemory(&g_textMeasureCache, sizeof(g_textMeasureCache));
}

typedef struct {
    BOOL valid;
    char fontFileName[MAX_PATH];
    char absoluteFontPath[MAX_PATH];
    BOOL resolved;
    DWORD retryAfterFailureTick;
} FontPathResolveCache;

static FontPathResolveCache g_fontPathResolveCache = {0};

static DWORD ComputeHeadingSignature(const MarkdownHeading* headings, int headingCount) {
    DWORD hash = 2166136261u;

    if (!headings || headingCount <= 0) {
        return (hash ^ 0u) * 16777619u;
    }

    hash = (hash ^ (DWORD)headingCount) * 16777619u;
    for (int i = 0; i < headingCount; ++i) {
        hash = (hash ^ (DWORD)headings[i].level) * 16777619u;
        hash = (hash ^ (DWORD)headings[i].startPos) * 16777619u;
        hash = (hash ^ (DWORD)headings[i].endPos) * 16777619u;
    }

    return hash;
}

static DWORD ComputeFontTagSignature(const MarkdownFontTag* fontTags, int fontTagCount) {
    DWORD hash = 2166136261u;

    if (!fontTags || fontTagCount <= 0) {
        return (hash ^ 0u) * 16777619u;
    }

    hash = (hash ^ (DWORD)fontTagCount) * 16777619u;
    for (int i = 0; i < fontTagCount; ++i) {
        hash = (hash ^ (DWORD)fontTags[i].startPos) * 16777619u;
        hash = (hash ^ (DWORD)fontTags[i].endPos) * 16777619u;
        const wchar_t* p = fontTags[i].fontName;
        while (p && *p) {
            hash = (hash ^ (DWORD)*p++) * 16777619u;
        }
    }

    return hash;
}

static void ClearMarkdownRenderCache(void) {
    if (g_markdownRenderCache.links) {
        FreeMarkdownLinks(g_markdownRenderCache.links, g_markdownRenderCache.linkCount);
    }
    free(g_markdownRenderCache.headings);
    free(g_markdownRenderCache.styles);
    free(g_markdownRenderCache.listItems);
    free(g_markdownRenderCache.blockquotes);
    free(g_markdownRenderCache.colorTags);
    free(g_markdownRenderCache.fontTags);
    free(g_markdownRenderCache.mdText);
    ZeroMemory(&g_markdownRenderCache, sizeof(g_markdownRenderCache));
}

static void ClearPluginPaintCache(void) {
    if (g_pluginPaintCache.images) {
        FreeMarkdownImages(g_pluginPaintCache.images, g_pluginPaintCache.imageCount);
    }
    ZeroMemory(&g_pluginPaintCache, sizeof(g_pluginPaintCache));
}

static void CopyCachedWideText(wchar_t* dest, size_t destCount,
                               size_t* storedLen, const wchar_t* src) {
    if (!dest || destCount == 0) {
        if (storedLen) {
            *storedLen = src ? wcslen(src) : 0;
        }
        return;
    }

    size_t srcLen = src ? wcslen(src) : 0;
    if (storedLen) {
        *storedLen = srcLen;
    }

    if (!src) {
        dest[0] = L'\0';
        return;
    }

    wcsncpy(dest, src, destCount - 1);
    dest[destCount - 1] = L'\0';
}

static BOOL CachedWideTextEquals(const wchar_t* cached, size_t cachedCount,
                                 size_t storedLen, const wchar_t* text) {
    if (!cached || cachedCount == 0 || !text) return FALSE;

    size_t textLen = wcslen(text);
    if (storedLen != textLen) {
        return FALSE;
    }
    if (textLen >= cachedCount) {
        return FALSE;
    }

    return wcsncmp(cached, text, textLen) == 0 &&
           cached[textLen] == L'\0';
}

static BOOL BuildStableDigitMeasureText(const wchar_t* source, wchar_t* dest, size_t destCount) {
    if (!source || !dest || destCount == 0) {
        return FALSE;
    }

    BOOL changed = FALSE;
    size_t i = 0;
    for (; source[i] && i + 1 < destCount; ++i) {
        wchar_t ch = source[i];
        if (ch >= L'0' && ch <= L'9') {
            dest[i] = L'8';
            changed = TRUE;
        } else {
            dest[i] = ch;
        }
    }
    dest[i] = L'\0';

    return changed;
}

static void StabilizeScaleGestureText(HWND hwnd, wchar_t* text, size_t textCount) {
    if (!text || textCount == 0) return;

    DWORD gestureSerial = GetScaleWindowVisualSerial(hwnd);
    if (gestureSerial == 0) {
        ZeroMemory(&g_scaleGestureTextCache, sizeof(g_scaleGestureTextCache));
        return;
    }

    if (g_scaleGestureTextCache.valid &&
        g_scaleGestureTextCache.hwnd == hwnd &&
        g_scaleGestureTextCache.gestureSerial == gestureSerial) {
        wcscpy_s(text, textCount, g_scaleGestureTextCache.text);
        return;
    }

    g_scaleGestureTextCache.valid = TRUE;
    g_scaleGestureTextCache.hwnd = hwnd;
    g_scaleGestureTextCache.gestureSerial = gestureSerial;
    wcscpy_s(g_scaleGestureTextCache.text,
             _countof(g_scaleGestureTextCache.text),
             text);
}

static BOOL CanUseMeasureCacheForFontTags(const MarkdownFontTag* fontTags, int fontTagCount) {
    if (!fontTags || fontTagCount <= 0) {
        return TRUE;
    }

    int uniqueCount = 0;
    for (int i = 0; i < fontTagCount; i++) {
        BOOL seen = FALSE;
        for (int j = 0; j < i; j++) {
            if (wcscmp(fontTags[i].fontName, fontTags[j].fontName) == 0) {
                seen = TRUE;
                break;
            }
        }
        if (seen) {
            continue;
        }

        if (++uniqueCount > MAX_CACHED_FONTS) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL RefreshMeasureCacheFontTags(const MarkdownFontTag* fontTags, int fontTagCount) {
    if (!fontTags || fontTagCount <= 0) {
        return TRUE;
    }

    if (!CanUseMeasureCacheForFontTags(fontTags, fontTagCount)) {
        return FALSE;
    }

    if (!BeginFontUseSTB()) {
        return FALSE;
    }

    for (int i = 0; i < fontTagCount; i++) {
        BOOL seen = FALSE;
        for (int j = 0; j < i; j++) {
            if (wcscmp(fontTags[i].fontName, fontTags[j].fontName) == 0) {
                seen = TRUE;
                break;
            }
        }
        if (!seen) {
            (void)GetCachedFontSTB(fontTags[i].fontName);
        }
    }

    EndFontUseSTB();
    return TRUE;
}

static void EnsureMarkdownRenderCache(const wchar_t* text) {
    if (!text) {
        ClearMarkdownRenderCache();
        return;
    }

    if (g_markdownRenderCache.valid &&
        CachedWideTextEquals(g_markdownRenderCache.sourceText,
                             _countof(g_markdownRenderCache.sourceText),
                             g_markdownRenderCache.sourceTextLen,
                             text)) {
        return;
    }

    ClearMarkdownRenderCache();
    CopyCachedWideText(g_markdownRenderCache.sourceText,
                       _countof(g_markdownRenderCache.sourceText),
                       &g_markdownRenderCache.sourceTextLen,
                       text);

    if (!HasPotentialMarkdownSyntax(text)) {
        g_markdownRenderCache.valid = TRUE;
        return;
    }

    BOOL parsedMarkdown = ParseMarkdownLinks(
        text,
        &g_markdownRenderCache.mdText,
        &g_markdownRenderCache.links, &g_markdownRenderCache.linkCount,
        &g_markdownRenderCache.headings, &g_markdownRenderCache.headingCount,
        &g_markdownRenderCache.styles, &g_markdownRenderCache.styleCount,
        &g_markdownRenderCache.listItems, &g_markdownRenderCache.listItemCount,
        &g_markdownRenderCache.blockquotes, &g_markdownRenderCache.blockquoteCount,
        &g_markdownRenderCache.colorTags, &g_markdownRenderCache.colorTagCount,
        &g_markdownRenderCache.fontTags, &g_markdownRenderCache.fontTagCount
    );
    g_markdownRenderCache.isMarkdown = parsedMarkdown;

    if (!parsedMarkdown) {
        if (g_markdownRenderCache.links) {
            FreeMarkdownLinks(g_markdownRenderCache.links, g_markdownRenderCache.linkCount);
            g_markdownRenderCache.links = NULL;
            g_markdownRenderCache.linkCount = 0;
        }
        free(g_markdownRenderCache.headings);
        free(g_markdownRenderCache.styles);
        free(g_markdownRenderCache.listItems);
        free(g_markdownRenderCache.blockquotes);
        free(g_markdownRenderCache.colorTags);
        free(g_markdownRenderCache.fontTags);
        free(g_markdownRenderCache.mdText);
        g_markdownRenderCache.headings = NULL;
        g_markdownRenderCache.styles = NULL;
        g_markdownRenderCache.listItems = NULL;
        g_markdownRenderCache.blockquotes = NULL;
        g_markdownRenderCache.colorTags = NULL;
        g_markdownRenderCache.fontTags = NULL;
        g_markdownRenderCache.mdText = NULL;
        g_markdownRenderCache.headingCount = 0;
        g_markdownRenderCache.styleCount = 0;
        g_markdownRenderCache.listItemCount = 0;
        g_markdownRenderCache.blockquoteCount = 0;
        g_markdownRenderCache.colorTagCount = 0;
        g_markdownRenderCache.fontTagCount = 0;
        g_markdownRenderCache.valid = TRUE;
        return;
    }

    g_markdownRenderCache.valid = TRUE;
}

/**
 * @note Static buffers avoid per-frame allocation
 */
static BOOL ExpandFontPathEnvironmentUtf8(const char* fontFileName, char* outPath, size_t outPathSize) {
    if (!fontFileName || !outPath || outPathSize == 0) return FALSE;

    if (_strnicmp(fontFileName, "%LOCALAPPDATA%", strlen("%LOCALAPPDATA%")) == 0) {
        return ExpandEffectiveLocalAppDataPath(fontFileName, outPath, outPathSize);
    }

    wchar_t fontFileNameW[MAX_PATH] = {0};
    wchar_t expandedW[MAX_PATH] = {0};
    if (!Utf8ToWide(fontFileName, fontFileNameW, MAX_PATH)) {
        return FALSE;
    }

    DWORD expandedLen = ExpandEnvironmentStringsW(fontFileNameW, expandedW, _countof(expandedW));
    if (expandedLen == 0 || expandedLen >= _countof(expandedW)) {
        return FALSE;
    }

    return WideToUtf8(expandedW, outPath, outPathSize);
}

static BOOL ResolveFontPathFromName(const char* fontFileName, char* outPath) {
    if (!fontFileName || !outPath) return FALSE;

    const char* relPath = ExtractRelativePath(fontFileName);
    if (relPath) {
        return BuildFullFontPath(relPath, outPath, MAX_PATH);
    }

    if (ExpandFontPathEnvironmentUtf8(fontFileName, outPath, MAX_PATH)) {
        if (!strchr(outPath, ':')) {
            char simpleName[MAX_PATH];
            strcpy_s(simpleName, MAX_PATH, outPath);
            return BuildFullFontPath(simpleName, outPath, MAX_PATH);
        }
        return TRUE;
    }

    outPath[0] = '\0';
    return FALSE;
}

static BOOL ResolveFontPathFromNameCached(const char* fontFileName,
                                          char* outPath,
                                          size_t outPathSize) {
    if (!fontFileName || !outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';

    if (g_fontPathResolveCache.valid &&
        strcmp(g_fontPathResolveCache.fontFileName, fontFileName) == 0) {
        if (!g_fontPathResolveCache.resolved) {
            DWORD now = GetTickCount();
            if ((LONG)(g_fontPathResolveCache.retryAfterFailureTick - now) > 0) {
                return FALSE;
            }
        } else {
            if (strlen(g_fontPathResolveCache.absoluteFontPath) >= outPathSize) {
                return FALSE;
            }
            strcpy_s(outPath, outPathSize, g_fontPathResolveCache.absoluteFontPath);
            return TRUE;
        }
    }

    char resolvedPath[MAX_PATH] = {0};
    BOOL resolved = ResolveFontPathFromName(fontFileName, resolvedPath);

    ZeroMemory(&g_fontPathResolveCache, sizeof(g_fontPathResolveCache));
    g_fontPathResolveCache.valid = TRUE;
    strcpy_s(g_fontPathResolveCache.fontFileName,
             sizeof(g_fontPathResolveCache.fontFileName),
             fontFileName);
    g_fontPathResolveCache.resolved = resolved;
    if (resolved) {
        strcpy_s(g_fontPathResolveCache.absoluteFontPath,
                 sizeof(g_fontPathResolveCache.absoluteFontPath),
                 resolvedPath);
    } else {
        DWORD retryAfter = GetTickCount() + FONT_PATH_RESOLVE_FAILURE_RETRY_MS;
        g_fontPathResolveCache.retryAfterFailureTick = retryAfter ? retryAfter : 1;
    }

    if (!resolved) {
        return FALSE;
    }

    if (strlen(resolvedPath) >= outPathSize) {
        return FALSE;
    }
    strcpy_s(outPath, outPathSize, resolvedPath);
    return TRUE;
}

static void CreateRenderContext(RenderContext* ctx) {
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

static BOOL MeasureTextMarkdown(const wchar_t* text, const RenderContext* ctx, SIZE* outSize,
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

static void ReleaseRenderDibCache(void);
static void ReleaseScaleFrameSnapshot(void);

static BOOL RenderTextMarkdown(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode, void* bits,
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

static BOOL CalculatePixelCount(int width, int height, size_t* pixelCount) {
    if (!pixelCount || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD)) return FALSE;

    *pixelCount = (size_t)width * (size_t)height;
    return TRUE;
}

static UINT GetRenderAnimationTimerInterval(size_t pixelCount, BOOL hasColorTagGradient) {
    if (GetActiveEffect() == EFFECT_TYPE_AQUA) {
        (void)pixelCount;
        return 120u;
    }

    if (hasColorTagGradient) {
        return (pixelCount < 50000u) ? 33u :
               (pixelCount < 200000u) ? 50u : 80u;
    }

    return (pixelCount < 50000u) ? 33u :
           (pixelCount < 200000u) ? 50u :
           (pixelCount < 500000u) ? 80u : 120u;
}

static UINT ChooseRenderTimerResolutionMs(UINT intervalMs) {
    UINT resolution = intervalMs / 2u;
    if (resolution < RENDER_TIMER_RESOLUTION_MIN_MS) {
        resolution = RENDER_TIMER_RESOLUTION_MIN_MS;
    }
    if (resolution > RENDER_TIMER_RESOLUTION_MAX_MS) {
        resolution = RENDER_TIMER_RESOLUTION_MAX_MS;
    }
    return resolution;
}

static UINT ClampRenderTimerResolutionToDeviceCaps(UINT requestedMs) {
    TIMECAPS caps;
    if (timeGetDevCaps(&caps, sizeof(caps)) != TIMERR_NOERROR) {
        return requestedMs;
    }

    if (requestedMs < caps.wPeriodMin) {
        return caps.wPeriodMin;
    }
    if (requestedMs > caps.wPeriodMax) {
        return caps.wPeriodMax;
    }
    return requestedMs;
}

static void ReleaseRenderAnimationTimerResolution(void) {
    if (s_renderAnimationTimerResolutionMs > 0) {
        timeEndPeriod(s_renderAnimationTimerResolutionMs);
        s_renderAnimationTimerResolutionMs = 0;
    }
}

static void UpdateRenderAnimationTimerResolution(UINT interval) {
    UINT requestedResolutionMs = 0;

    if (interval <= 25u) {
        requestedResolutionMs = ClampRenderTimerResolutionToDeviceCaps(
            ChooseRenderTimerResolutionMs(interval));
    }

    if (s_renderAnimationTimerResolutionMs == requestedResolutionMs) {
        return;
    }

    ReleaseRenderAnimationTimerResolution();

    if (requestedResolutionMs > 0) {
        MMRESULT res = timeBeginPeriod(requestedResolutionMs);
        if (res == TIMERR_NOERROR) {
            s_renderAnimationTimerResolutionMs = requestedResolutionMs;
        } else {
            WriteLog(LOG_LEVEL_WARNING,
                     "Failed to set render animation timer resolution (resolution=%u)",
                     requestedResolutionMs);
        }
    }
}

static BOOL SetDrawingRenderAnimationTimer(HWND hwnd, UINT interval) {
    if (!IsValidRenderAnimationWindow(hwnd) || interval == 0) {
        StopDrawingRenderAnimationTimer(NULL);
        return FALSE;
    }

    if (s_renderAnimationTimerActive &&
        s_renderAnimationTimerHwnd == hwnd &&
        s_renderAnimationTimerInterval == interval) {
        return TRUE;
    }

    if (!SetTimer(hwnd, TIMER_ID_RENDER_ANIMATION, interval, NULL)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to set render animation timer (interval=%u, error=%lu)",
                 interval, GetLastError());
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    HWND previousHwnd = s_renderAnimationTimerHwnd;
    BOOL hadPreviousTimer = s_renderAnimationTimerActive;
    if (hadPreviousTimer && previousHwnd != hwnd) {
        HWND killHwnd = IsValidRenderAnimationWindow(previousHwnd) ? previousHwnd : NULL;
        if (killHwnd) {
            KillTimer(killHwnd, TIMER_ID_RENDER_ANIMATION);
        }
    }

    s_renderAnimationTimerActive = TRUE;
    s_renderAnimationTimerInterval = interval;
    s_renderAnimationTimerHwnd = hwnd;
    UpdateRenderAnimationTimerResolution(interval);
    return TRUE;
}

static BOOL IsActiveTextColorAnimated(void) {
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));

    static char s_lastActiveColor[COLOR_HEX_BUFFER] = {0};
    static BOOL s_lastActiveColorAnimated = FALSE;

    if (strcmp(activeColor, s_lastActiveColor) == 0) {
        return s_lastActiveColorAnimated;
    }

    strncpy_s(s_lastActiveColor, sizeof(s_lastActiveColor), activeColor, _TRUNCATE);
    s_lastActiveColorAnimated = IsGradientNameAnimated(activeColor);
    return s_lastActiveColorAnimated;
}

static BOOL ShouldRunRenderAnimationTimer(BOOL hasRenderableContent,
                                          BOOL hasColorTagGradient) {
    /* Holographic is a static prism/glow pass; only liquid and animated
     * gradients need a render-only timer.
     */
    if (!hasRenderableContent) {
        return FALSE;
    }

    EffectType activeEffect = GetActiveEffect();
    return TextEffect_NeedsRenderTimer(activeEffect) ||
           hasColorTagGradient ||
           IsActiveTextColorAnimated();
}

static BOOL UpdateDrawingRenderAnimationTimerForFrame(HWND hwnd,
                                                      BOOL hasRenderableContent,
                                                      BOOL hasColorTagGradient) {
    if (!IsValidRenderAnimationWindow(hwnd)) {
        StopDrawingRenderAnimationTimer(NULL);
        return FALSE;
    }

    /* Scale frames already repaint at the gesture cadence. Running the
     * independent effect timer at the same time only queues duplicate paints
     * and makes wheel input feel uneven on slower machines. */
    if (GetScaleWindowGestureSerial(hwnd) != 0) {
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    if (!IsWindowVisible(hwnd) ||
        !ShouldRunRenderAnimationTimer(hasRenderableContent, hasColorTagGradient)) {
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    size_t pixelCount = 0;
    if (hwnd) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        CalculatePixelCount(rect.right, rect.bottom, &pixelCount);
    }

    return SetDrawingRenderAnimationTimer(
        hwnd,
        GetRenderAnimationTimerInterval(pixelCount, hasColorTagGradient));
}

void StopDrawingRenderAnimationTimer(HWND hwnd) {
    HWND trackedHwnd = s_renderAnimationTimerHwnd;

    if (IsValidRenderAnimationWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_RENDER_ANIMATION);
    }

    if (trackedHwnd != hwnd && IsValidRenderAnimationWindow(trackedHwnd)) {
        KillTimer(trackedHwnd, TIMER_ID_RENDER_ANIMATION);
    }

    s_renderAnimationTimerActive = FALSE;
    s_renderAnimationTimerInterval = 0;
    s_renderAnimationTimerHwnd = NULL;
    ReleaseRenderAnimationTimerResolution();
}

static int ClampRenderInt64(long long value, int minValue, int maxValue) {
    if (value < (long long)minValue) return minValue;
    if (value > (long long)maxValue) return maxValue;
    return (int)value;
}

static int AddRenderDimensionClamped(int value, int delta) {
    return ClampRenderInt64((long long)value + (long long)delta, 0, INT_MAX);
}

static BOOL GetRenderWindowLimits(HWND hwnd, SIZE* outLimits) {
    if (!outLimits) return FALSE;

    int maxWindowWidth = MAX_RENDER_DIB_DIMENSION;
    int maxWindowHeight = MAX_RENDER_DIB_DIMENSION;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo;
    ZeroMemory(&monitorInfo, sizeof(monitorInfo));
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        if (workWidth > 0 && workWidth < maxWindowWidth) {
            maxWindowWidth = workWidth;
        }
        if (workHeight > 0 && workHeight < maxWindowHeight) {
            maxWindowHeight = workHeight;
        }
    }

    if (maxWindowWidth <= 0 || maxWindowHeight <= 0) {
        return FALSE;
    }

    size_t maxPixels = 0;
    if (!CalculatePixelCount(maxWindowWidth, maxWindowHeight, &maxPixels)) {
        return FALSE;
    }
    if (maxPixels > MAX_RENDER_DIB_PIXELS) {
        size_t limitedHeight = MAX_RENDER_DIB_PIXELS / (size_t)maxWindowWidth;
        maxWindowHeight = (limitedHeight > 0 && limitedHeight < (size_t)INT_MAX)
            ? (int)limitedHeight
            : 1;
    }

    outLimits->cx = maxWindowWidth;
    outLimits->cy = maxWindowHeight;
    return TRUE;
}

static BOOL GetConstrainedRenderWindowSize(HWND hwnd, const SIZE* contentSize, SIZE* outSize) {
    if (!contentSize || !outSize || contentSize->cx <= 0 || contentSize->cy <= 0) {
        return FALSE;
    }

    SIZE limits;
    if (!GetRenderWindowLimits(hwnd, &limits)) {
        return FALSE;
    }

    int requestedWidth = ClampRenderInt64((long long)contentSize->cx + WINDOW_HORIZONTAL_PADDING,
                                          1, limits.cx);
    int requestedHeight = ClampRenderInt64((long long)contentSize->cy + WINDOW_VERTICAL_PADDING,
                                           1, limits.cy);

    outSize->cx = requestedWidth;
    outSize->cy = requestedHeight;
    return TRUE;
}

static void FreePaintMarkdownImages(MarkdownImage* images, int imageCount, BOOL heapAllocated) {
    if (!images) return;

    if (heapAllocated) {
        FreeMarkdownImages(images, imageCount);
    } else {
        FreeMarkdownImageEntries(images, imageCount);
    }
}

static BOOL HasTickReached(DWORD tick) {
    return (DWORD)(GetTickCount() - tick) < 0x80000000u;
}

static BOOL IsMarkdownImageRetryPending(const MarkdownImage* image) {
    return image && image->downloadFailed && image->downloadRetryScheduled &&
           !HasTickReached(image->downloadRetryTick);
}

static void PreparePaintMarkdownImagesForFrame(MarkdownImage* images, int imageCount) {
    if (!images || imageCount <= 0) return;

    DWORD now = GetTickCount();
    BOOL checkResolvedFiles = s_nextMarkdownImageFileCheckTick == 0 ||
                              HasTickReached(s_nextMarkdownImageFileCheckTick);
    if (checkResolvedFiles) {
        s_nextMarkdownImageFileCheckTick = now + MARKDOWN_IMAGE_FILE_RECHECK_MS;
    }

    for (int i = 0; i < imageCount; i++) {
        SetRectEmpty(&images[i].imageRect);

        if (checkResolvedFiles &&
            images[i].resolvedPath &&
            !RefreshMarkdownImageResolvedFileState(&images[i])) {
            free(images[i].resolvedPath);
            images[i].resolvedPath = NULL;
            images[i].isDownloaded = FALSE;
            images[i].intrinsicWidth = 0;
            images[i].intrinsicHeight = 0;
            ZeroMemory(&images[i].resolvedLastWriteTime,
                       sizeof(images[i].resolvedLastWriteTime));
            images[i].resolvedFileSize = 0;
            images[i].resolvedFileInfoValid = FALSE;
        }

        if (images[i].isNetworkImage) {
            if (!images[i].isDownloaded) {
                images[i].intrinsicWidth = 0;
                images[i].intrinsicHeight = 0;

                if (images[i].isDownloading &&
                    !IsMarkdownImageDownloadInProgress(images[i].imagePath)) {
                    images[i].isDownloading = FALSE;
                    DWORD retryTick = 0;
                    if (GetMarkdownImageDownloadRetryTick(images[i].imagePath, &retryTick)) {
                        images[i].downloadFailed = TRUE;
                        images[i].downloadRetryScheduled = TRUE;
                        images[i].downloadRetryTick = retryTick;
                    }
                }

                if (IsMarkdownImageRetryPending(&images[i])) {
                    continue;
                }

                if (images[i].downloadRetryScheduled &&
                    HasTickReached(images[i].downloadRetryTick)) {
                    images[i].downloadRetryScheduled = FALSE;
                    images[i].downloadRetryTick = 0;
                    images[i].downloadFailed = FALSE;
                } else if (!images[i].downloadRetryScheduled) {
                    images[i].downloadFailed = FALSE;
                }
            }
        }
    }
}

static BOOL EnsurePaintMarkdownImageCapacity(MarkdownImage** images,
                                             int* imageCapacity,
                                             BOOL* heapAllocated,
                                             MarkdownImage* stackImages) {
    if (!images || !*images || !imageCapacity || !heapAllocated || !stackImages) {
        return FALSE;
    }

    int oldCapacity = *imageCapacity;
    if (oldCapacity <= 0 ||
        (size_t)oldCapacity > ((size_t)-1) / 2u / sizeof(MarkdownImage)) {
        return FALSE;
    }

    int newCapacity = oldCapacity * 2;
    MarkdownImage* newImages = NULL;

    if (*heapAllocated) {
        newImages = (MarkdownImage*)realloc(*images, (size_t)newCapacity * sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        ZeroMemory(newImages + oldCapacity, (size_t)(newCapacity - oldCapacity) * sizeof(MarkdownImage));
    } else {
        newImages = (MarkdownImage*)calloc((size_t)newCapacity, sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        memcpy(newImages, stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        ZeroMemory(stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        *heapAllocated = TRUE;
    }

    *images = newImages;
    *imageCapacity = newCapacity;
    return TRUE;
}

static MarkdownImage* MovePaintMarkdownImagesToHeap(MarkdownImage* images,
                                                    int imageCount,
                                                    BOOL* heapAllocated,
                                                    MarkdownImage* stackImages) {
    if (!images || imageCount <= 0 || !heapAllocated) {
        return NULL;
    }

    if (*heapAllocated) {
        *heapAllocated = FALSE;
        return images;
    }

    MarkdownImage* heapImages =
        (MarkdownImage*)calloc((size_t)imageCount, sizeof(MarkdownImage));
    if (!heapImages) {
        return NULL;
    }

    memcpy(heapImages, images, (size_t)imageCount * sizeof(MarkdownImage));
    if (images == stackImages && stackImages) {
        ZeroMemory(stackImages, (size_t)imageCount * sizeof(MarkdownImage));
    }
    return heapImages;
}

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

static RenderDibCache g_renderDibCache = {0};

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

static ScaleFrameSnapshot g_scaleFrameSnapshot = {0};

static void ReleaseScaleFrameSnapshot(void) {
    if (g_scaleFrameSnapshot.memDC && g_scaleFrameSnapshot.oldBitmap) {
        SelectObject(g_scaleFrameSnapshot.memDC,
                     g_scaleFrameSnapshot.oldBitmap);
    }
    if (g_scaleFrameSnapshot.memBitmap) {
        DeleteObject(g_scaleFrameSnapshot.memBitmap);
    }
    if (g_scaleFrameSnapshot.memDC) {
        DeleteDC(g_scaleFrameSnapshot.memDC);
    }
    ZeroMemory(&g_scaleFrameSnapshot, sizeof(g_scaleFrameSnapshot));
}

static BOOL CreateScaleFrameSnapshotSurface(HDC referenceDC,
                                            int width,
                                            int height) {
    if (!referenceDC || width <= 0 || height <= 0) {
        return FALSE;
    }

    size_t pixelCount = 0;
    if (!CalculatePixelCount(width, height, &pixelCount) ||
        pixelCount > MAX_RENDER_DIB_PIXELS) {
        return FALSE;
    }

    HDC memDC = CreateCompatibleDC(referenceDC);
    if (!memDC) {
        return FALSE;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP bitmap = CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS,
                                      &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    if (!oldBitmap || oldBitmap == (HBITMAP)HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    g_scaleFrameSnapshot.memDC = memDC;
    g_scaleFrameSnapshot.memBitmap = bitmap;
    g_scaleFrameSnapshot.oldBitmap = oldBitmap;
    g_scaleFrameSnapshot.bits = bits;
    g_scaleFrameSnapshot.width = width;
    g_scaleFrameSnapshot.height = height;
    return TRUE;
}

static BOOL TryCaptureScaleFrameSnapshot(HWND hwnd,
                                         HDC referenceDC,
                                         DWORD gestureSerial,
                                         const RECT* currentRect) {
    if (!hwnd || !referenceDC || gestureSerial == 0 || !currentRect) {
        return FALSE;
    }

    if (g_scaleFrameSnapshot.hwnd == hwnd &&
        g_scaleFrameSnapshot.gestureSerial == gestureSerial &&
        g_scaleFrameSnapshot.memDC && g_scaleFrameSnapshot.bits) {
        return TRUE;
    }

    ReleaseScaleFrameSnapshot();

    int width = currentRect->right - currentRect->left;
    int height = currentRect->bottom - currentRect->top;
    size_t pixelCount = 0;
    if (!CalculatePixelCount(width, height, &pixelCount) ||
        pixelCount < SCALE_SNAPSHOT_MIN_PIXELS ||
        !g_renderDibCache.frameValid ||
        g_renderDibCache.frameWasScaleComposite ||
        !g_renderDibCache.frameEditMode ||
        g_renderDibCache.frameHwnd != hwnd ||
        g_renderDibCache.frameWidth != width ||
        g_renderDibCache.frameHeight != height ||
        !g_renderDibCache.bits) {
        return FALSE;
    }

    if (!CreateScaleFrameSnapshotSurface(referenceDC, width, height)) {
        return FALSE;
    }

    memcpy(g_scaleFrameSnapshot.bits,
           g_renderDibCache.bits,
           pixelCount * sizeof(DWORD));
    g_scaleFrameSnapshot.hwnd = hwnd;
    g_scaleFrameSnapshot.gestureSerial = gestureSerial;
    return TRUE;
}

static BOOL CompositeScaleFrameSnapshot(HWND hwnd,
                                        DWORD gestureSerial,
                                        HDC destDC,
                                        void* destBits,
                                        int destWidth,
                                        int destHeight) {
    if (!hwnd || gestureSerial == 0 || !destDC || !destBits ||
        destWidth <= 0 || destHeight <= 0 ||
        g_scaleFrameSnapshot.hwnd != hwnd ||
        g_scaleFrameSnapshot.gestureSerial != gestureSerial ||
        !g_scaleFrameSnapshot.memDC || !g_scaleFrameSnapshot.bits) {
        return FALSE;
    }

    size_t pixelCount = 0;
    if (!CalculatePixelCount(destWidth, destHeight, &pixelCount)) {
        return FALSE;
    }
    ZeroMemory(destBits, pixelCount * sizeof(DWORD));

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    return AlphaBlend(destDC,
                      0, 0, destWidth, destHeight,
                      g_scaleFrameSnapshot.memDC,
                      0, 0,
                      g_scaleFrameSnapshot.width,
                      g_scaleFrameSnapshot.height,
                      blend);
}

static void ReleaseRenderDibCache(void) {
    if (g_renderDibCache.memDC && g_renderDibCache.oldBitmap) {
        SelectObject(g_renderDibCache.memDC, g_renderDibCache.oldBitmap);
    }
    if (g_renderDibCache.memBitmap) {
        DeleteObject(g_renderDibCache.memBitmap);
    }
    if (g_renderDibCache.memDC) {
        DeleteDC(g_renderDibCache.memDC);
    }
    ZeroMemory(&g_renderDibCache, sizeof(g_renderDibCache));
}

static BOOL ShouldReuseRenderDibCache(int width, int height, size_t requiredPixels) {
    size_t cachedPixels = 0;
    if (!g_renderDibCache.memDC || !g_renderDibCache.memBitmap || !g_renderDibCache.bits) {
        return FALSE;
    }
    if (g_renderDibCache.width != width || g_renderDibCache.height < height) {
        return FALSE;
    }
    if (!CalculatePixelCount(g_renderDibCache.width, g_renderDibCache.height, &cachedPixels)) {
        return FALSE;
    }
    if (requiredPixels > 0 &&
        cachedPixels / RENDER_DIB_SHRINK_THRESHOLD_MULTIPLIER > requiredPixels) {
        return FALSE;
    }
    return TRUE;
}

/** @note GM_ADVANCED + HALFTONE improve text quality on high-DPI displays */
static BOOL SetupDoubleBufferDIB(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap, void** ppvBits) {
    size_t pixelCount;
    if (!rect || !CalculatePixelCount(rect->right, rect->bottom, &pixelCount)) {
        return FALSE;
    }
    if (pixelCount > MAX_RENDER_DIB_PIXELS) {
        WriteLog(LOG_LEVEL_WARNING, "Render DIB too large: %dx%d", rect->right, rect->bottom);
        return FALSE;
    }

    if (ShouldReuseRenderDibCache(rect->right, rect->bottom, pixelCount)) {
        *memDC = g_renderDibCache.memDC;
        *memBitmap = g_renderDibCache.memBitmap;
        *oldBitmap = g_renderDibCache.oldBitmap;
        *ppvBits = g_renderDibCache.bits;
        return TRUE;
    }

    HDC newMemDC = CreateCompatibleDC(hdc);
    if (!newMemDC) {
        return FALSE;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect->right;
    // Negative height creates a top-down DIB, matching STB's coordinate system
    bmi.bmiHeader.biHeight = -rect->bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* newBits = NULL;
    HBITMAP newBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &newBits, NULL, 0);
    if (!newBitmap || !newBits) {
        if (newBitmap) DeleteObject(newBitmap);
        DeleteDC(newMemDC);
        return FALSE;
    }

    HBITMAP newOldBitmap = (HBITMAP)SelectObject(newMemDC, newBitmap);
    if (!newOldBitmap) {
        DeleteObject(newBitmap);
        DeleteDC(newMemDC);
        return FALSE;
    }

    SetGraphicsMode(newMemDC, GM_ADVANCED);
    SetBkMode(newMemDC, TRANSPARENT);
    SetStretchBltMode(newMemDC, HALFTONE);
    SetBrushOrgEx(newMemDC, 0, 0, NULL);
    SetTextAlign(newMemDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(newMemDC, 0);
    SetMapMode(newMemDC, MM_TEXT);
    SetICMMode(newMemDC, ICM_ON);
    SetLayout(newMemDC, 0);

    ReleaseRenderDibCache();

    g_renderDibCache.memDC = newMemDC;
    g_renderDibCache.memBitmap = newBitmap;
    g_renderDibCache.oldBitmap = newOldBitmap;
    g_renderDibCache.bits = newBits;
    g_renderDibCache.width = rect->right;
    g_renderDibCache.height = rect->bottom;

    *memDC = g_renderDibCache.memDC;
    *memBitmap = g_renderDibCache.memBitmap;
    *oldBitmap = g_renderDibCache.oldBitmap;
    *ppvBits = g_renderDibCache.bits;

    return TRUE;
}

/**
 * @brief Manually set alpha channel to opaque for non-black pixels
 * @details GDI text drawing leaves alpha channel as 0, which DWM treats as transparent.
 *          We iterate pixels to set Alpha=255 where RGB != 0.
 */
static void FixAlphaChannel(void* bits, int width, int height) {
    if (!bits) return;

    DWORD* pixels = (DWORD*)bits;
    size_t count;
    if (!CalculatePixelCount(width, height, &count)) return;

    for (size_t i = 0; i < count; i++) {
        // Check if RGB is not black (0x00RRGGBB)
        if ((pixels[i] & 0x00FFFFFF) != 0) {
            // Only set Alpha to 255 if it's currently 0 (meaning it was drawn by GDI without alpha)
            if ((pixels[i] & 0xFF000000) == 0) {
                pixels[i] |= 0xFF000000;
            }
        } else {
            // Ensure black background is transparent
            pixels[i] &= 0x00FFFFFF;
        }
    }
}

/** @note Skips resize if size unchanged to reduce SetWindowPos overhead */
static void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect) {
    SIZE targetSize;
    DWORD scaleGestureSerial = GetScaleWindowGestureSerial(hwnd);
    if (CLOCK_IS_DRAGGING && scaleGestureSerial == 0) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    if (!GetConstrainedRenderWindowSize(hwnd, textSize, &targetSize)) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    POINT resizeAnchor = {0};
    double resizeAnchorRatioX = 0.5;
    double resizeAnchorRatioY = 0.5;
    BOOL hasResizeAnchor = GetPendingScaleResizeAnchorInfo(hwnd,
                                                           &resizeAnchor,
                                                           &resizeAnchorRatioX,
                                                           &resizeAnchorRatioY);
    LONG currentClientWidth = rect->right - rect->left;
    LONG currentClientHeight = rect->bottom - rect->top;

    if (targetSize.cx == currentClientWidth &&
        targetSize.cy == currentClientHeight) {
        if (hasResizeAnchor && scaleGestureSerial == 0) {
            ConsumePendingScaleResizeAnchor(hwnd);
        } else {
            ClearPendingScaleResizeAnchor(hwnd);
        }
        return;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int newX = windowRect.left;
    int newY = windowRect.top;
    if (hasResizeAnchor) {
        newX = resizeAnchor.x - (int)(resizeAnchorRatioX * (double)targetSize.cx + 0.5);
        newY = resizeAnchor.y - (int)(resizeAnchorRatioY * (double)targetSize.cy + 0.5);
    } else {
        HMONITOR monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {0};
        RECT taskbarRect = {0};
        RECT intersection = {0};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor && GetMonitorInfo(monitor, &monitorInfo) &&
            GetTaskbarRectForMonitor(monitor, &taskbarRect) &&
            IntersectRect(&intersection, &windowRect, &taskbarRect)) {
            int axisRatio = 0;
            int crossOffset = 0;
            if (WindowPlacement_CaptureTaskbarAnchor(
                    &windowRect, &taskbarRect, &monitorInfo.rcMonitor,
                    &axisRatio, &crossOffset)) {
                WindowPlacement_ResolveTaskbarAnchor(
                    &taskbarRect, &monitorInfo.rcMonitor,
                    targetSize.cx, targetSize.cy,
                    axisRatio, crossOffset, &newX, &newY);
            }
        }
    }

    ClampWindowPositionToVisibleMonitor(targetSize.cx, targetSize.cy,
                                        &newX, &newY);

    SetWindowPos(hwnd, NULL,
        newX, newY,
        targetSize.cx,
        targetSize.cy,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    if (hasResizeAnchor && scaleGestureSerial == 0) {
        ConsumePendingScaleResizeAnchor(hwnd);
    } else {
        ClearPendingScaleResizeAnchor(hwnd);
    }

    CLOCK_WINDOW_POS_X = newX;
    CLOCK_WINDOW_POS_Y = newY;

    GetClientRect(hwnd, rect);
}

void HandleWindowPaint(HWND hwnd, const PAINTSTRUCT* ps) {
    if (CLOCK_IS_DRAGGING) {
        ReleaseScaleFrameSnapshot();
        return;
    }

    PaintTextBuffers* paintBuffers = &g_paintTextBuffers;
    wchar_t* timeText = paintBuffers->timeText;
    wchar_t* pluginText = paintBuffers->pluginText;
    wchar_t* result = paintBuffers->pluginResult;
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);
    DWORD activeScaleSerial = GetScaleWindowGestureSerial(hwnd);
    if (activeScaleSerial == 0) {
        ReleaseScaleFrameSnapshot();
    } else {
        TryCaptureScaleFrameSnapshot(hwnd, hdc, activeScaleSerial, &rect);
    }

    ClearClickableRegions();
    timeText[0] = L'\0';
    GetTimeText(timeText, TIME_TEXT_MAX_LEN);
    wcscpy_s(paintBuffers->timerTextSnapshot, TIME_TEXT_MAX_LEN, timeText);

    // Check for plugin data
    pluginText[0] = L'\0';
    MarkdownImage stackImages[PLUGIN_IMAGE_STACK_CAPACITY];
    MarkdownImage* images = NULL;
    int imageCount = 0;
    int imageCapacity = 0;
    BOOL imagesHeapAllocated = FALSE;
    BOOL imageCapacityExhausted = FALSE;
    BOOL imagesOwnedByCache = FALSE;

    if (PluginData_GetText(pluginText, TIME_TEXT_MAX_LEN)) {
        BOOL canUsePluginPaintCache = (wcsstr(pluginText, CATIME_OPEN_TAG) == NULL);

        if (canUsePluginPaintCache && g_pluginPaintCache.valid &&
            CachedWideTextEquals(g_pluginPaintCache.sourceText,
                                 _countof(g_pluginPaintCache.sourceText),
                                 g_pluginPaintCache.sourceTextLen,
                                 pluginText)) {
            wcscpy_s(timeText, TIME_TEXT_MAX_LEN, g_pluginPaintCache.renderedText);
            images = g_pluginPaintCache.images;
            imageCount = g_pluginPaintCache.imageCount;
            imagesOwnedByCache = TRUE;
        } else {
        wchar_t savedTime[256];
        wcsncpy(savedTime, timeText, _countof(savedTime) - 1);
        savedTime[_countof(savedTime) - 1] = L'\0';
        size_t savedTimeLen = wcslen(savedTime);

        ZeroMemory(stackImages, sizeof(stackImages));
        images = stackImages;
        imageCapacity = PLUGIN_IMAGE_STACK_CAPACITY;

        // Replace ALL <catime></catime> tags and extract ![](path) images in one pass.
        result[0] = L'\0';
        const wchar_t* src = pluginText;
        wchar_t* dst = result;
        size_t remaining = TIME_TEXT_MAX_LEN - 1;

        while (*src && remaining > 0) {
            PluginTextMarkerKind markerKind = PLUGIN_TEXT_MARKER_NONE;
            const wchar_t* marker = FindNextPluginTextMarker(src, !imageCapacityExhausted, &markerKind);

            if (!marker) {
                AppendWideSpan(&dst, &remaining, src, wcslen(src));
                break;
            }

            if (marker > src) {
                AppendWideSpan(&dst, &remaining, src, (size_t)(marker - src));
                src = marker;
                if (remaining == 0) {
                    break;
                }
            }

            if (markerKind == PLUGIN_TEXT_MARKER_IMAGE && images && !imageCapacityExhausted) {
                if (imageCount >= imageCapacity &&
                    !EnsurePaintMarkdownImageCapacity(&images, &imageCapacity,
                                                      &imagesHeapAllocated, stackImages)) {
                    imageCapacityExhausted = TRUE;
                }

                if (!imageCapacityExhausted) {
                    const wchar_t* imgSrc = src;
                    if (ExtractMarkdownImage(&imgSrc, images, &imageCount, imageCapacity,
                                             (int)(dst - result))) {
                        src = imgSrc;
                        continue;
                    }
                }
            }

            if (markerKind == PLUGIN_TEXT_MARKER_CATIME) {
                const wchar_t* tagEnd = wcsstr(src + CATIME_OPEN_TAG_LEN, CATIME_CLOSE_TAG);
                if (tagEnd) {
                    AppendWideSpan(&dst, &remaining, savedTime, savedTimeLen);
                    src = tagEnd + CATIME_CLOSE_TAG_LEN;
                    continue;
                }
            }

            *dst++ = *src++;
            remaining--;
        }
        *dst = L'\0';

        wcscpy_s(timeText, TIME_TEXT_MAX_LEN, result);

        if (canUsePluginPaintCache) {
            ClearPluginPaintCache();

            CopyCachedWideText(g_pluginPaintCache.sourceText,
                               _countof(g_pluginPaintCache.sourceText),
                               &g_pluginPaintCache.sourceTextLen,
                               pluginText);
            wcscpy_s(g_pluginPaintCache.renderedText, TIME_TEXT_MAX_LEN, result);

            if (imageCount > 0) {
                g_pluginPaintCache.images =
                    MovePaintMarkdownImagesToHeap(images, imageCount,
                                                  &imagesHeapAllocated,
                                                  stackImages);

                if (g_pluginPaintCache.images) {
                    g_pluginPaintCache.imageCount = imageCount;
                    g_pluginPaintCache.valid = TRUE;
                    images = g_pluginPaintCache.images;
                    imagesOwnedByCache = TRUE;
                }
            } else {
                g_pluginPaintCache.valid = TRUE;
            }
        } else if (g_pluginPaintCache.valid &&
                   !CachedWideTextEquals(g_pluginPaintCache.sourceText,
                                         _countof(g_pluginPaintCache.sourceText),
                                         g_pluginPaintCache.sourceTextLen,
                                         pluginText)) {
            ClearPluginPaintCache();
        }
        }
    } else {
        ClearPluginPaintCache();
    }

    PreparePaintMarkdownImagesForFrame(images, imageCount);

    if (timeText[0] == L'\0') {
        GetPreviewTimeText(timeText, TIME_TEXT_MAX_LEN);
    }

    StabilizeScaleGestureText(hwnd, timeText, TIME_TEXT_MAX_LEN);

    RenderContext ctx;
    CreateRenderContext(&ctx);

    EnsureMarkdownRenderCache(timeText);

    BOOL isMarkdown = g_markdownRenderCache.isMarkdown;
    const MarkdownHeading* headings = g_markdownRenderCache.headings;
    int headingCount = g_markdownRenderCache.headingCount;
    MarkdownColorTag* colorTags = g_markdownRenderCache.colorTags;
    int colorTagCount = g_markdownRenderCache.colorTagCount;
    const MarkdownFontTag* fontTags = g_markdownRenderCache.fontTags;
    int fontTagCount = g_markdownRenderCache.fontTagCount;

    const wchar_t* textToRender = (isMarkdown && g_markdownRenderCache.mdText) ? g_markdownRenderCache.mdText : timeText;
    BOOL hasText = textToRender[0] != L'\0';
    const wchar_t* textToMeasure = textToRender;
    if (hasText &&
        !isMarkdown &&
        !PluginData_IsActive() &&
        BuildStableDigitMeasureText(textToRender,
                                    g_paintTextBuffers.measureText,
                                    _countof(g_paintTextBuffers.measureText))) {
        textToMeasure = g_paintTextBuffers.measureText;
    }

    // Measure text and resize window BEFORE creating the buffer
    // This prevents buffer overflow if the window grows
    SIZE textSize = {0};
    BOOL hasContent = hasText || (images && imageCount > 0);

    SIZE measuredTextSize = {0};
    BOOL measuredTextSizeValid = FALSE;

    if (hasContent) {
        // Measure text if any
        if (hasText) {
            if (isMarkdown) {
                measuredTextSizeValid = MeasureTextMarkdown(textToRender, &ctx, &textSize,
                                                            headings, headingCount,
                                                            fontTags, fontTagCount);
            } else {
                measuredTextSizeValid = MeasureTextMarkdown(textToMeasure, &ctx, &textSize,
                                                            NULL, 0, NULL, 0);
            }

            if (measuredTextSizeValid) {
                measuredTextSize = textSize;
            }

            // If measurement failed, use default size
            if (!measuredTextSizeValid) {
                textSize.cx = 100;
                textSize.cy = 30;
            }
        }

        // Add image dimensions to total size
        if (images && imageCount > 0) {
            SIZE renderLimits = {MAX_RENDER_DIB_DIMENSION, MAX_RENDER_DIB_DIMENSION};
            if (!GetRenderWindowLimits(hwnd, &renderLimits)) {
                renderLimits.cx = MAX_RENDER_DIB_DIMENSION;
                renderLimits.cy = MAX_RENDER_DIB_DIMENSION;
            }
            int imageMeasureMaxW = ClampRenderInt64(
                (long long)renderLimits.cx - WINDOW_HORIZONTAL_PADDING - 10,
                1, MAX_RENDER_DIB_DIMENSION);
            int imageMeasureMaxH = ClampRenderInt64(
                (long long)renderLimits.cy - WINDOW_VERTICAL_PADDING - 5,
                1, MAX_RENDER_DIB_DIMENSION);

            textSize.cy = AddRenderDimensionClamped(textSize.cy, 5);  // Small gap between text and first image

            for (int i = 0; i < imageCount; i++) {
                int renderW = 0, renderH = 0;
                if (CalculateImageRenderSize(&images[i], imageMeasureMaxW, imageMeasureMaxH,
                                             &renderW, &renderH)) {
                    renderW = AddRenderDimensionClamped(renderW, 10);  // Add padding
                    renderH = AddRenderDimensionClamped(renderH, 5);

                    if (renderW > textSize.cx) textSize.cx = renderW;
                    textSize.cy = AddRenderDimensionClamped(textSize.cy, renderH);
                } else if (images[i].isNetworkImage && !images[i].isDownloaded) {
                    // Reserve space for "Loading..." text
                    textSize.cy = AddRenderDimensionClamped(textSize.cy, 25);  // Approximate height for loading text
                }
            }
        }
        AdjustWindowSize(hwnd, &textSize, &rect);
    }

    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    void* pBits = NULL;

    // Create buffer with the final correct size
    if (!SetupDoubleBufferDIB(hdc, &rect, &memDC, &memBitmap, &oldBitmap, &pBits)) {
        if (!imagesOwnedByCache) {
            FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
        }
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return;
    }

    // Manually clear background
    // Edit Mode: Alpha=5 to capture mouse click on background
    // Normal Mode: Alpha=0 for full transparency (clickable regions filled later)
    size_t numPixels = 0;
    if (!pBits || !CalculatePixelCount(rect.right, rect.bottom, &numPixels)) {
        ReleaseRenderDibCache();
        if (!imagesOwnedByCache) {
            FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
        }
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return;
    }
    DWORD* pixels = (DWORD*)pBits;
    BOOL usedScaleComposite =
        CompositeScaleFrameSnapshot(hwnd, activeScaleSerial,
                                    memDC, pBits,
                                    rect.right, rect.bottom);

    if (!usedScaleComposite) {
        DWORD clearColor = CLOCK_EDIT_MODE ? 0x05000000 : 0x00000000;

        if (clearColor == 0) {
            ZeroMemory(pixels, numPixels * sizeof(*pixels));
        } else {
            // Edit mode needs a small non-zero alpha so the background can receive mouse input.
            for (size_t i = 0; i < numPixels; i++) {
                pixels[i] = clearColor;
            }
        }

        if (hasContent) {
            int textHeight = 0;

            // Render text if any
            if (hasText) {
                RECT textRect = rect;
                SIZE textSizeMeasured = measuredTextSize;

                if (!measuredTextSizeValid) {
                    if (isMarkdown) {
                        MeasureTextMarkdown(textToRender, &ctx, &textSizeMeasured,
                                            headings, headingCount,
                                            fontTags, fontTagCount);
                    } else {
                        MeasureTextMarkdown(textToRender, &ctx, &textSizeMeasured,
                                            NULL, 0, NULL, 0);
                    }
                }

                if (textSizeMeasured.cy > 0) {
                    textHeight = textSizeMeasured.cy;
                }

                if (isMarkdown) {
                    MarkdownLink* links = g_markdownRenderCache.links;
                    int linkCount = g_markdownRenderCache.linkCount;
                    MarkdownStyle* styles = g_markdownRenderCache.styles;
                    int styleCount = g_markdownRenderCache.styleCount;
                    MarkdownBlockquote* blockquotes = g_markdownRenderCache.blockquotes;
                    int blockquoteCount = g_markdownRenderCache.blockquoteCount;

                    RenderTextMarkdown(memDC, &textRect, textToRender, &ctx, CLOCK_EDIT_MODE, pBits,
                                      links, linkCount, headings, headingCount, styles, styleCount,
                                      blockquotes, blockquoteCount, colorTags, colorTagCount,
                                      fontTags, fontTagCount, &textSizeMeasured);
                } else {
                    RenderTextMarkdown(memDC, &textRect, textToRender, &ctx, CLOCK_EDIT_MODE, pBits,
                                      NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0,
                                      &textSizeMeasured);
                }
            }

            // Fill clickable regions with minimal alpha for mouse hit-testing (non-edit mode only)
            if (!CLOCK_EDIT_MODE) {
                FillClickableRegionsAlpha(pixels, rect.right, rect.bottom);
            }

            // Render images below text (centered horizontally like text)
            if (images && imageCount > 0) {
                int imgY = textHeight > 0 ? AddRenderDimensionClamped(textHeight, 5) : 5;
                int maxW = rect.right - 10;
                if (maxW <= 0) maxW = rect.right;  // Fallback if window too narrow
                ImageRenderContext imageRenderCtx = {0};
                BOOL imageRenderCtxActive = FALSE;
                BOOL imageRenderCtxAttempted = FALSE;

                for (int i = 0; i < imageCount; i++) {
                    int maxH = rect.bottom - imgY - 5;
                    if (maxH <= 0) break;  // No more space for images

                    // Check if network image needs async download
                    if (images[i].isNetworkImage && !images[i].isDownloaded &&
                        !images[i].isDownloading && !IsMarkdownImageRetryPending(&images[i])) {
                        StartAsyncImageDownload(&images[i], hwnd);
                    }

                    // If downloading, show "Loading..." text
                    if (images[i].isDownloading || (images[i].isNetworkImage && !images[i].isDownloaded)) {
                        // Draw "Loading..." centered with same color as text
                        const wchar_t loadingText[] = L"Loading...";
                        SetBkMode(memDC, TRANSPARENT);
                        SetTextColor(memDC, ctx.textColor);
                        int loadingTextLen = (int)(_countof(loadingText) - 1);
                        SIZE loadingTextSize = {
                            70,
                            CalculateRenderFontSize(ctx.renderFontSize, ctx.fontScaleFactor)
                        };
                        if (loadingTextSize.cy <= 0) {
                            loadingTextSize.cy = 16;
                        }
                        GetTextExtentPoint32W(memDC, loadingText, loadingTextLen, &loadingTextSize);
                        int textX = (rect.right - loadingTextSize.cx) / 2;
                        TextOutW(memDC, textX, imgY, loadingText, loadingTextLen);
                        imgY = AddRenderDimensionClamped(imgY,
                                                         AddRenderDimensionClamped(loadingTextSize.cy, 5));
                        continue;
                    }

                    // Get render size for centering
                    int imgRenderW = 0, imgRenderH = 0;
                    if (!CalculateImageRenderSize(&images[i], maxW, maxH, &imgRenderW, &imgRenderH)) {
                        continue;  // Skip this image if calculation fails
                    }

                    // Center horizontally
                    int imgX = (rect.right - imgRenderW) / 2;
                    if (imgX < 5) imgX = 5;

                    int imgHeight = 0;
                    if (!imageRenderCtxAttempted) {
                        imageRenderCtxActive = BeginImageRenderContext(memDC, &imageRenderCtx);
                        imageRenderCtxAttempted = TRUE;
                    }
                    if (imageRenderCtxActive) {
                        imgHeight = RenderMarkdownImageSizedWithContext(&imageRenderCtx,
                                                                        &images[i], imgX, imgY,
                                                                        imgRenderW, imgRenderH);
                    }
                    if (imgHeight > 0) {
                        imgY = AddRenderDimensionClamped(imgY,
                                                         AddRenderDimensionClamped(imgHeight, 5));
                    }
                }

                if (imageRenderCtxActive) {
                    EndImageRenderContext(&imageRenderCtx);
                }
            }
        } else if (CLOCK_EDIT_MODE) {
            FixAlphaChannel(pBits, rect.right, rect.bottom);
        }
    }

    /* Check if any color tag has gradient (multiple colors) before freeing */
    BOOL hasColorTagGradient = FALSE;
    if (colorTags && colorTagCount > 0) {
        for (int i = 0; i < colorTagCount; i++) {
            if (colorTags[i].colorCount > 1) {
                hasColorTagGradient = TRUE;
                break;
            }
        }
    }

    // Free image resources
    if (!imagesOwnedByCache) {
        FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
    }

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        ReleaseRenderDibCache();
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return;
    }
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {rect.right, rect.bottom};
    POINT ptDst = {0, 0};

    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    ptDst.x = rcWindow.left;
    ptDst.y = rcWindow.top;

    BYTE alpha = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;

    BOOL layeredUpdateSucceeded = TRUE;
    if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
        DWORD err = GetLastError();
        layeredUpdateSucceeded = FALSE;
        if (err == ERROR_INVALID_PARAMETER) {
            // Error 87 often implies conflict between SetLayeredWindowAttributes and UpdateLayeredWindow
            // Reset WS_EX_LAYERED style to clear the internal state
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

            // Retry update
            if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
                err = GetLastError();
                if (ShouldLogMainWindowRenderFailure()) {
                    WriteLog(LOG_LEVEL_ERROR,
                             "UpdateLayeredWindow failed retry! Error code: %lu", err);
                }
            } else {
                layeredUpdateSucceeded = TRUE;
            }
        } else {
            if (ShouldLogMainWindowRenderFailure()) {
                WriteLog(LOG_LEVEL_ERROR,
                         "UpdateLayeredWindow failed! Error code: %lu", err);
            }
        }
        if (!layeredUpdateSucceeded) {
            StopDrawingRenderAnimationTimer(hwnd);
            RecordMainWindowRenderFailure(hwnd);
        }
    }

    ReleaseDC(NULL, hdcScreen);

    UNREFERENCED_PARAMETER(memBitmap);
    UNREFERENCED_PARAMETER(oldBitmap);

    if (layeredUpdateSucceeded) {
        g_renderDibCache.frameValid = TRUE;
        g_renderDibCache.frameWasScaleComposite = usedScaleComposite;
        g_renderDibCache.frameEditMode = CLOCK_EDIT_MODE;
        g_renderDibCache.frameHwnd = hwnd;
        g_renderDibCache.frameWidth = rect.right;
        g_renderDibCache.frameHeight = rect.bottom;
        ResetMainWindowRenderRetry(hwnd);
        Timer_NotifyMainWindowPainted(paintBuffers->timerTextSnapshot);
        RefreshClickThroughState(hwnd);
        UpdateDrawingRenderAnimationTimerForFrame(hwnd, hasContent, hasColorTagGradient);
    } else {
        g_renderDibCache.frameValid = FALSE;
    }
}
