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
