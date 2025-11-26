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

static const GradientInfo GRADIENT_REGISTRY[] = {
    {
        GRADIENT_CANDY,
        "#FF5E96_#56C6FF",
        NULL,
        RGB(255, 94, 150),  // #FF5E96
        RGB(86, 198, 255)   // #56C6FF
    },
    {
        GRADIENT_BREEZE,
        "#648CFF_#64DC78",
        NULL,
        RGB(100, 140, 255), // #648CFF
        RGB(100, 220, 120)  // #64DC78
    },
    {
        GRADIENT_SUNSET,
        "#FF9A56_#56CCBA",
        NULL,
        RGB(255, 154, 86),  // #FF9A56
        RGB(86, 204, 186)   // #56CCBA
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

void DrawGradientRect(HDC hdc, const RECT* rect, const GradientInfo* info) {
    if (!hdc || !rect || !info) return;
    
    int r1 = GetRValue(info->startColor);
    int g1 = GetGValue(info->startColor);
    int b1 = GetBValue(info->startColor);
    
    int r2 = GetRValue(info->endColor);
    int g2 = GetGValue(info->endColor);
    int b2 = GetBValue(info->endColor);
    
    int width = rect->right - rect->left;
    
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
