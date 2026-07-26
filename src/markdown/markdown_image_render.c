/**
 * @file markdown_image_render.c
 * @brief Render-size calculation and GDI+ drawing for Markdown images.
 */

#include "markdown_image_internal.h"
#include "drawing/drawing_image.h"
#include "plugin/plugin_data.h"
#include "config.h"
#include <limits.h>

BOOL CalculateImageRenderSize(MarkdownImage* image,
                              int maxWidth, int maxHeight,
                              int* outWidth, int* outHeight) {
    if (!image || !outWidth || !outHeight ||
        maxWidth <= 0 || maxHeight <= 0) {
        return FALSE;
    }
    *outWidth = 0;
    *outHeight = 0;

    if (!image->resolvedPath && !ResolveImagePath(image)) {
        return FALSE;
    }

    int imgW = image->intrinsicWidth;
    int imgH = image->intrinsicHeight;
    if (imgW <= 0 || imgH <= 0) {
        if (!GetImageDimensions(image->resolvedPath, &imgW, &imgH) ||
            imgW <= 0 || imgH <= 0) {
            return FALSE;
        }
        image->intrinsicWidth = imgW;
        image->intrinsicHeight = imgH;
    }

    float scale = PluginData_IsActive()
        ? PLUGIN_FONT_SCALE_FACTOR
        : CLOCK_FONT_SCALE_FACTOR;
    if (scale < 0.1f) {
        scale = 1.0f;
    }

    int targetW;
    int targetH;
    if (image->specifiedWidth > 0 && image->specifiedHeight > 0) {
        if (!ScaleIntToInt(image->specifiedWidth, scale, &targetW) ||
            !ScaleIntToInt(image->specifiedHeight, scale, &targetH)) {
            return FALSE;
        }
    } else if (image->specifiedWidth > 0) {
        if (!ScaleIntToInt(image->specifiedWidth, scale, &targetW)) {
            return FALSE;
        }
        double computedH = (double)imgH *
                           ((double)targetW / (double)imgW);
        if (computedH <= 0.0 || computedH > (double)INT_MAX) {
            return FALSE;
        }
        targetH = (int)computedH;
    } else if (image->specifiedHeight > 0) {
        if (!ScaleIntToInt(image->specifiedHeight, scale, &targetH)) {
            return FALSE;
        }
        double computedW = (double)imgW *
                           ((double)targetH / (double)imgH);
        if (computedW <= 0.0 || computedW > (double)INT_MAX) {
            return FALSE;
        }
        targetW = (int)computedW;
    } else if (!ScaleIntToInt(imgW, scale, &targetW) ||
               !ScaleIntToInt(imgH, scale, &targetH)) {
        return FALSE;
    }

    if (targetW > maxWidth) {
        targetW = maxWidth;
    }
    if (targetH > maxHeight) {
        targetH = maxHeight;
    }

    float scaleX = (float)targetW / imgW;
    float scaleY = (float)targetH / imgH;
    float fitScale = scaleX < scaleY ? scaleX : scaleY;
    if (!ScaleIntToInt(imgW, fitScale, outWidth) ||
        !ScaleIntToInt(imgH, fitScale, outHeight)) {
        return FALSE;
    }
    return TRUE;
}

int RenderMarkdownImageSized(HDC hdc, MarkdownImage* image,
                             int x, int y,
                             int actualWidth, int actualHeight) {
    if (!hdc || !image || !image->resolvedPath ||
        actualWidth <= 0 || actualHeight <= 0) {
        return 0;
    }

    if (RenderImageGDIPlus(hdc, x, y, actualWidth, actualHeight,
                           image->resolvedPath) &&
        SetImageRenderRect(image, x, y, actualWidth, actualHeight)) {
        return actualHeight;
    }
    return 0;
}

int RenderMarkdownImageSizedWithContext(ImageRenderContext* renderCtx,
                                        MarkdownImage* image,
                                        int x, int y,
                                        int actualWidth,
                                        int actualHeight) {
    if (!renderCtx || !image || !image->resolvedPath ||
        actualWidth <= 0 || actualHeight <= 0) {
        return 0;
    }

    if (!RenderImageGDIPlusWithContext(
            renderCtx, x, y, actualWidth, actualHeight,
            image->resolvedPath) ||
        !SetImageRenderRect(image, x, y, actualWidth, actualHeight)) {
        return 0;
    }
    return actualHeight;
}

int RenderMarkdownImage(HDC hdc, MarkdownImage* image, int x, int y,
                        int maxWidth, int maxHeight) {
    if (!hdc || !image) {
        return 0;
    }

    int actualWidth = 0;
    int actualHeight = 0;
    if (!CalculateImageRenderSize(image, maxWidth, maxHeight,
                                  &actualWidth, &actualHeight)) {
        return 0;
    }
    return RenderMarkdownImageSized(hdc, image, x, y,
                                    actualWidth, actualHeight);
}
