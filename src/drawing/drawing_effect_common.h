#ifndef DRAWING_EFFECT_COMMON_H
#define DRAWING_EFFECT_COMMON_H

#include <windows.h>

typedef struct {
    unsigned char* buffer1;
    unsigned char* buffer2;
    unsigned char* buffer3;
} DrawingEffectBuffers;

BOOL DrawingEffect_BeginBufferUse(void);
void DrawingEffect_EndBufferUse(void);

BOOL DrawingEffect_CalculateBufferSize(int w, int h, int padding,
                                       int* outGw, int* outGh,
                                       int* outNeededSize);

BOOL DrawingEffect_CalculateVisibleSpan(long long start, int length, int limit,
                                        int* outFirst, int* outLast);

BOOL DrawingEffect_EnsureBuffers(int neededSize, DrawingEffectBuffers* outBuffers);

#endif /* DRAWING_EFFECT_COMMON_H */
