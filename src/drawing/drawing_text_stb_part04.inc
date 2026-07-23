/* Context for gradient glow callback */
typedef struct {
    const GradientInfo* info;
    int startX;
    int totalWidth;
    int timeOffset;
    BOOL isAnimated;
    int startR;
    int startG;
    int startB;
    int endR;
    int endG;
    int endB;
} GlowGradientContext;

static void InitGlowGradientContext(GlowGradientContext* ctx,
                                    const GradientInfo* info,
                                    int startX,
                                    int totalWidth,
                                    int timeOffset) {
    if (!ctx) return;

    ZeroMemory(ctx, sizeof(*ctx));
    ctx->info = info;
    ctx->startX = startX;
    ctx->totalWidth = totalWidth;
    ctx->timeOffset = timeOffset;
    ctx->isAnimated = info ? info->isAnimated : FALSE;

    if (info) {
        ctx->startR = GetRValue(info->startColor);
        ctx->startG = GetGValue(info->startColor);
        ctx->startB = GetBValue(info->startColor);
        ctx->endR = GetRValue(info->endColor);
        ctx->endG = GetGValue(info->endColor);
        ctx->endB = GetBValue(info->endColor);
    }
}

/**
 * @brief Callback to calculate gradient color for glow effect
 */
static void GetGlowGradientColor(int x, int y, int* r, int* g, int* b, void* userData) {
    const GlowGradientContext* ctx = (const GlowGradientContext*)userData;
    (void)y;

    if (!ctx || !ctx->info || !r || !g || !b) return;

    if (ctx->isAnimated) {
        long long lutPosition = 0;
        if (ctx->totalWidth > 0) {
            lutPosition = ((long long)(x - ctx->startX) * (long long)LUT_SIZE) /
                          (long long)ctx->totalWidth;
        }

        int lutIdx = (int)lutPosition - ctx->timeOffset;

        /* Wrap around */
        lutIdx = lutIdx & (LUT_SIZE - 1);

        COLORREF c = g_gradientLUT[lutIdx];
        *r = GetRValue(c);
        *g = GetGValue(c);
        *b = GetBValue(c);
    } else {
        long long position = GradientPositionFixed(x, ctx->startX, ctx->totalWidth);
        *r = InterpolateGradientChannelFixed(ctx->startR, ctx->endR, position);
        *g = InterpolateGradientChannelFixed(ctx->startG, ctx->endG, position);
        *b = InterpolateGradientChannelFixed(ctx->startB, ctx->endB, position);
    }
}

static DWORD ComputeGradientLUTSignature(const GradientInfo* info) {
    if (!info) return 0;

    DWORD signature = 2166136261u;
    signature = (signature ^ (DWORD)info->startColor) * 16777619u;
    signature = (signature ^ (DWORD)info->endColor) * 16777619u;
    signature = (signature ^ (DWORD)info->paletteCount) * 16777619u;

    if (info->palette && info->paletteCount > 0) {
        for (int i = 0; i < info->paletteCount; i++) {
            signature = (signature ^ (DWORD)info->palette[i]) * 16777619u;
        }
    }

    return signature ? signature : 1u;
}

static void SetGradientLUTKey(const GradientInfo* info) {
    g_lutType = info ? info->type : GRADIENT_NONE;
    g_lutSignature = info ? ComputeGradientLUTSignature(info) : 0;
    g_lutName[0] = '\0';
    if (info && info->type == GRADIENT_CUSTOM && info->name) {
        strncpy(g_lutName, info->name, sizeof(g_lutName) - 1);
        g_lutName[sizeof(g_lutName) - 1] = '\0';
    }
}

static BOOL GradientLUTMatches(const GradientInfo* info) {
    if (!info || g_lutType != info->type) return FALSE;
    if (g_lutSignature != ComputeGradientLUTSignature(info)) return FALSE;
    if (info->type == GRADIENT_CUSTOM) {
        return info->name && strcmp(g_lutName, info->name) == 0;
    }
    return TRUE;
}

static void InitializeGradientLUT(const GradientInfo* info) {
    if (!info) return;

    /* Fallback logic if palette is invalid but isAnimated is true */
    if (!info->palette || info->paletteCount < 2) {
        int r1 = GetRValue(info->startColor);
        int g1 = GetGValue(info->startColor);
        int b1 = GetBValue(info->startColor);

        int r2 = GetRValue(info->endColor);
        int g2 = GetGValue(info->endColor);
        int b2 = GetBValue(info->endColor);

        for (int i = 0; i < LUT_SIZE; i++) {
            float t = (float)i / (float)(LUT_SIZE - 1);
            int r = (int)(r1 + (r2 - r1) * t);
            int g = (int)(g1 + (g2 - g1) * t);
            int b = (int)(b1 + (b2 - b1) * t);
            g_gradientLUT[i] = RGB(r, g, b);
        }
        SetGradientLUTKey(info);
        return;
    }

    const int colorCount = info->paletteCount;
    const COLORREF* colors = info->palette;

    for (int i = 0; i < LUT_SIZE; i++) {
        float t = (float)i / (float)(LUT_SIZE - 1);

        /* Standard multi-stop gradient logic */
        float scaledT = t * (colorCount - 1);
        int idx = (int)scaledT;
        int nextIdx = idx + 1;
        if (nextIdx >= colorCount) nextIdx = colorCount - 1;

        float frac = scaledT - idx;

        COLORREF c1 = colors[idx];
        COLORREF c2 = colors[nextIdx];

        int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * frac);
        int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * frac);
        int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * frac);

        g_gradientLUT[i] = RGB(r, g, b);
    }
    SetGradientLUTKey(info);
}

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
