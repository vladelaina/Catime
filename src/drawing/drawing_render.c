/**
 * @file drawing_render.c
 * @brief Public entry point for the GDI rendering pipeline.
 */

#include "drawing_render_internal.h"
#include "main/main_initialization.h"

void HandleWindowPaint(HWND hwnd, const PAINTSTRUCT* ps) {
    static BOOL ciPaintLogged = FALSE;
    if (IsCiSmokeMode() && !ciPaintLogged) {
        ciPaintLogged = TRUE;
        WriteLog(LOG_LEVEL_INFO, "CI smoke WM_PAINT dispatched (dragging=%d)",
                 CLOCK_IS_DRAGGING);
    }
    if (!ps) return;
    if (CLOCK_IS_DRAGGING) {
        ReleaseScaleFrameSnapshot();
        return;
    }

    PaintFrameContext frame = {0};
    if (!PrepareDrawingPaintFrame(&frame, hwnd, ps)) {
        return;
    }
    if (!RenderDrawingPaintFrame(&frame)) {
        return;
    }
    PresentDrawingPaintFrame(&frame);
}
