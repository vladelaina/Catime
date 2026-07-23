    ReleaseMappedFont(g_fallbackFontBuffer, g_hFallbackFontFile, g_hFallbackFontMapping);
    g_fallbackFontBuffer = NULL;
    g_hFallbackFontFile = INVALID_HANDLE_VALUE;
    g_hFallbackFontMapping = NULL;

    /* Cleanup font cache */
    ClearFontCacheSTBLocked();
    ClearGlyphMetricsCacheLocked();
    ClearGlyphBitmapCacheLocked();

    g_fontLoaded = FALSE;
    g_fallbackFontLoaded = FALSE;
    memset(g_currentFontPath, 0, sizeof(g_currentFontPath));
    ZeroMemory(&g_currentFontLastWriteTime, sizeof(g_currentFontLastWriteTime));
    g_currentFontFileSize = 0;
    g_currentFontLastValidateTick = 0;
    g_currentFontFileInfoValid = FALSE;

    /* Also cleanup effect buffers */
    CleanupDrawingEffects();

    if (hadMainFontState) {
        AdvanceFontStateGeneration();
    }
}

BOOL InitFontSTB(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;

    if (!BeginFontUseSTB()) return FALSE;

    if (IsCurrentMainFontFileStillCurrentLocked(fontFilePath)) {
        EndFontUseSTB();
        return TRUE;
    }

    HANDLE hNewFile = INVALID_HANDLE_VALUE;
    HANDLE hNewMapping = NULL;

    // Load to temp buffer (view) first
    unsigned char* newBuffer = LoadFontMapping(fontFilePath, &hNewFile, &hNewMapping);
    if (!newBuffer) {
        EndFontUseSTB();
        return FALSE;
    }

    FILETIME newLastWriteTime;
    ULONGLONG newFileSize = 0;
    if (!GetFontFileInfoFromHandle(hNewFile, &newLastWriteTime, &newFileSize)) {
        ReleaseMappedFont(newBuffer, hNewFile, hNewMapping);
        EndFontUseSTB();
        return FALSE;
    }

    stbtt_fontinfo newInfo;
    if (!InitFontInfoFromBuffer(&newInfo, newBuffer, fontFilePath)) {
        ReleaseMappedFont(newBuffer, hNewFile, hNewMapping);
        EndFontUseSTB();
        return FALSE;
    }

    // Success - now replace the global state
    CleanupFontSTBLocked();

    g_fontBuffer = newBuffer;
    g_fontInfo = newInfo;
    g_hFontFile = hNewFile;
    g_hFontMapping = hNewMapping;
    strncpy(g_currentFontPath, fontFilePath, MAX_PATH - 1);
    g_currentFontPath[MAX_PATH - 1] = '\0';
    g_currentFontLastWriteTime = newLastWriteTime;
    g_currentFontFileSize = newFileSize;
    g_currentFontLastValidateTick = GetTickCount();
    g_currentFontFileInfoValid = TRUE;
    g_fontLoaded = TRUE;
    AdvanceFontStateGeneration();

    LOG_INFO("STB Font loaded successfully: %s", fontFilePath);

    /* Load Fallback Font */
    /* Priority:
       1. Microsoft YaHei (msyh.ttc) - Best coverage for CJK, Blocks & BW Emojis
       2. Microsoft YaHei (msyh.ttf) - Legacy
       3. Segoe UI Symbol (seguisym.ttf) - Good for blocks
       4. Segoe UI Emoji (seguiemj.ttf) - Last resort (might render blank in STB)
    */
    const char* fallbackPath = "C:\\Windows\\Fonts\\msyh.ttc";
    HANDLE hFallbackFile = INVALID_HANDLE_VALUE;
    HANDLE hFallbackMapping = NULL;
    unsigned char* fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);

    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\msyh.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguisym.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguiemj.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (fallbackBuffer) {
        if (InitFontInfoFromBuffer(&g_fallbackFontInfo, fallbackBuffer, fallbackPath)) {
            g_fallbackFontBuffer = fallbackBuffer;
            g_hFallbackFontFile = hFallbackFile;
            g_hFallbackFontMapping = hFallbackMapping;
            g_fallbackFontLoaded = TRUE;
            LOG_INFO("STB Fallback Font loaded: %s", fallbackPath);
        } else {
            ReleaseMappedFont(fallbackBuffer, hFallbackFile, hFallbackMapping);
        }
    } else {
        LOG_WARNING("Failed to load fallback font (Emoji/Symbol)");
    }

    EndFontUseSTB();
    return TRUE;
}

/**
 * @brief Blend a single character bitmap into the destination buffer
 */
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

/* Pre-calculated gradient LUT to reduce CPU usage */
#define LUT_SIZE GRADIENT_LUT_SIZE
#define GRADIENT_FIXED_ONE (1LL << 32)
static COLORREF g_gradientLUT[LUT_SIZE];
static GradientType g_lutType = GRADIENT_NONE;
static char g_lutName[GRADIENT_NAME_BUFFER];
static DWORD g_lutSignature = 0;

static long long GradientPositionFixed(long long x, int startX, int totalWidth) {
    if (totalWidth <= 0) return 0;

    long long offset = x - (long long)startX;
    if (offset <= 0) return 0;
    if (offset >= (long long)totalWidth) return GRADIENT_FIXED_ONE;

    return (offset * GRADIENT_FIXED_ONE) / (long long)totalWidth;
}

static int InterpolateGradientChannelFixed(int from, int to, long long position) {
    if (position <= 0) return from;
    if (position >= GRADIENT_FIXED_ONE) return to;

    long long weighted = (long long)from * (GRADIENT_FIXED_ONE - position) +
                         (long long)to * position;
    return (int)(weighted / GRADIENT_FIXED_ONE);
}

static void AdvanceGradientPositionFixed(long long* position, long long step) {
    if (!position || step <= 0 || *position >= GRADIENT_FIXED_ONE) return;

    *position += step;
    if (*position > GRADIENT_FIXED_ONE) {
        *position = GRADIENT_FIXED_ONE;
    }
}
