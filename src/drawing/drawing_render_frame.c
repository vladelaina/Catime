/**
 * @file drawing_render_frame.c
 * @brief Render text and images into the reusable DIB frame.
 */

#include "drawing_render_internal.h"

BOOL RenderDrawingPaintFrame(PaintFrameContext* frame) {
    if (!frame) return FALSE;

    HWND hwnd = frame->hwnd;
    HDC hdc = frame->hdc;
    RECT rect = frame->rect;
    DWORD activeScaleSerial = frame->activeScaleSerial;
    MarkdownImage* images = frame->images;
    int imageCount = frame->imageCount;
    BOOL imagesHeapAllocated = frame->imagesHeapAllocated;
    BOOL imagesOwnedByCache = frame->imagesOwnedByCache;
    RenderContext ctx = frame->renderContext;
    BOOL isMarkdown = frame->isMarkdown;
    const MarkdownHeading* headings = frame->headings;
    int headingCount = frame->headingCount;
    MarkdownColorTag* colorTags = frame->colorTags;
    int colorTagCount = frame->colorTagCount;
    const MarkdownFontTag* fontTags = frame->fontTags;
    int fontTagCount = frame->fontTagCount;
    const wchar_t* textToRender = frame->textToRender;
    BOOL hasText = frame->hasText;
    BOOL hasContent = frame->hasContent;
    SIZE measuredTextSize = frame->measuredTextSize;
    BOOL measuredTextSizeValid = frame->measuredTextSizeValid;

    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    void* pBits = NULL;

    // Create buffer with the final correct size
    if (!SetupDoubleBufferDIB(hdc, &rect, &memDC, &memBitmap, &oldBitmap, &pBits)) {
        DWORD error = GetLastError();
        if (ShouldLogMainWindowRenderFailure()) {
            WriteLog(LOG_LEVEL_ERROR,
                     "Render DIB setup failed (%dx%d, error=%lu)",
                     rect.right, rect.bottom, error);
        }
        if (!imagesOwnedByCache) {
            FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
        }
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return FALSE;
    }

    // Manually clear background
    // Edit Mode: Alpha=5 to capture mouse click on background
    // Normal Mode: Alpha=0 for full transparency (clickable regions filled later)
    size_t numPixels = 0;
    if (!pBits || !CalculatePixelCount(rect.right, rect.bottom, &numPixels)) {
        if (ShouldLogMainWindowRenderFailure()) {
            WriteLog(LOG_LEVEL_ERROR,
                     "Render DIB pixel validation failed (%dx%d, bits=%p)",
                     rect.right, rect.bottom, pBits);
        }
        ReleaseRenderDibCache();
        if (!imagesOwnedByCache) {
            FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
        }
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return FALSE;
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

    frame->memDC = memDC;
    frame->memBitmap = memBitmap;
    frame->oldBitmap = oldBitmap;
    frame->bits = pBits;
    frame->usedScaleComposite = usedScaleComposite;
    return TRUE;
}
