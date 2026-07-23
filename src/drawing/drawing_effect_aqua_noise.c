#include "drawing/drawing_effect_aqua_internal.h"

#define AQUA_NOISE_FIXED_ONE 256

int AquaClampByte(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }
int AquaClampInt(int value, int minValue, int maxValue) {
    return value < minValue ? minValue : value > maxValue ? maxValue : value;
}
int AquaLerpByte256(int a, int b, int t256) { return a + (((b - a) * t256) >> 8); }
static int SmoothStepByte(int t) { return (t * t * (768 - (t << 1))) >> 16; }
static unsigned int AquaHashNoise(int x, int y, unsigned int seed) {
    unsigned int h = (unsigned int)x * 374761393u;
    h += (unsigned int)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
int AquaFloorFixed8(int value) {
    return value >= 0 ? value >> 8 : -(((-value) + 255) >> 8);
}
static int AquaValueNoiseQ8(int xQ8, int yQ8, unsigned int seed) {
    int xi = AquaFloorFixed8(xQ8), yi = AquaFloorFixed8(yQ8);
    int sx = SmoothStepByte(xQ8 - (xi << 8));
    int sy = SmoothStepByte(yQ8 - (yi << 8));
    int n00 = AquaHashNoise(xi, yi, seed) & 0xFF;
    int n10 = AquaHashNoise(xi + 1, yi, seed) & 0xFF;
    int n01 = AquaHashNoise(xi, yi + 1, seed) & 0xFF;
    int n11 = AquaHashNoise(xi + 1, yi + 1, seed) & 0xFF;
    return AquaLerpByte256(AquaLerpByte256(n00, n10, sx),
                           AquaLerpByte256(n01, n11, sx), sy);
}
int AquaFractalNoise(int screenX, int screenY, int freqXPpm, int freqYPpm,
                     int flowQ8, unsigned int seed) {
    long long x = (long long)screenX * freqXPpm * AQUA_NOISE_FIXED_ONE / 1000000LL;
    long long y = (long long)screenY * freqYPpm * AQUA_NOISE_FIXED_ONE / 1000000LL;
    return AquaValueNoiseQ8((int)x, (int)(y - flowQ8), seed);
}
int AquaNoiseAt(const unsigned char* map, int width, int height, int x, int y) {
    x = AquaClampInt(x, 0, width - 1); y = AquaClampInt(y, 0, height - 1);
    return map[(size_t)y * width + (size_t)x];
}
int AquaNoiseAtUnchecked(const unsigned char* map, int width, int x, int y) {
    return map[(size_t)y * width + (size_t)x];
}
int AquaSampleAlphaBilinear(const unsigned char* map, int width, int height,
                            int xQ8, int yQ8) {
    int x = AquaFloorFixed8(xQ8), y = AquaFloorFixed8(yQ8);
    if (x < 0 || y < 0 || x >= width - 1 || y >= height - 1) return 0;
    int tx = xQ8 - (x << 8), ty = yQ8 - (y << 8);
    const unsigned char* row0 = map + (size_t)y * width;
    const unsigned char* row1 = row0 + width;
    int top = AquaLerpByte256(row0[x], row0[x + 1], tx);
    int bottom = AquaLerpByte256(row1[x], row1[x + 1], tx);
    return AquaLerpByte256(top, bottom, ty);
}
int AquaErodedAlpha(int alpha, int poreNoise, int displacementNoise) {
    if (alpha <= 0) return 0;
    int edge = alpha < 250 ? 250 - alpha : 0;
    int pore = AquaClampByte(poreNoise - 92);
    int displacement = AquaClampByte(displacementNoise - 104);
    int bite = (pore * (18 + edge) + displacement * (edge >> 1)) >> 8;
    return AquaClampByte(alpha - (alpha < 255 ? (bite * alpha) >> 8 : bite));
}
