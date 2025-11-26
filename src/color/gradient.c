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
 * Custom Gradient State
 * ============================================================================ */

#include <stdio.h>
#include "color/color_parser.h"

static GradientInfo g_CustomGradient = {
    GRADIENT_CUSTOM,
    NULL,
    L"Custom Gradient",
    0, 0,
    FALSE,
    NULL, 0
};
static COLORREF g_CustomPalette[MAX_GRADIENT_STOPS];
static char g_CustomNameBuffer[COLOR_HEX_BUFFER];
static uint32_t g_CustomVersion = 0;

static COLORREF ParseHexColor(const char* hex) {
    int r, g, b;
    const char* start = (hex[0] == '#') ? hex + 1 : hex;
    if (sscanf(start, "%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    }
    return RGB(0, 0, 0);
}

static void ParseCustomGradient(const char* name) {
    if (!name) return;

    /* Check if already current to avoid unnecessary re-parsing */
    if (strcmp(name, g_CustomNameBuffer) == 0) return;

    /* Update name buffer */
    strncpy(g_CustomNameBuffer, name, sizeof(g_CustomNameBuffer) - 1);
    g_CustomNameBuffer[sizeof(g_CustomNameBuffer) - 1] = '\0';

    /* Parse colors */
    char tempName[COLOR_HEX_BUFFER];
    strncpy(tempName, name, sizeof(tempName) - 1);
    tempName[sizeof(tempName) - 1] = '\0';

    int count = 0;
    char* ctx = NULL;
    char* token = strtok_s(tempName, "_", &ctx);
    
    while (token && count < MAX_GRADIENT_STOPS) {
        g_CustomPalette[count++] = ParseHexColor(token);
        token = strtok_s(NULL, "_", &ctx);
    }

    /* Update global struct */
    g_CustomGradient.name = g_CustomNameBuffer;
    g_CustomGradient.palette = g_CustomPalette;
    g_CustomGradient.paletteCount = count;
    
    if (count >= 1) {
        g_CustomGradient.startColor = g_CustomPalette[0];
        g_CustomGradient.endColor = (count > 1) ? g_CustomPalette[count - 1] : g_CustomPalette[0];
    }

    /* Rule: 2 colors = static, 3+ colors = animated streamer */
    g_CustomGradient.isAnimated = (count > 2);

    /* Increment version to trigger cache invalidation */
    g_CustomVersion++;
}

uint32_t GetCustomGradientVersion(void) {
    return g_CustomVersion;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

const GradientInfo* GetGradientInfo(GradientType type) {
    if (type == GRADIENT_CUSTOM) return &g_CustomGradient;
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
    
    /* 1. Check registry first */
    for (size_t i = 0; i < REGISTRY_COUNT; i++) {
        if (strcasecmp(name, GRADIENT_REGISTRY[i].name) == 0) {
            return GRADIENT_REGISTRY[i].type;
        }
    }
    
    /* 2. Check for custom gradient format (contains '_') */
    if (strchr(name, '_')) {
        ParseCustomGradient(name);
        return GRADIENT_CUSTOM;
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
