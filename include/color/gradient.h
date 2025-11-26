/**
 * @file gradient.h
 * @brief Centralized gradient color definitions
 */

#ifndef COLOR_GRADIENT_H
#define COLOR_GRADIENT_H

#include <windows.h>

typedef enum {
    GRADIENT_NONE = 0,
    GRADIENT_CANDY,
    GRADIENT_BREEZE,
    GRADIENT_SUNSET,
    /* Add new gradients here */
    GRADIENT_COUNT
} GradientType;

typedef struct {
    GradientType type;
    const char* name;           // Internal config name (e.g. "CANDY")
    const wchar_t* displayName; // UI Display name (e.g. "Candy Gradient")
    COLORREF startColor;        // Start color (Top/Left)
    COLORREF endColor;          // End color (Bottom/Right)
} GradientInfo;

/**
 * @brief Get gradient info by type
 */
const GradientInfo* GetGradientInfo(GradientType type);

/**
 * @brief Get gradient type from internal name string (case-insensitive)
 * @return GRADIENT_NONE if not found
 */
GradientType GetGradientTypeByName(const char* name);

/**
 * @brief Get the total number of defined gradients (excluding NONE)
 */
int GetGradientCount(void);

/**
 * @brief Get gradient info by index (0 to Count-1)
 * @note This is useful for iterating all gradients
 */
const GradientInfo* GetGradientInfoByIndex(int index);

/**
 * @brief Draw a horizontal gradient rectangle using GDI
 * @param hdc Device context to draw on
 * @param rect Rectangle area to fill with gradient
 * @param info Gradient info containing start/end colors
 */
void DrawGradientRect(HDC hdc, const RECT* rect, const GradientInfo* info);

#endif // COLOR_GRADIENT_H
