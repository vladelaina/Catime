        int holoR = GetRValue(info->startColor);
        int holoG = GetGValue(info->startColor);
        int holoB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderHolographicEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                                holoR, holoG, holoB, GetGlowGradientColor, &ctx, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_LIQUID) {
        int liquidR = GetRValue(info->startColor);
        int liquidG = GetGValue(info->startColor);
        int liquidB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderLiquidEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                           liquidR, liquidG, liquidB, GetGlowGradientColor, &ctx, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_AQUA) {
        int aquaR = GetRValue(info->startColor);
        int aquaG = GetGValue(info->startColor);
        int aquaB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        ctx.timeOffset = 0;
        RenderAquaEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         aquaR, aquaG, aquaB, GetGlowGradientColor, &ctx, timeOffset);
        return;
    } else if (effect == EFFECT_TYPE_RETRO) {
        int shadowR = GetRValue(info->endColor);
        int shadowG = GetGValue(info->endColor);
        int shadowB = GetBValue(info->endColor);

        if (!info->isAnimated) {
            RenderRetroEffect(pixels, destWidth, destHeight, x_pos, y_pos,
                              bitmap, w, h,
                              r1, g1, b1,
                              shadowR, shadowG, shadowB,
                              NULL, NULL);
            return;
        }

        RenderRetroShadowEffect(pixels, destWidth, destHeight, x_pos, y_pos,
                                bitmap, w, h, shadowR, shadowG, shadowB);
    }

    TextBitmapClip clip;
    if (!ClipTextBitmapToDestination(x_pos, y_pos, w, h, destWidth, destHeight, &clip)) {
        return;
    }

    for (int j = clip.srcTop; j < clip.srcBottom; ++j) {
        int destY = clip.destTop + (j - clip.srcTop);
        size_t destIndex = (size_t)destY * (size_t)destWidth + (size_t)clip.destLeft;
        size_t srcIndex = (size_t)j * (size_t)w + (size_t)clip.srcLeft;
        if (destIndex >= destPixelCount || srcIndex >= bitmapPixelCount) continue;

        DWORD* destRow = pixels + destIndex;
        const unsigned char* srcRow = bitmap + srcIndex;

        /* Pre-calculate starting LUT index for this row if Animated */
        float currentLutIdxFloat = 0.0f;
        if (info->isAnimated) {
            long long rowStartX = (long long)clip.destLeft - (long long)startX;
            if (totalWidth > 0) {
                currentLutIdxFloat = ((float)rowStartX / (float)totalWidth) * LUT_SIZE;
            }
        }

        long long currentGradientFixed = 0;
        long long gradientFixedStep = 0;
        if (!info->isAnimated && totalWidth > 0) {
            currentGradientFixed = GradientPositionFixed(clip.destLeft, startX, totalWidth);
            gradientFixedStep = GRADIENT_FIXED_ONE / (long long)totalWidth;
        }

        for (int i = clip.srcLeft; i < clip.srcRight; ++i) {
            unsigned char alpha = *srcRow++;

            if (alpha == 0) {
                if (info->isAnimated) currentLutIdxFloat += lutStep;
                else AdvanceGradientPositionFixed(&currentGradientFixed, gradientFixedStep);
                destRow++;
                continue;
            }

            int r, g, b;

            if (info->isAnimated) {
                /* Optimized LUT Lookup */
                int lutIdx = (int)currentLutIdxFloat - timeOffset;
                currentLutIdxFloat += lutStep;

                /* Optimized wrap-around logic */
                lutIdx = lutIdx & (LUT_SIZE - 1);

                COLORREF c = g_gradientLUT[lutIdx];
                r = GetRValue(c);
                g = GetGValue(c);
                b = GetBValue(c);
            } else {
                r = InterpolateGradientChannelFixed(r1, r2, currentGradientFixed);
                g = InterpolateGradientChannelFixed(g1, g2, currentGradientFixed);
                b = InterpolateGradientChannelFixed(b1, b2, currentGradientFixed);
                AdvanceGradientPositionFixed(&currentGradientFixed, gradientFixedStep);
            }

            /* Blend */
            DWORD currentPixel = *destRow;
            DWORD currentA = (currentPixel >> 24) & 0xFF;

            if (alpha > currentA) {
                DWORD finalR = (r * alpha) / 255;
                DWORD finalG = (g * alpha) / 255;
                DWORD finalB = (b * alpha) / 255;
                DWORD finalA = (DWORD)alpha;

                *destRow = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
            }
            destRow++;
        }
    }
}

