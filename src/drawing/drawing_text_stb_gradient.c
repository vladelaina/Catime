/**
 * @file drawing_text_stb_gradient.c
 * @brief Gradient interpolation and animated lookup-table support.
 */

#include "drawing_text_stb_internal.h"

/* Pre-calculated gradient LUT to reduce CPU usage */
COLORREF g_gradientLUT[LUT_SIZE];
static GradientType g_lutType = GRADIENT_NONE;
static char g_lutName[GRADIENT_NAME_BUFFER];
static DWORD g_lutSignature = 0;

long long GradientPositionFixed(long long x, int startX, int totalWidth) {
    if (totalWidth <= 0) return 0;

    long long offset = x - (long long)startX;
    if (offset <= 0) return 0;
    if (offset >= (long long)totalWidth) return GRADIENT_FIXED_ONE;

    return (offset * GRADIENT_FIXED_ONE) / (long long)totalWidth;
}

int InterpolateGradientChannelFixed(int from, int to, long long position) {
    if (position <= 0) return from;
    if (position >= GRADIENT_FIXED_ONE) return to;

    long long weighted = (long long)from * (GRADIENT_FIXED_ONE - position) +
                         (long long)to * position;
    return (int)(weighted / GRADIENT_FIXED_ONE);
}

void AdvanceGradientPositionFixed(long long* position, long long step) {
    if (!position || step <= 0 || *position >= GRADIENT_FIXED_ONE) return;

    *position += step;
    if (*position > GRADIENT_FIXED_ONE) {
        *position = GRADIENT_FIXED_ONE;
    }
}
/* Context for gradient glow callback */
void InitGlowGradientContext(GlowGradientContext* ctx,
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
void GetGlowGradientColor(int x, int y,
                          int* r, int* g, int* b,
                          void* userData) {
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

BOOL GradientLUTMatches(const GradientInfo* info) {
    if (!info || g_lutType != info->type) return FALSE;
    if (g_lutSignature != ComputeGradientLUTSignature(info)) return FALSE;
    if (info->type == GRADIENT_CUSTOM) {
        return info->name && strcmp(g_lutName, info->name) == 0;
    }
    return TRUE;
}

void InitializeGradientLUT(const GradientInfo* info) {
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
