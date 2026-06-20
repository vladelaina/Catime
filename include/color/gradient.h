/**
 * @file gradient.h
 * @brief Centralized gradient color definitions
 */

#ifndef COLOR_GRADIENT_H
#define COLOR_GRADIENT_H

#include <windows.h>
#include <stdint.h>

#define MAX_GRADIENT_STOPS 5  /* Legacy preset limit */
#define MAX_CUSTOM_GRADIENT_COLORS 20  /* Custom gradient limit */
#define GRADIENT_NAME_BUFFER 256

typedef enum {
    GRADIENT_NONE = 0,
    GRADIENT_CANDY,
    GRADIENT_BREEZE,
    GRADIENT_FROST,
    GRADIENT_SUNSET,
    GRADIENT_STREAMER,
    GRADIENT_CUSTOM,  // User-defined custom gradient
    GRADIENT_COUNT
} GradientType;

typedef struct {
    GradientType type;
    const char* name;           // Internal config name (e.g. "CANDY")
    const wchar_t* displayName; // UI Display name (e.g. "Candy Gradient")
    COLORREF startColor;        // Start color (Top/Left)
    COLORREF endColor;          // End color (Bottom/Right)
    
    /* Advanced properties */
    BOOL isAnimated;            // Requires high-frequency refresh
    const COLORREF* palette;    // Optional: Multi-stop color array
    int paletteCount;           // Count of colors in palette
} GradientInfo;

typedef struct {
    GradientInfo info;
    char name[GRADIENT_NAME_BUFFER];
    COLORREF palette[MAX_CUSTOM_GRADIENT_COLORS];
} GradientInfoSnapshot;

/**
 * @brief Get gradient info by type
 */
const GradientInfo* GetGradientInfo(GradientType type);

/**
 * @brief Copy gradient info into caller-owned storage.
 * @details Required for GRADIENT_CUSTOM because its legacy global pointer can be
 *          republished when another color string is parsed.
 */
BOOL GetGradientInfoSnapshot(GradientType type, GradientInfoSnapshot* out);

/**
 * @brief Resolve a gradient name and copy a stable snapshot into caller storage.
 * @return GRADIENT_NONE if not a known preset or custom gradient string.
 */
GradientType GetGradientInfoSnapshotByName(const char* name, GradientInfoSnapshot* out);

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

/* Helper to check if a gradient needs animation timer */
BOOL IsGradientAnimated(GradientType type);

/* Helper to check a specific gradient name without publishing custom state */
BOOL IsGradientNameAnimated(const char* name);

/* Custom gradient support */
uint32_t GetCustomGradientVersion(void);

#endif // COLOR_GRADIENT_H