void GetCharMetricsSTB(wchar_t c, wchar_t nextC, float scale, float fallbackScale, GlyphMetrics* out) {
    if (!out) return;

    out->index = 0;
    out->isFallback = FALSE;
    out->advance = 0;
    out->kern = 0;

    if (c == L'\n' || c == L'\r') return;

    wchar_t cacheNextC = (nextC == L'\n' || nextC == L'\r') ? 0 : nextC;
    if (c == L'\t') {
        cacheNextC = 0;
    }

    DWORD cacheSlot = GetGlyphMetricsCacheSlot(c, cacheNextC);
    GlyphMetricsCacheEntry* cached = &g_glyphMetricsCache[cacheSlot];
    if (cached->valid && cached->c == c && cached->nextC == cacheNextC) {
        ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
        return;
    }

    if (c == L'\t') {
        // Tab = 4 spaces
        int spaceIdx = stbtt_FindGlyphIndex(&g_fontInfo, ' ');
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&g_fontInfo, spaceIdx, &adv, &lsb);
        cached->valid = TRUE;
        cached->c = c;
        cached->nextC = cacheNextC;
        cached->index = spaceIdx;
        cached->isFallback = FALSE;
        cached->advanceUnits = adv * 4;
        cached->kernUnits = 0;
        ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
        return;
    }

    out->index = stbtt_FindGlyphIndex(&g_fontInfo, (int)c);

    if (out->index == 0 && g_fallbackFontLoaded && c != L' ') {
        int fallbackIndex = stbtt_FindGlyphIndex(&g_fallbackFontInfo, (int)c);
        if (fallbackIndex != 0) {
            out->index = fallbackIndex;
            out->isFallback = TRUE;
        }
    }

    int adv = 0;
    int lsb = 0;
    int kern = 0;
    if (out->isFallback) {
        stbtt_GetGlyphHMetrics(&g_fallbackFontInfo, out->index, &adv, &lsb);
    } else {
        stbtt_GetGlyphHMetrics(&g_fontInfo, out->index, &adv, &lsb);

        // Kerning
        if (cacheNextC) {
            int nextIdx = stbtt_FindGlyphIndex(&g_fontInfo, (int)cacheNextC);
            if (nextIdx != 0) {
                kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, out->index, nextIdx);
            }
        }
    }

    cached->valid = TRUE;
    cached->c = c;
    cached->nextC = cacheNextC;
    cached->index = out->index;
    cached->isFallback = out->isFallback;
    cached->advanceUnits = adv;
    cached->kernUnits = kern;
    ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
}

BOOL GetCachedFontCharMetricsSTB(const stbtt_fontinfo* fontInfo,
                                 wchar_t c,
                                 float scale,
                                 GlyphMetrics* out) {
    if (!fontInfo || !out) return FALSE;

    out->index = 0;
    out->isFallback = FALSE;
    out->advance = 0;
    out->kern = 0;

    int fontSlot = -1;
    for (int i = 0; i < MAX_CACHED_FONTS; ++i) {
        if (g_fontCache[i].isLoaded && fontInfo == &g_fontCache[i].fontInfo) {
            fontSlot = i;
            break;
        }
    }
    if (fontSlot < 0) {
        return FALSE;
    }

    DWORD cacheSlot = GetFontTagGlyphMetricsCacheSlot(c);
    FontTagGlyphMetricsCacheEntry* cached =
        &g_fontTagGlyphMetricsCache[fontSlot][cacheSlot];
    if (cached->valid && cached->c == c) {
        out->index = cached->index;
        out->advance = ScaleTextMetricClamped(cached->advanceUnits, scale);
        return TRUE;
    }

    int glyphIndex = stbtt_FindGlyphIndex(fontInfo, (int)c);
    int advanceUnits = 0;
    if (glyphIndex != 0) {
        int lsb = 0;
        stbtt_GetGlyphHMetrics(fontInfo, glyphIndex, &advanceUnits, &lsb);
    }

    cached->valid = TRUE;
    cached->c = c;
    cached->index = glyphIndex;
    cached->advanceUnits = advanceUnits;

    out->index = glyphIndex;
    out->advance = ScaleTextMetricClamped(advanceUnits, scale);
    return TRUE;
}

BOOL IsGlyphBitmapVisibleSTB(const stbtt_fontinfo* fontInfo,
                             int glyphIndex,
                             float scaleX,
                             float scaleY,
                             int originX,
                             int originY,
                             int destWidth,
                             int destHeight,
                             EffectType effect,
