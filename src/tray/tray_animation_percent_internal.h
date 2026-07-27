#ifndef TRAY_ANIMATION_PERCENT_INTERNAL_H
#define TRAY_ANIMATION_PERCENT_INTERNAL_H

#include "tray/tray_animation_percent.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ICON_MASK_STACK_BYTES 2048u
#define GENERATED_TRAY_ICON_FALLBACK_SIZE 16
#define GENERATED_TRAY_ICON_MAX_SIZE 256
#define GENERATED_PERCENT_ICON_MAX_VALUE 100
#define GENERATED_PERCENT_ICON_CACHE_SIZE \
    (GENERATED_PERCENT_ICON_MAX_VALUE + 1)

typedef struct {
    HICON icon;
    COLORREF textColor;
    COLORREF bgColor;
    int cx;
    int cy;
    BOOL valid;
} PercentIconCacheEntry;

typedef struct {
    HICON icon;
    COLORREF textColor;
    COLORREF bgColor;
    int cx;
    int cy;
    BOOL capsOn;
    BOOL valid;
} CapsIconCacheEntry;

extern COLORREF g_percentTextColor;
extern COLORREF g_percentBgColor;
extern PercentIconCacheEntry g_percentIconCache[
    GENERATED_PERCENT_ICON_CACHE_SIZE];
extern CapsIconCacheEntry g_capsIconCache[2];
extern COLORREF g_cachedThemeTextColor;
extern DWORD g_lastThemeCheckTick;
extern INIT_ONCE g_percentIconCacheLockOnce;
extern CRITICAL_SECTION g_percentIconCacheCS;

BOOL BeginPercentIconCacheAccess(void);
void EndPercentIconCacheAccess(void);
DWORD ColorRefToDibRgb(COLORREF color);
void GetGeneratedTrayIconSize(int* outCx, int* outCy);
void FillTransparentIconBackground(
    void* bits, int cx, int cy, DWORD marker);
void FillSolidIconBackground(
    void* bits, int cx, int cy, COLORREF bgColor);
void RepairTransparentIconAlpha(
    void* bits, int cx, int cy, DWORD marker);
void MakeIconFullyOpaque(void* bits, int cx, int cy);
BOOL DrawFallbackTextOnTransparentIcon(
    HDC dc, void* bits, int cx, int cy, DWORD marker,
    HFONT font, const wchar_t* text, int textLen,
    int x, int y, COLORREF textColor);
BOOL DrawAlphaTextOnTransparentIcon(
    HDC screenDc, void* targetBits, int cx, int cy,
    HFONT font, const wchar_t* text, int textLen,
    int x, int y, COLORREF textColor);
HFONT CreateFittedIconTextFont(
    HDC hdc, const wchar_t* text, int textLen,
    int maxWidth, int maxHeight, LONG weight,
    int minPixelHeight, int maxPixelHeight, SIZE* outSize);
HFONT CreateFittedMetricIconTextFont(
    HDC hdc, const wchar_t* text, int textLen,
    int maxWidth, int maxHeight, UINT dpi, SIZE* outSize);
HBITMAP CreateInitializedMaskBitmap(int cx, int cy, BYTE value);
void ClearGeneratedIconCacheLocked(void);
BOOL SnapshotIconColorsLocked(COLORREF* textColor, COLORREF* bgColor);
BOOL GetIconColorSnapshot(COLORREF* textColor, COLORREF* bgColor);

#endif
