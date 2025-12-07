#ifndef DRAWING_EFFECT_H
#define DRAWING_EFFECT_H

#include <windows.h>

/**
 * @brief Callback for retrieving color at specific coordinates
 * @param x Screen X coordinate
 * @param y Screen Y coordinate
 * @param r Output Red
 * @param g Output Green
 * @param b Output Blue
 * @param userData Custom data passed to the callback
 */
typedef void (*GlowColorCallback)(int x, int y, int* r, int* g, int* b, void* userData);

/**
 * @brief Render a glow effect around a bitmap using Gaussian blur
 * 
 * @param pixels Destination 32-bit pixel buffer (ARGB)
 * @param destWidth Width of destination buffer
 * @param destHeight Height of destination buffer
 * @param x_pos Destination X position
 * @param y_pos Destination Y position
 * @param bitmap Source 8-bit alpha bitmap
 * @param w Source bitmap width
 * @param h Source bitmap height
 * @param r Glow color Red (0-255) - Used if callback is NULL
 * @param g Glow color Green (0-255) - Used if callback is NULL
 * @param b Glow color Blue (0-255) - Used if callback is NULL
 * @param colorCb Optional callback for per-pixel color (e.g., for gradients). If NULL, uses r,g,b.
 * @param userData User data passed to colorCb
 */
void RenderGlowEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData);

/**
 * @brief Render a glass/liquid crystal effect
 * 
 * @param pixels Destination 32-bit pixel buffer (ARGB)
 * @param destWidth Width of destination buffer
 * @param destHeight Height of destination buffer
 * @param x_pos Destination X position
 * @param y_pos Destination Y position
 * @param bitmap Source 8-bit alpha bitmap
 * @param w Source bitmap width
 * @param h Source bitmap height
 * @param r Base color Red (0-255)
 * @param g Base color Green (0-255)
 * @param b Base color Blue (0-255)
 * @param colorCb Optional callback for per-pixel color
 * @param userData User data passed to colorCb
 */
void RenderGlassEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData);

/**
 * @brief Render Hong Kong style Neon Tube effect (Double-line Outline + Glow)
 */
void RenderNeonEffect(DWORD* pixels, int destWidth, int destHeight,
                      int x_pos, int y_pos,
                      unsigned char* bitmap, int w, int h,
                      int r, int g, int b,
                      GlowColorCallback colorCb, void* userData);

/**
 * @brief Render Holographic/Prism Dispersion effect
 * @param timeOffset Time-based offset for animation
 */
void RenderHolographicEffect(DWORD* pixels, int destWidth, int destHeight,
                            int x_pos, int y_pos,
                            unsigned char* bitmap, int w, int h,
                            int r, int g, int b,
                            GlowColorCallback colorCb, void* userData,
                            int timeOffset);

/**
 * @brief Apply Gaussian blur approximation to a single-channel bitmap
 * 
 * @param src Source bitmap
 * @param dest Destination bitmap (must be same size as src)
 * @param tempBuffer Temporary buffer for intermediate results (must be same size as src)
 * @param w Bitmap width
 * @param h Bitmap height
 * @param radius Blur radius
 */
void ApplyGaussianBlur(unsigned char* src, unsigned char* dest, unsigned char* tempBuffer, int w, int h, int radius);

/**
 * @brief Free static resources used by drawing effects
 */
void CleanupDrawingEffects(void);

#endif /* DRAWING_EFFECT_H */
