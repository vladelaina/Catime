/**
 * @file drawing_render_present.c
 * @brief Present a rendered DIB through the layered main window.
 */

#include "drawing_render_internal.h"

void PresentDrawingPaintFrame(PaintFrameContext* frame) {
    if (!frame) return;

    HWND hwnd = frame->hwnd;
    RECT rect = frame->rect;
    const PaintTextBuffers* paintBuffers = frame->paintBuffers;
    MarkdownImage* images = frame->images;
    int imageCount = frame->imageCount;
    BOOL imagesHeapAllocated = frame->imagesHeapAllocated;
    BOOL imagesOwnedByCache = frame->imagesOwnedByCache;
    const MarkdownColorTag* colorTags = frame->colorTags;
    int colorTagCount = frame->colorTagCount;
    HDC memDC = frame->memDC;
    HBITMAP memBitmap = frame->memBitmap;
    HBITMAP oldBitmap = frame->oldBitmap;
    BOOL usedScaleComposite = frame->usedScaleComposite;
    BOOL hasContent = frame->hasContent;

    /* Check if any color tag has gradient (multiple colors) before freeing */
    BOOL hasColorTagGradient = FALSE;
    if (colorTags && colorTagCount > 0) {
        for (int i = 0; i < colorTagCount; i++) {
            if (colorTags[i].colorCount > 1) {
                hasColorTagGradient = TRUE;
                break;
            }
        }
    }

    // Free image resources
    if (!imagesOwnedByCache) {
        FreePaintMarkdownImages(images, imageCount, imagesHeapAllocated);
    }

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        ReleaseRenderDibCache();
        StopDrawingRenderAnimationTimer(hwnd);
        RecordMainWindowRenderFailure(hwnd);
        return;
    }
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {rect.right, rect.bottom};
    POINT ptDst = {0, 0};

    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    ptDst.x = rcWindow.left;
    ptDst.y = rcWindow.top;

    BYTE alpha = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;

    BOOL layeredUpdateSucceeded = TRUE;
    if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
        DWORD err = GetLastError();
        layeredUpdateSucceeded = FALSE;
        if (err == ERROR_INVALID_PARAMETER) {
            // Error 87 often implies conflict between SetLayeredWindowAttributes and UpdateLayeredWindow
            // Reset WS_EX_LAYERED style to clear the internal state
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

            // Retry update
            if (!UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
                err = GetLastError();
                if (ShouldLogMainWindowRenderFailure()) {
                    WriteLog(LOG_LEVEL_ERROR,
                             "UpdateLayeredWindow failed retry! Error code: %lu", err);
                }
            } else {
                layeredUpdateSucceeded = TRUE;
            }
        } else {
            if (ShouldLogMainWindowRenderFailure()) {
                WriteLog(LOG_LEVEL_ERROR,
                         "UpdateLayeredWindow failed! Error code: %lu", err);
            }
        }
        if (!layeredUpdateSucceeded) {
            StopDrawingRenderAnimationTimer(hwnd);
            RecordMainWindowRenderFailure(hwnd);
        }
    }

    ReleaseDC(NULL, hdcScreen);

    UNREFERENCED_PARAMETER(memBitmap);
    UNREFERENCED_PARAMETER(oldBitmap);

    if (layeredUpdateSucceeded) {
        g_renderDibCache.frameValid = TRUE;
        g_renderDibCache.frameWasScaleComposite = usedScaleComposite;
        g_renderDibCache.frameEditMode = CLOCK_EDIT_MODE;
        g_renderDibCache.frameHwnd = hwnd;
        g_renderDibCache.frameWidth = rect.right;
        g_renderDibCache.frameHeight = rect.bottom;
        ResetMainWindowRenderRetry(hwnd);
        Timer_NotifyMainWindowPainted(paintBuffers->timerTextSnapshot);
        RefreshClickThroughState(hwnd);
        UpdateDrawingRenderAnimationTimerForFrame(hwnd, hasContent, hasColorTagGradient);
    } else {
        g_renderDibCache.frameValid = FALSE;
    }
}
