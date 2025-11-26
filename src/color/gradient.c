/**
 * @file gradient.c
 * @brief Implementation of centralized gradient definitions
 */

#include "color/gradient.h"
#include <string.h>
#include <strings.h> // for strcasecmp

/* ============================================================================
 * Gradient Definitions Table
 * ============================================================================ */

static const COLORREF STREAMER_PALETTE[] = {
    RGB(255, 167, 69),  // #FFA745
    RGB(254, 134, 159), // #FE869F
    RGB(239, 122, 200), // #EF7AC8
    RGB(160, 131, 237), // #A083ED
    RGB(67, 174, 255),  // #43AEFF
    RGB(160, 131, 237), // #A083ED
    RGB(239, 122, 200), // #EF7AC8
    RGB(254, 134, 159), // #FE869F
    RGB(255, 167, 69)   // #FFA745
};

static const GradientInfo GRADIENT_REGISTRY[] = {
    {
        GRADIENT_CANDY,
        "#FF5E96_#56C6FF",
        NULL,
        RGB(255, 94, 150),  // #FF5E96
        RGB(86, 198, 255),  // #56C6FF
        FALSE, NULL, 0
    },
    {
        GRADIENT_BREEZE,
        "#648CFF_#64DC78",
        NULL,
        RGB(100, 140, 255), // #648CFF
        RGB(100, 220, 120), // #64DC78
        FALSE, NULL, 0
    },
    {
        GRADIENT_SUNSET,
        "#FF9A56_#56CCBA",
        NULL,
        RGB(255, 154, 86),  // #FF9A56
        RGB(86, 204, 186),  // #56CCBA
        FALSE, NULL, 0
    },
    {
        GRADIENT_STREAMER,
        "#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF",
        L"Streamer Gradient",
        RGB(255, 167, 69),  // #FFA745 (Orange)
        RGB(67, 174, 255),  // #43AEFF (Blue)
        TRUE, 
        STREAMER_PALETTE, 
        sizeof(STREAMER_PALETTE) / sizeof(STREAMER_PALETTE[0])
    }
};

#define REGISTRY_COUNT (sizeof(GRADIENT_REGISTRY) / sizeof(GRADIENT_REGISTRY[0]))

/* ============================================================================
 * Public API
 * ============================================================================ */

const GradientInfo* GetGradientInfo(GradientType type) {
    if (type <= GRADIENT_NONE || type >= GRADIENT_COUNT) return NULL;
    
    for (size_t i = 0; i < REGISTRY_COUNT; i++) {
        if (GRADIENT_REGISTRY[i].type == type) {
            return &GRADIENT_REGISTRY[i];
        }
    }
    return NULL;
}

GradientType GetGradientTypeByName(const char* name) {
    if (!name) return GRADIENT_NONE;
    
    for (size_t i = 0; i < REGISTRY_COUNT; i++) {
        if (strcasecmp(name, GRADIENT_REGISTRY[i].name) == 0) {
            return GRADIENT_REGISTRY[i].type;
        }
    }
    return GRADIENT_NONE;
}

int GetGradientCount(void) {
    return (int)REGISTRY_COUNT;
}

const GradientInfo* GetGradientInfoByIndex(int index) {
    if (index < 0 || index >= (int)REGISTRY_COUNT) return NULL;
    return &GRADIENT_REGISTRY[index];
}

BOOL IsGradientAnimated(GradientType type) {
    const GradientInfo* info = GetGradientInfo(type);
    return info ? info->isAnimated : FALSE;
}

void DrawGradientRect(HDC hdc, const RECT* rect, const GradientInfo* info) {
    if (!hdc || !rect || !info) return;
    
    int width = rect->right - rect->left;
    
    /* Multi-stop gradient logic (e.g. Streamer) */
    if (info->palette && info->paletteCount > 1) {
        const int colorCount = info->paletteCount;
        const COLORREF* colors = info->palette;

        for (int x = 0; x < width; x++) {
            float t = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;
            
            // Map t to color array index
            float scaledT = t * (colorCount - 1);
            int idx = (int)scaledT;
            int nextIdx = idx + 1;
            if (nextIdx >= colorCount) nextIdx = colorCount - 1;
            
            float frac = scaledT - idx;
            
            COLORREF c1 = colors[idx];
            COLORREF c2 = colors[nextIdx];
            
            int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * frac);
            int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * frac);
            int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * frac);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
            HPEN oldPen = SelectObject(hdc, hPen);
            
            MoveToEx(hdc, rect->left + x, rect->top, NULL);
            LineTo(hdc, rect->left + x, rect->bottom);
            
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }
        return;
    }
    
    /* Standard 2-color gradient */
    int r1 = GetRValue(info->startColor);
    int g1 = GetGValue(info->startColor);
    int b1 = GetBValue(info->startColor);
    
    int r2 = GetRValue(info->endColor);
    int g2 = GetGValue(info->endColor);
    int b2 = GetBValue(info->endColor);
    
    for (int x = 0; x < width; x++) {
        float t = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);
        
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN oldPen = SelectObject(hdc, hPen);
        
        MoveToEx(hdc, rect->left + x, rect->top, NULL);
        LineTo(hdc, rect->left + x, rect->bottom);
        
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }
}
