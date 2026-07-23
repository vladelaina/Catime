/**
 * @file drawing_image_gdiplus.c
 * @brief Public GDI+ image dimensions and rendering operations
 */
#include "drawing/drawing_image.h"
#include "drawing_image_gdiplus_internal.h"

#include <limits.h>

static BOOL ScaleDimension(UINT value, float scale, int* output) {
    double scaled;

    if (!output || value == 0 || scale <= 0.0f) return FALSE;
    scaled = (double)value * (double)scale;
    if (!(scaled > 0.0) || scaled > (double)INT_MAX) return FALSE;
    *output = scaled < 1.0 ? 1 : (int)scaled;
    return TRUE;
}

BOOL GetImageDimensions(const wchar_t* imagePath, int* width, int* height) {
    const CachedImageEntry* entry;

    if (!imagePath || !width || !height) return FALSE;
    *width = 0;
    *height = 0;
    if (!DrawingImage_LockState()) return FALSE;
    if (!DrawingImage_EnsureInitializedLocked()) {
        DrawingImage_UnlockState();
        return FALSE;
    }
    entry = DrawingImageCache_Get(imagePath);
    if (entry) {
        *width = (int)entry->width;
        *height = (int)entry->height;
    }
    DrawingImage_UnlockState();
    return entry != NULL;
}

BOOL BeginImageRenderContext(HDC hdc, ImageRenderContext* context) {
    GpGraphics graphics = NULL;

    if (!context) return FALSE;
    context->graphics = NULL;
    context->stateLocked = FALSE;
    if (!hdc || !DrawingImage_LockState()) return FALSE;
    context->stateLocked = TRUE;
    if (!DrawingImage_EnsureInitializedLocked() ||
        !g_drawingImageRuntime.createFromHdc ||
        !g_drawingImageRuntime.deleteGraphics ||
        !g_drawingImageRuntime.drawImageRect ||
        g_drawingImageRuntime.createFromHdc(hdc, &graphics) !=
            GDIPLUS_STATUS_OK ||
        !graphics) {
        context->stateLocked = FALSE;
        DrawingImage_UnlockState();
        return FALSE;
    }
    context->graphics = graphics;
    return TRUE;
}

void EndImageRenderContext(ImageRenderContext* context) {
    if (!context) return;
    if (context->stateLocked && context->graphics &&
        g_drawingImageRuntime.deleteGraphics) {
        g_drawingImageRuntime.deleteGraphics((GpGraphics)context->graphics);
    }
    context->graphics = NULL;
    if (context->stateLocked) {
        context->stateLocked = FALSE;
        DrawingImage_UnlockState();
    }
}

BOOL RenderImageGDIPlusWithContext(ImageRenderContext* context,
                                   int x, int y, int width, int height,
                                   const wchar_t* imagePath) {
    CachedImageEntry* entry;
    float scaleX;
    float scaleY;
    float scale;
    int drawWidth = 0;
    int drawHeight = 0;

    if (!context || !context->stateLocked || !context->graphics ||
        !imagePath || width <= 0 || height <= 0 ||
        !g_drawingImageRuntime.drawImageRect) {
        return FALSE;
    }
    entry = DrawingImageCache_Get(imagePath);
    if (!entry || !entry->bitmap || entry->width == 0 ||
        entry->height == 0) {
        return FALSE;
    }
    scaleX = (float)width / entry->width;
    scaleY = (float)height / entry->height;
    scale = scaleX < scaleY ? scaleX : scaleY;
    if (!ScaleDimension(entry->width, scale, &drawWidth) ||
        !ScaleDimension(entry->height, scale, &drawHeight)) {
        return FALSE;
    }
    return g_drawingImageRuntime.drawImageRect(
               (GpGraphics)context->graphics, (GpImage)entry->bitmap,
               x, y, drawWidth, drawHeight) == GDIPLUS_STATUS_OK;
}

BOOL RenderImageGDIPlus(HDC hdc, int x, int y, int width, int height,
                        const wchar_t* imagePath) {
    ImageRenderContext context;
    BOOL result;

    if (!BeginImageRenderContext(hdc, &context)) return FALSE;
    result = RenderImageGDIPlusWithContext(&context, x, y, width, height,
                                           imagePath);
    EndImageRenderContext(&context);
    return result;
}
