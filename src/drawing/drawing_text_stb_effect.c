/**
 * @file drawing_text_stb_effect.c
 * @brief Solid glyph compositing and text effects.
 */

#include "drawing_text_stb_internal.h"

static void GetContrastShadowColor(int r, int g, int b,
                                   int* shadowR, int* shadowG, int* shadowB) {
    int brightness = (r * 299 + g * 587 + b * 114) / 1000;
    int shadow = (brightness < 120) ? 255 : 0;

    if (shadowR) *shadowR = shadow;
    if (shadowG) *shadowG = shadow;
    if (shadowB) *shadowB = shadow;
}

void BlendCharBitmapSTB(void* destBits, int destWidth, int destHeight,
                          int x_pos, int y_pos,
                          const unsigned char* bitmap, int w, int h,
                          int r, int g, int b) {
    BlendCharBitmapSTBWithEffect(destBits, destWidth, destHeight,
                                 x_pos, y_pos, bitmap, w, h, r, g, b,
                                 GetActiveEffect(), (int)GetTickCount());
}

void BlendCharBitmapSTBWithEffect(void* destBits, int destWidth, int destHeight,
                                  int x_pos, int y_pos,
                                  const unsigned char* bitmap, int w, int h,
                                  int r, int g, int b,
                                  EffectType effect, int timeOffset) {
    DWORD* pixels = (DWORD*)destBits;
    size_t pixelCount = 0;

    if (!pixels || !bitmap ||
        !CalculateBitmapPixelCount(destWidth, destHeight, &pixelCount) ||
        !CalculateBitmapPixelCount(w, h, &pixelCount)) {
        return;
    }

    /* Render glow or glass effect if enabled */
    if (effect == EFFECT_TYPE_GLOW) {
        RenderGlowEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
    } else if (effect == EFFECT_TYPE_GLASS) {
        RenderGlassEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_NEON) {
        RenderNeonEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
        /* Critical: Return early (Tube replaces solid text) */
        return;
    } else if (effect == EFFECT_TYPE_HOLOGRAPHIC) {
        RenderHolographicEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_LIQUID) {
        RenderLiquidEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_AQUA) {
        RenderAquaEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL, timeOffset);
        return;
    } else if (effect == EFFECT_TYPE_RETRO) {
        int shadowR = 0, shadowG = 0, shadowB = 0;
        GetContrastShadowColor(r, g, b, &shadowR, &shadowG, &shadowB);
        RenderRetroEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                          r, g, b, shadowR, shadowG, shadowB, NULL, NULL);
        return;
    }

    TextBitmapClip clip;
    if (!ClipTextBitmapToDestination(x_pos, y_pos, w, h, destWidth, destHeight, &clip)) {
        return;
    }

    for (int j = clip.srcTop; j < clip.srcBottom; ++j) {
        int destY = clip.destTop + (j - clip.srcTop);
        DWORD* destRow = pixels + (size_t)destY * (size_t)destWidth + (size_t)clip.destLeft;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)clip.srcLeft;

        for (int i = clip.srcLeft; i < clip.srcRight; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                destRow++;
                continue;
            }

            DWORD currentPixel = *destRow;
            DWORD currentA = (currentPixel >> 24) & 0xFF;

            /* If new pixel is more opaque, overwrite */
            if (alpha > currentA) {
                /* Calculate premultiplied color values for UpdateLayeredWindow */
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
