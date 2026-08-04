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

BOOL GetImageDimensions(const wchar_t* imagePath, int* outWidth, int* outHeight) {
    const CachedImageEntry* entry;

    if (!imagePath || !outWidth || !outHeight) return FALSE;
    *outWidth = 0;
    *outHeight = 0;
    if (!DrawingImage_LockState()) return FALSE;
    if (!DrawingImage_EnsureInitializedLocked()) {
        DrawingImage_UnlockState();
        return FALSE;
    }
    entry = DrawingImageCache_Get(imagePath);
    if (entry) {
        *outWidth = (int)entry->width;
        *outHeight = (int)entry->height;
    }
    DrawingImage_UnlockState();
    return entry != NULL;
}

BOOL BeginImageRenderContext(HDC hdc, ImageRenderContext* ctx) {
    GpGraphics graphics = NULL;

    if (!ctx) return FALSE;
    ctx->graphics = NULL;
    ctx->stateLocked = FALSE;
    if (!hdc || !DrawingImage_LockState()) return FALSE;
    ctx->stateLocked = TRUE;
    if (!DrawingImage_EnsureInitializedLocked() ||
        !g_drawingImageRuntime.createFromHdc ||
        !g_drawingImageRuntime.deleteGraphics ||
        !g_drawingImageRuntime.drawImageRect ||
        g_drawingImageRuntime.createFromHdc(hdc, &graphics) !=
            GDIPLUS_STATUS_OK ||
        !graphics) {
        ctx->stateLocked = FALSE;
        DrawingImage_UnlockState();
        return FALSE;
    }
    ctx->graphics = graphics;
    return TRUE;
}

void EndImageRenderContext(ImageRenderContext* ctx) {
    if (!ctx) return;
    if (ctx->stateLocked && ctx->graphics &&
        g_drawingImageRuntime.deleteGraphics) {
        g_drawingImageRuntime.deleteGraphics((GpGraphics)ctx->graphics);
    }
    ctx->graphics = NULL;
    if (ctx->stateLocked) {
        ctx->stateLocked = FALSE;
        DrawingImage_UnlockState();
    }
}

BOOL RenderImageGDIPlusWithContext(ImageRenderContext* ctx,
                                   int x, int y, int width, int height,
                                   const wchar_t* imagePath) {
    CachedImageEntry* entry;
    float scaleX;
    float scaleY;
    float scale;
    int drawWidth = 0;
    int drawHeight = 0;

    if (!ctx || !ctx->stateLocked || !ctx->graphics ||
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
               (GpGraphics)ctx->graphics, (GpImage)entry->bitmap,
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
