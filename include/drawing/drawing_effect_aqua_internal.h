#ifndef DRAWING_EFFECT_AQUA_INTERNAL_H
#define DRAWING_EFFECT_AQUA_INTERNAL_H

#include <windows.h>

int AquaClampByte(int value);
int AquaClampInt(int value, int minValue, int maxValue);
int AquaLerpByte256(int a, int b, int t256);
int AquaFloorFixed8(int value);
int AquaFractalNoise(int screenX, int screenY, int freqXPpm, int freqYPpm,
                     int flowQ8, unsigned int seed);
int AquaNoiseAt(const unsigned char* noiseMap, int width, int height,
                int x, int y);
int AquaNoiseAtUnchecked(const unsigned char* noiseMap, int width, int x, int y);
int AquaSampleAlphaBilinear(const unsigned char* alphaMap, int width, int height,
                            int xQ8, int yQ8);
int AquaErodedAlpha(int alpha, int poreNoise, int displacementNoise);

#endif
