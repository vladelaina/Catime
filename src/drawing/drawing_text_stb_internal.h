/**
 * @file drawing_text_stb_internal.h
 * @brief Internal interface shared by the split STB text modules.
 */

#ifndef DRAWING_TEXT_STB_INTERNAL_H
#define DRAWING_TEXT_STB_INTERNAL_H

#include "drawing/drawing_text_stb_types.h"
#include "drawing/drawing_effect.h"
#include "menu_preview.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned char* g_fontBuffer;
extern stbtt_fontinfo g_fontInfo;
extern char g_currentFontPath[MAX_PATH];
extern FILETIME g_currentFontLastWriteTime;
extern ULONGLONG g_currentFontFileSize;
extern DWORD g_currentFontLastValidateTick;
extern BOOL g_currentFontFileInfoValid;
extern BOOL g_fontLoaded;

extern unsigned char* g_fallbackFontBuffer;
extern stbtt_fontinfo g_fallbackFontInfo;
extern BOOL g_fallbackFontLoaded;

extern HANDLE g_hFontFile;
extern HANDLE g_hFontMapping;
extern HANDLE g_hFallbackFontFile;
extern HANDLE g_hFallbackFontMapping;
extern volatile LONG g_fontStateGeneration;

extern CachedFont g_fontCache[MAX_CACHED_FONTS];
extern int g_fontCacheLRU[MAX_CACHED_FONTS];
extern int g_fontCacheAccessCounter;
extern FontTagGlyphMetricsCacheEntry
    g_fontTagGlyphMetricsCache[MAX_CACHED_FONTS][FONT_TAG_GLYPH_METRICS_CACHE_SIZE];
extern FailedFontCacheEntry g_failedFontCache[MAX_FAILED_FONT_CACHE];
extern GlyphMetricsCacheEntry g_glyphMetricsCache[GLYPH_METRICS_CACHE_SIZE];
extern GlyphBitmapCacheEntry g_glyphBitmapCache[GLYPH_BITMAP_CACHE_SIZE];
extern DWORD g_glyphBitmapCacheUseCounter;
extern INIT_ONCE g_fontStateLockOnce;
extern CRITICAL_SECTION g_fontStateCS;
extern COLORREF g_gradientLUT[LUT_SIZE];

void AdvanceFontStateGeneration(void);
void ClearGlyphMetricsCacheLocked(void);
void ClearGlyphBitmapCacheLocked(void);

DWORD GetGlyphMetricsCacheSlot(wchar_t c, wchar_t nextC);
DWORD FloatBitsForGlyphCache(float value);
DWORD PointerBitsForGlyphCache(const void* ptr);
DWORD GetGlyphBitmapCacheSlot(const stbtt_fontinfo* fontInfo,
                              int glyphIndex,
                              DWORD scaleXBits,
                              DWORD scaleYBits,
                              int width,
                              int height,
                              int xoff,
                              int yoff);
BOOL GlyphBitmapCacheEntryMatches(const GlyphBitmapCacheEntry* entry,
                                  const stbtt_fontinfo* fontInfo,
                                  DWORD generation,
                                  int glyphIndex,
                                  DWORD scaleXBits,
                                  DWORD scaleYBits,
                                  int width,
                                  int height,
                                  int xoff,
                                  int yoff,
                                  size_t pixelCount);
unsigned char* CopyCachedGlyphBitmapLocked(const stbtt_fontinfo* fontInfo,
                                           DWORD generation,
                                           int glyphIndex,
                                           DWORD scaleXBits,
                                           DWORD scaleYBits,
                                           int width,
                                           int height,
                                           int xoff,
                                           int yoff,
                                           size_t pixelCount);
void StoreGlyphBitmapCacheLocked(const stbtt_fontinfo* fontInfo,
                                 DWORD generation,
                                 int glyphIndex,
                                 DWORD scaleXBits,
                                 DWORD scaleYBits,
                                 int width,
                                 int height,
                                 int xoff,
                                 int yoff,
                                 const unsigned char* pixels,
                                 size_t pixelCount);
DWORD GetFontTagGlyphMetricsCacheSlot(wchar_t c);
void ClearFontTagGlyphMetricsCacheSlotLocked(int slot);
void CompactFontCacheLRULocked(void);
int TouchFontCacheSlotLocked(int slot);
int ScaleTextMetricClamped(int metric, float scale);
void ApplyCachedGlyphMetrics(const GlyphMetricsCacheEntry* entry,
                             float scale,
                             float fallbackScale,
                             GlyphMetrics* out);

void ReleaseMappedFont(unsigned char* buffer, HANDLE hFile, HANDLE hMapping);
BOOL InitFontInfoFromBuffer(stbtt_fontinfo* fontInfo,
                            const unsigned char* buffer,
                            const char* pathForLog);
BOOL InitFontInfoFromBufferW(stbtt_fontinfo* fontInfo,
                             const unsigned char* buffer,
                             const wchar_t* pathForLog);
BOOL CalculateBitmapPixelCount(int width, int height, size_t* outPixelCount);
BOOL ClipTextBitmapToDestination(int x, int y,
                                 int bitmapWidth, int bitmapHeight,
                                 int destWidth, int destHeight,
                                 TextBitmapClip* clip);
int ClampTextInt64(long long value);
int AddTextIntClamped(int value, int delta);
int MulTextIntClamped(int value, int factor);
BOOL IsFontMappingSizeAllowed(HANDLE hFile, const wchar_t* pathForLog);
BOOL GetFontFileInfoFromHandle(HANDLE hFile,
                               FILETIME* lastWriteTime,
                               ULONGLONG* fileSizeOut);
BOOL GetFontFileInfoFromPathW(const wchar_t* path,
                              FILETIME* lastWriteTime,
                              ULONGLONG* fileSizeOut);
BOOL IsCurrentMainFontFileStillCurrentLocked(const char* fontFilePath);
unsigned char* LoadFontMappingW(const wchar_t* path,
                                HANDLE* phFile,
                                HANDLE* phMapping);

void ClearFontCacheSTBLocked(void);
BOOL ResolveFontTagPath(const wchar_t* fontPath,
                        wchar_t* outPath,
                        size_t pathSize);

long long GradientPositionFixed(long long x, int startX, int totalWidth);
int InterpolateGradientChannelFixed(int from, int to, long long position);
void AdvanceGradientPositionFixed(long long* position, long long step);
void InitGlowGradientContext(GlowGradientContext* ctx,
                             const GradientInfo* info,
                             int startX,
                             int totalWidth,
                             int timeOffset);
void GetGlowGradientColor(int x, int y,
                          int* r, int* g, int* b,
                          const void* userData);
BOOL GradientLUTMatches(const GradientInfo* info);
void InitializeGradientLUT(const GradientInfo* info);

#endif /* DRAWING_TEXT_STB_INTERNAL_H */
