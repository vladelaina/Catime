#ifndef DRAWING_IMAGE_H
#define DRAWING_IMAGE_H

#include <windows.h>

// Initialize GDI+ subsystem
void InitDrawingImage(void);

// Shutdown GDI+ subsystem
void ShutdownDrawingImage(void);

/**
 * @brief Render an image file onto the target DC
 * @param hdc Target Device Context
 * @param x Dest X
 * @param y Dest Y
 * @param width Dest Width
 * @param height Dest Height
 * @param imagePath Full path to image file
 * @return TRUE on success
 */
BOOL RenderImageGDIPlus(HDC hdc, int x, int y, int width, int height, const wchar_t* imagePath);

/**
 * @brief Get image dimensions
 * @param imagePath Full path to image file
 * @param outWidth Output width
 * @param outHeight Output height
 * @return TRUE on success
 */
BOOL GetImageDimensions(const wchar_t* imagePath, int* outWidth, int* outHeight);

#endif
