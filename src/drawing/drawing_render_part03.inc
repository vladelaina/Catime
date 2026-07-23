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
