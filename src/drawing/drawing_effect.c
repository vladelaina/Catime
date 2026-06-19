#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <windows.h>
#include "drawing/drawing_effect.h"
#include "drawing/drawing_effect_common.h"

#define EFFECT_BUFFER_SHRINK_RATIO 4
#define EFFECT_BUFFER_SHRINK_DELAY_MS 5000ULL
#define EFFECT_BUFFER_SHRINK_MIN_SIZE (256 * 1024)
#define EFFECT_BUFFER_MAX_PIXELS (4096 * 4096)

static unsigned char* g_effectBuffer1 = NULL;
static unsigned char* g_effectBuffer2 = NULL;
static unsigned char* g_effectBuffer3 = NULL;
static int g_effectBufferSize = 0;
static ULONGLONG g_effectShrinkCandidateTick = 0;
static INIT_ONCE g_effectBufferLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_effectBufferCS;

static BOOL CALLBACK InitEffectBufferLock(PINIT_ONCE initOnce, PVOID parameter, PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_effectBufferCS);
    return TRUE;
}

BOOL DrawingEffect_BeginBufferUse(void) {
    if (!InitOnceExecuteOnce(&g_effectBufferLockOnce, InitEffectBufferLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_effectBufferCS);
    return TRUE;
}

void DrawingEffect_EndBufferUse(void) {
    LeaveCriticalSection(&g_effectBufferCS);
}

BOOL DrawingEffect_CalculateBufferSize(int w, int h, int padding,
                                       int* outGw, int* outGh,
                                       int* outNeededSize) {
    int padTotal;
    int gw;
    int gh;
    int neededSize;

    if (!outGw || !outGh || !outNeededSize) return FALSE;
    if (w <= 0 || h <= 0 || padding < 0) return FALSE;
    if (padding > INT_MAX / 2) return FALSE;

    padTotal = padding * 2;
    if (w > INT_MAX - padTotal || h > INT_MAX - padTotal) return FALSE;

    gw = w + padTotal;
    gh = h + padTotal;
    if (gw <= 0 || gh <= 0 || gw > INT_MAX / gh) return FALSE;
    if (gw > EFFECT_BUFFER_MAX_PIXELS / gh) return FALSE;

    neededSize = gw * gh;

    *outGw = gw;
    *outGh = gh;
    *outNeededSize = neededSize;
    return TRUE;
}

static void FreeEffectBuffers(void) {
    if (g_effectBuffer1) { free(g_effectBuffer1); g_effectBuffer1 = NULL; }
    if (g_effectBuffer2) { free(g_effectBuffer2); g_effectBuffer2 = NULL; }
    if (g_effectBuffer3) { free(g_effectBuffer3); g_effectBuffer3 = NULL; }
    g_effectBufferSize = 0;
    g_effectShrinkCandidateTick = 0;
}

static void ResetEffectBufferShrinkCandidate(void) {
    g_effectShrinkCandidateTick = 0;
}

static void MaybeShrinkOversizedEffectBuffers(int neededSize) {
    if (g_effectBufferSize < EFFECT_BUFFER_SHRINK_MIN_SIZE ||
        neededSize > g_effectBufferSize / EFFECT_BUFFER_SHRINK_RATIO ||
        !g_effectBuffer1 || !g_effectBuffer2 || !g_effectBuffer3) {
        ResetEffectBufferShrinkCandidate();
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (g_effectShrinkCandidateTick == 0) {
        g_effectShrinkCandidateTick = now;
        return;
    }

    if (now - g_effectShrinkCandidateTick >= EFFECT_BUFFER_SHRINK_DELAY_MS) {
        FreeEffectBuffers();
    }
}

static BOOL EnsureEffectBuffers(int neededSize) {
    if (neededSize <= 0) return FALSE;
    if (neededSize <= g_effectBufferSize &&
        g_effectBuffer1 && g_effectBuffer2 && g_effectBuffer3) {
        MaybeShrinkOversizedEffectBuffers(neededSize);
        if (neededSize <= g_effectBufferSize &&
            g_effectBuffer1 && g_effectBuffer2 && g_effectBuffer3) {
            return TRUE;
        }
    } else {
        ResetEffectBufferShrinkCandidate();
    }

    unsigned char* newBuffer1 = (unsigned char*)malloc((size_t)neededSize);
    unsigned char* newBuffer2 = (unsigned char*)malloc((size_t)neededSize);
    unsigned char* newBuffer3 = (unsigned char*)malloc((size_t)neededSize);

    if (!newBuffer1 || !newBuffer2 || !newBuffer3) {
        free(newBuffer1);
        free(newBuffer2);
        free(newBuffer3);
        return FALSE;
    }

    FreeEffectBuffers();
    g_effectBuffer1 = newBuffer1;
    g_effectBuffer2 = newBuffer2;
    g_effectBuffer3 = newBuffer3;
    g_effectBufferSize = neededSize;
    return TRUE;
}

BOOL DrawingEffect_EnsureBuffers(int neededSize, DrawingEffectBuffers* outBuffers) {
    if (!outBuffers) return FALSE;
    ZeroMemory(outBuffers, sizeof(*outBuffers));

    if (!EnsureEffectBuffers(neededSize)) {
        return FALSE;
    }

    outBuffers->buffer1 = g_effectBuffer1;
    outBuffers->buffer2 = g_effectBuffer2;
    outBuffers->buffer3 = g_effectBuffer3;
    return TRUE;
}

static BOOL CalculateBlurPixelCount(int w, int h, size_t* outPixelCount) {
    if (!outPixelCount || w <= 0 || h <= 0) return FALSE;

    size_t sw = (size_t)w;
    size_t sh = (size_t)h;
    if (sw > (size_t)-1 / sh) return FALSE;

    *outPixelCount = sw * sh;
    return TRUE;
}

BOOL DrawingEffect_CalculateVisibleSpan(long long start, int length, int limit,
                                        int* outFirst, int* outLast) {
    if (!outFirst || !outLast || length <= 0 || limit <= 0) return FALSE;

    long long spanStart = start;
    long long spanEnd = spanStart + (long long)length;
    if (spanEnd <= 0 || spanStart >= (long long)limit) return FALSE;

    long long first = (spanStart < 0) ? -spanStart : 0;
    long long last = (spanEnd > (long long)limit)
        ? ((long long)limit - spanStart)
        : (long long)length;

    if (first < 0 || last < first || first > (long long)INT_MAX || last > (long long)INT_MAX) {
        return FALSE;
    }

    *outFirst = (int)first;
    *outLast = (int)last;
    return first < last;
}

void ApplyGaussianBlur(unsigned char* src, unsigned char* dest, unsigned char* tempBuffer,
                       int w, int h, int radius) {
    size_t pixelCount = 0;
    if (!src || !dest || !tempBuffer || !CalculateBlurPixelCount(w, h, &pixelCount)) return;

    if (radius < 1) {
        memcpy(dest, src, pixelCount);
        return;
    }

    int div = radius * 2 + 1;
    int reciprocal = (1 << 20) / div;

    for (int y = 0; y < h; y++) {
        int rowOffset = y * w;
        const unsigned char* rowSrc = src + rowOffset;
        unsigned char* rowDest = tempBuffer + rowOffset;

        int sum = 0;

        for (int k = -radius; k <= radius; k++) {
            int idx = k;
            if (idx < 0) idx = 0;
            if (idx >= w) idx = w - 1;
            sum += rowSrc[idx];
        }

        for (int x = 0; x < w; x++) {
            rowDest[x] = (unsigned char)((sum * reciprocal) >> 20);

            int outIdx = x - radius;
            int inIdx = x + radius + 1;

            int outVal;
            int inVal;

            if (outIdx < 0) outVal = rowSrc[0];
            else if (outIdx >= w) outVal = rowSrc[w - 1];
            else outVal = rowSrc[outIdx];

            if (inIdx < 0) inVal = rowSrc[0];
            else if (inIdx >= w) inVal = rowSrc[w - 1];
            else inVal = rowSrc[inIdx];

            sum -= outVal;
            sum += inVal;
        }
    }

    for (int x = 0; x < w; x++) {
        int sum = 0;

        for (int k = -radius; k <= radius; k++) {
            int idx = k;
            if (idx < 0) idx = 0;
            if (idx >= h) idx = h - 1;
            sum += tempBuffer[idx * w + x];
        }

        for (int y = 0; y < h; y++) {
            dest[y * w + x] = (unsigned char)((sum * reciprocal) >> 20);

            int outIdx = y - radius;
            int inIdx = y + radius + 1;

            int outVal;
            int inVal;

            if (outIdx < 0) outVal = tempBuffer[x];
            else if (outIdx >= h) outVal = tempBuffer[(h - 1) * w + x];
            else outVal = tempBuffer[outIdx * w + x];

            if (inIdx >= h) inVal = tempBuffer[(h - 1) * w + x];
            else inVal = tempBuffer[inIdx * w + x];

            sum -= outVal;
            sum += inVal;
        }
    }
}

void CleanupDrawingEffects(void) {
    if (!DrawingEffect_BeginBufferUse()) return;
    FreeEffectBuffers();
    DrawingEffect_EndBufferUse();
}
