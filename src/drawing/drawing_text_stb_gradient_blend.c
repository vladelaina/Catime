/**
 * @file drawing_text_stb_gradient_blend.c
 * @brief Gradient glyph compositing.
 */

#include "drawing_text_stb_internal.h"

void BlendCharBitmapGradientSTB(void* destBits, int destWidth, int destHeight,
                                int x_pos, int y_pos,
                                const unsigned char* bitmap, int w, int h,
                                int startX, int totalWidth, int gradientType,
                                int timeOffset) {
    BlendCharBitmapGradientSTBWithEffect(destBits, destWidth, destHeight,
                                         x_pos, y_pos, bitmap, w, h,
                                         startX, totalWidth, gradientType,
                                         timeOffset, GetActiveEffect());
}

void BlendCharBitmapGradientSTBWithEffect(void* destBits, int destWidth, int destHeight,
                                          int x_pos, int y_pos,
                                          const unsigned char* bitmap, int w, int h,
                                          int startX, int totalWidth, int gradientType,
                                          int timeOffset, EffectType effect) {
    GradientInfoSnapshot snapshot;
    if (!GetGradientInfoSnapshot((GradientType)gradientType, &snapshot)) return;

    BlendCharBitmapGradientSTBWithInfo(destBits, destWidth, destHeight,
                                       x_pos, y_pos, bitmap, w, h,
                                       startX, totalWidth, &snapshot.info,
                                       timeOffset, effect);
}

void BlendCharBitmapGradientSTBWithInfo(void* destBits, int destWidth, int destHeight,
                                        int x_pos, int y_pos,
                                        const unsigned char* bitmap, int w, int h,
                                        int startX, int totalWidth,
                                        const GradientInfo* gradientInfo,
                                        int timeOffset, EffectType effect) {
    const GradientInfo* info = gradientInfo;
    DWORD* pixels = (DWORD*)destBits;
    size_t destPixelCount = 0;
    size_t bitmapPixelCount = 0;

    if (!pixels || !bitmap ||
        !CalculateBitmapPixelCount(destWidth, destHeight, &destPixelCount) ||
        !CalculateBitmapPixelCount(w, h, &bitmapPixelCount)) {
        return;
    }

    if (!info) return;

    int r1 = 0, g1 = 0, b1 = 0;
    int r2 = 0, g2 = 0, b2 = 0;

    /* Animation parameters */
    float lutStep = 0.0f;

    if (info->isAnimated) {
        BOOL needLutUpdate = !GradientLUTMatches(info);
        if (needLutUpdate) InitializeGradientLUT(info);

        if (totalWidth > 0) {
            lutStep = (float)LUT_SIZE / (float)totalWidth;
        }
    } else {
        r1 = GetRValue(info->startColor);
        g1 = GetGValue(info->startColor);
        b1 = GetBValue(info->startColor);

        r2 = GetRValue(info->endColor);
        g2 = GetGValue(info->endColor);
        b2 = GetBValue(info->endColor);
    }

    /* Render glow effect if enabled - use gradient start color as base but use callback for per-pixel color */
    if (effect == EFFECT_TYPE_GLOW) {
        int glowR = GetRValue(info->startColor);
        int glowG = GetGValue(info->startColor);
        int glowB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderGlowEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         glowR, glowG, glowB, GetGlowGradientColor, &ctx);
    } else if (effect == EFFECT_TYPE_GLASS) {
        int glassR = GetRValue(info->startColor);
        int glassG = GetGValue(info->startColor);
        int glassB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderGlassEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         glassR, glassG, glassB, GetGlowGradientColor, &ctx);
        /*
           CRITICAL: Return early to prevent solid gradient overwriting the glass effect.
        */
        return;
    } else if (effect == EFFECT_TYPE_NEON) {
        int neonR = GetRValue(info->startColor);
        int neonG = GetGValue(info->startColor);
        int neonB = GetBValue(info->startColor);

        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderNeonEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         neonR, neonG, neonB, GetGlowGradientColor, &ctx);
        /* Neon replaces solid text */
        return;
    } else if (effect == EFFECT_TYPE_HOLOGRAPHIC) {
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
