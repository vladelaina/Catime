/**
 * @file tray_animation_decoder.h
 * @brief Image decoding with WIC (Windows Imaging Component)
 * 
 * Decodes GIF/WebP animations and static images, pre-compositing all frames.
 * Uses memory pool for temporary buffers to reduce malloc overhead.
 */

#ifndef TRAY_ANIMATION_DECODER_H
#define TRAY_ANIMATION_DECODER_H

#include <windows.h>
#include <wincodec.h>
#include "utils/memory_pool.h"

/**
 * @brief Decoded animation frames
 */
typedef struct {
    HICON* icons;           /**< Array of pre-rendered icons */
    int count;              /**< Number of frames */
    UINT* delays;           /**< Frame delays in milliseconds */
    BOOL isAnimated;        /**< TRUE if multi-frame */
    UINT canvasWidth;       /**< Logical canvas width */
    UINT canvasHeight;      /**< Logical canvas height */
    BYTE* canvas;           /**< Composition canvas (PBGRA) */
} DecodedAnimation;

/**
 * @brief Initialize decoded animation structure
 * @param anim Structure to initialize
 */
void DecodedAnimation_Init(DecodedAnimation* anim);

/**
 * @brief Free all resources in decoded animation
 * @param anim Structure to cleanup
 */
void DecodedAnimation_Free(DecodedAnimation* anim);

/**
 * @brief Decode animated image (GIF or WebP)
 * @param utf8Path File path
 * @param anim Output structure
 * @param pool Memory pool for temporary buffers (optional)
 * @param iconWidth Target icon width (e.g., SM_CXSMICON)
 * @param iconHeight Target icon height (e.g., SM_CYSMICON)
 * @return TRUE on success, FALSE on failure
 * 
 * @details
 * Pre-composites all frames with disposal handling.
 * Allocates canvas and icon array in anim structure.
 * Caller must call DecodedAnimation_Free() when done.
 */
BOOL DecodeAnimatedImage(const char* utf8Path, DecodedAnimation* anim, 
                         MemoryPool* pool, int iconWidth, int iconHeight);

/**
 * @brief Decode static image to single icon
 * @param utf8Path File path
 * @param iconWidth Target width
 * @param iconHeight Target height
 * @return HICON or NULL on failure
 */
HICON DecodeStaticImage(const char* utf8Path, int iconWidth, int iconHeight);

/**
 * @brief Create icon from WIC bitmap source
 * @param pFactory WIC factory instance
 * @param source Bitmap source (any format)
 * @param cx Target icon width
 * @param cy Target icon height
 * @return HICON or NULL on failure
 * 
 * @details
 * Scales image preserving aspect ratio, centers if needed.
 * Converts to 32bpp PBGRA for alpha transparency.
 */
HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                               IWICBitmapSource* source,
                               int cx, int cy);

/**
 * @brief Create icon from PBGRA pixel buffer
 * @param pFactory WIC factory instance
 * @param canvasPixels PBGRA pixels (bottom-up)
 * @param canvasWidth Canvas width
 * @param canvasHeight Canvas height
 * @param cx Target icon width
 * @param cy Target icon height
 * @return HICON or NULL on failure
 */
HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                          const BYTE* canvasPixels,
                          UINT canvasWidth, UINT canvasHeight,
                          int cx, int cy);

/**
 * @brief Blend pixel onto canvas with alpha compositing
 * @param canvas Canvas buffer (PBGRA format)
 * @param canvasStride Bytes per row
 * @param x X coordinate
 * @param y Y coordinate
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param a Alpha (0-255)
 * 
 * @details Uses "source over" blending for transparent pixels
 */
void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y,
                BYTE r, BYTE g, BYTE b, BYTE a);

/**
 * @brief Clear canvas rectangle (for GIF disposal mode 2)
 * @param canvas Canvas buffer
 * @param canvasWidth Canvas width
 * @param canvasHeight Canvas height
 * @param left Left coordinate
 * @param top Top coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 * @param bgR Background red
 * @param bgG Background green
 * @param bgB Background blue
 * @param bgA Background alpha
 */
void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight,
                     UINT left, UINT top, UINT width, UINT height,
                     BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA);

#endif /* TRAY_ANIMATION_DECODER_H */

