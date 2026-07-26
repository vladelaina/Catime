/**
 * @file drawing_render_prepare.c
 * @brief Build plugin, markdown, image, and measurement state for a frame.
 */

#include "drawing_render_internal.h"

BOOL PrepareDrawingPaintFrame(PaintFrameContext* frame, HWND hwnd,
                              const PAINTSTRUCT* ps) {
    if (!frame || !hwnd || !ps) return FALSE;

    frame->hwnd = hwnd;
    frame->hdc = ps->hdc;
    frame->paintBuffers = &g_paintTextBuffers;

    PaintTextBuffers* paintBuffers = frame->paintBuffers;
    wchar_t* timeText = paintBuffers->timeText;
    wchar_t* pluginText = paintBuffers->pluginText;
    wchar_t* result = paintBuffers->pluginResult;
    HDC hdc = frame->hdc;
    RECT rect = {0};
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
    MarkdownImage* stackImages = frame->stackImages;
    MarkdownImage* images = NULL;
    int imageCount = 0;
    int imageCapacity;
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

        ZeroMemory(stackImages, sizeof(frame->stackImages));
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

    frame->rect = rect;
    frame->activeScaleSerial = activeScaleSerial;
    frame->images = images;
    frame->imageCount = imageCount;
    frame->imagesHeapAllocated = imagesHeapAllocated;
    frame->imagesOwnedByCache = imagesOwnedByCache;
    frame->renderContext = ctx;
    frame->isMarkdown = isMarkdown;
    frame->headings = headings;
    frame->headingCount = headingCount;
    frame->colorTags = colorTags;
    frame->colorTagCount = colorTagCount;
    frame->fontTags = fontTags;
    frame->fontTagCount = fontTagCount;
    frame->textToRender = textToRender;
    frame->hasText = hasText;
    frame->hasContent = hasContent;
    frame->measuredTextSize = measuredTextSize;
    frame->measuredTextSizeValid = measuredTextSizeValid;
    return TRUE;
}
