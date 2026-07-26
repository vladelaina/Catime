/**
 * @file drawing_render_internal.h
 * @brief Cross-module declarations for the rendering pipeline.
 */

#ifndef DRAWING_RENDER_INTERNAL_H
#define DRAWING_RENDER_INTERNAL_H

#include "drawing_render_types.h"

BOOL IsValidRenderAnimationWindow(HWND hwnd);
void ResetMainWindowRenderRetry(HWND hwnd);
BOOL ArmMainWindowRenderRetry(HWND hwnd, UINT delayMs);
void RecordMainWindowRenderFailure(HWND hwnd);
BOOL ShouldLogMainWindowRenderFailure(void);
BOOL HandleDrawingRenderRetryTimer(HWND hwnd);
void AppendWideSpan(wchar_t** dst, size_t* remaining,
                           const wchar_t* text, size_t textLen);
const wchar_t* FindNextPluginTextMarker(const wchar_t* src,
                                               BOOL includeImages,
                                               PluginTextMarkerKind* markerKind);
COLORREF ParseColorString(const char* colorStr, const GradientInfo* gradientInfo);
int CalculateRenderFontSize(int baseFontSize, float scaleFactor);
BOOL HasPotentialMarkdownSyntax(const wchar_t* text);
void ClearTextMeasureCache(void);
DWORD ComputeHeadingSignature(const MarkdownHeading* headings, int headingCount);
DWORD ComputeFontTagSignature(const MarkdownFontTag* fontTags, int fontTagCount);
void ClearMarkdownRenderCache(void);
void ClearPluginPaintCache(void);
void CopyCachedWideText(wchar_t* dest, size_t destCount,
                               size_t* storedLen, const wchar_t* src);
BOOL CachedWideTextEquals(const wchar_t* cached, size_t cachedCount,
                                 size_t storedLen, const wchar_t* text);
BOOL BuildStableDigitMeasureText(const wchar_t* source, wchar_t* dest, size_t destCount);
void StabilizeScaleGestureText(HWND hwnd, wchar_t* text, size_t textCount);
BOOL CanUseMeasureCacheForFontTags(const MarkdownFontTag* fontTags, int fontTagCount);
BOOL RefreshMeasureCacheFontTags(const MarkdownFontTag* fontTags, int fontTagCount);
void EnsureMarkdownRenderCache(const wchar_t* text);
BOOL ExpandFontPathEnvironmentUtf8(const char* fontFileName, char* outPath, size_t outPathSize);
BOOL ResolveFontPathFromName(const char* fontFileName, char* outPath);
BOOL ResolveFontPathFromNameCached(const char* fontFileName,
                                          char* outPath,
                                          size_t outPathSize);
void CreateRenderContext(RenderContext* ctx);
BOOL MeasureTextMarkdown(const wchar_t* text, const RenderContext* ctx, SIZE* outSize,
                               const MarkdownHeading* headings, int headingCount,
                               const MarkdownFontTag* fontTags, int fontTagCount);
BOOL RenderTextMarkdown(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode, void* bits,
                              MarkdownLink* links, int linkCount,
                              const MarkdownHeading* headings, int headingCount,
                              MarkdownStyle* styles, int styleCount,
                              MarkdownBlockquote* blockquotes, int blockquoteCount,
                              MarkdownColorTag* colorTags, int colorTagCount,
                              const MarkdownFontTag* fontTags, int fontTagCount,
                              const SIZE* measuredSize);
void CleanupDrawingRenderCache(void);
BOOL CalculatePixelCount(int width, int height, size_t* pixelCount);
UINT GetRenderAnimationTimerInterval(size_t pixelCount, BOOL hasColorTagGradient);
UINT ChooseRenderTimerResolutionMs(UINT intervalMs);
UINT ClampRenderTimerResolutionToDeviceCaps(UINT requestedMs);
void ReleaseRenderAnimationTimerResolution(void);
void UpdateRenderAnimationTimerResolution(UINT interval);
BOOL SetDrawingRenderAnimationTimer(HWND hwnd, UINT interval);
BOOL IsActiveTextColorAnimated(void);
BOOL ShouldRunRenderAnimationTimer(BOOL hasRenderableContent,
                                          BOOL hasColorTagGradient);
BOOL UpdateDrawingRenderAnimationTimerForFrame(HWND hwnd,
                                                      BOOL hasRenderableContent,
                                                      BOOL hasColorTagGradient);
void StopDrawingRenderAnimationTimer(HWND hwnd);
int ClampRenderInt64(long long value, int minValue, int maxValue);
int AddRenderDimensionClamped(int value, int delta);
BOOL GetRenderWindowLimits(HWND hwnd, SIZE* outLimits);
BOOL GetConstrainedRenderWindowSize(HWND hwnd, const SIZE* contentSize, SIZE* outSize);
void FreePaintMarkdownImages(MarkdownImage* images, int imageCount, BOOL heapAllocated);
BOOL HasTickReached(DWORD tick);
BOOL IsMarkdownImageRetryPending(const MarkdownImage* image);
void PreparePaintMarkdownImagesForFrame(MarkdownImage* images, int imageCount);
BOOL EnsurePaintMarkdownImageCapacity(MarkdownImage** images,
                                             int* imageCapacity,
                                             BOOL* heapAllocated,
                                             MarkdownImage* stackImages);
MarkdownImage* MovePaintMarkdownImagesToHeap(MarkdownImage* images,
                                                    int imageCount,
                                                    BOOL* heapAllocated,
                                                    MarkdownImage* stackImages);
void ReleaseScaleFrameSnapshot(void);
BOOL CreateScaleFrameSnapshotSurface(HDC referenceDC,
                                            int width,
                                            int height);
BOOL TryCaptureScaleFrameSnapshot(HWND hwnd,
                                         HDC referenceDC,
                                         DWORD gestureSerial,
                                         const RECT* currentRect);
BOOL CompositeScaleFrameSnapshot(HWND hwnd,
                                        DWORD gestureSerial,
                                        HDC destDC,
                                        void* destBits,
                                        int destWidth,
                                        int destHeight);
void ReleaseRenderDibCache(void);
BOOL ShouldReuseRenderDibCache(int width, int height, size_t requiredPixels);
BOOL SetupDoubleBufferDIB(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap, void** ppvBits);
void FixAlphaChannel(void* bits, int width, int height);
void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect);
void HandleWindowPaint(HWND hwnd, const PAINTSTRUCT* ps);
BOOL PrepareDrawingPaintFrame(PaintFrameContext* frame, HWND hwnd, const PAINTSTRUCT* ps);
BOOL RenderDrawingPaintFrame(PaintFrameContext* frame);
void PresentDrawingPaintFrame(PaintFrameContext* frame);

#endif /* DRAWING_RENDER_INTERNAL_H */
