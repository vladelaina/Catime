/**
 * @file gradient.c
 * @brief Implementation of centralized gradient definitions
 */

#include "color/gradient.h"
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

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
static COLORREF g_CustomPalette[MAX_CUSTOM_GRADIENT_COLORS];
static char g_CustomNameBuffer[COLOR_HEX_BUFFER];
static uint32_t g_CustomVersion = 0;
static SRWLOCK g_CustomGradientLock = SRWLOCK_INIT;

static int HexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static BOOL ParseHexColor(const char* hex, COLORREF* out) {
    if (!hex || !out) return FALSE;

    const char* start = (hex[0] == '#') ? hex + 1 : hex;
    if (strlen(start) != 6) return FALSE;

    int channels[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        int hi = HexDigitValue(start[i * 2]);
        int lo = HexDigitValue(start[i * 2 + 1]);
        if (hi < 0 || lo < 0) return FALSE;
        channels[i] = hi * 16 + lo;
    }

    *out = RGB(channels[0], channels[1], channels[2]);
    return TRUE;
}

static const GradientInfo* FindGradientInfoByType(GradientType type) {
    if (type <= GRADIENT_NONE || type >= GRADIENT_COUNT) return NULL;

    for (size_t i = 0; i < REGISTRY_COUNT; i++) {
        if (GRADIENT_REGISTRY[i].type == type) {
            return &GRADIENT_REGISTRY[i];
        }
    }
    return NULL;
}

static const GradientInfo* FindGradientInfoByName(const char* name) {
    if (!name) return NULL;

    for (size_t i = 0; i < REGISTRY_COUNT; i++) {
        if (strcasecmp(name, GRADIENT_REGISTRY[i].name) == 0) {
            return &GRADIENT_REGISTRY[i];
        }
    }
    return NULL;
}

static void CopyGradientInfoToSnapshot(const GradientInfo* info, GradientInfoSnapshot* out) {
    if (!info || !out) return;

    ZeroMemory(out, sizeof(*out));
    out->info = *info;
    if (info->name) {
        strncpy(out->name, info->name, sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        out->info.name = out->name;
    }
    if (info->palette && info->paletteCount > 0) {
        int count = info->paletteCount;
        if (count > MAX_CUSTOM_GRADIENT_COLORS) count = MAX_CUSTOM_GRADIENT_COLORS;
        memcpy(out->palette, info->palette, (size_t)count * sizeof(out->palette[0]));
        out->info.palette = out->palette;
        out->info.paletteCount = count;
    }
}

static BOOL BuildCustomGradientSnapshot(const char* name, GradientInfoSnapshot* out) {
    if (!name || !out || !strchr(name, '_')) return FALSE;
    if (strlen(name) >= sizeof(out->name)) return FALSE;

    ZeroMemory(out, sizeof(*out));
    out->info.type = GRADIENT_CUSTOM;
    out->info.name = out->name;
    out->info.displayName = L"Custom Gradient";

    strncpy(out->name, name, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';

    char tempName[GRADIENT_NAME_BUFFER];
    strncpy(tempName, name, sizeof(tempName) - 1);
    tempName[sizeof(tempName) - 1] = '\0';

    int count = 0;
    char* ctx = NULL;
    const char* token = strtok_s(tempName, "_", &ctx);

    while (token && count < MAX_CUSTOM_GRADIENT_COLORS) {
        COLORREF parsedColor;
        if (!ParseHexColor(token, &parsedColor)) {
            return FALSE;
        }
        out->palette[count++] = parsedColor;
        token = strtok_s(NULL, "_", &ctx);
    }

    out->info.palette = out->palette;
    out->info.paletteCount = count;

    if (count >= 1) {
        out->info.startColor = out->palette[0];
        out->info.endColor = (count > 1) ? out->palette[count - 1] : out->palette[0];
    }
    out->info.isAnimated = (count > 2);

    return count >= 2;
}

static void ParseCustomGradient(const char* name) {
    if (!name) return;

    GradientInfoSnapshot snapshot;
    if (!BuildCustomGradientSnapshot(name, &snapshot)) return;

    AcquireSRWLockExclusive(&g_CustomGradientLock);

    /* Check if already current to avoid unnecessary re-parsing */
    if (strcmp(name, g_CustomNameBuffer) == 0) {
        ReleaseSRWLockExclusive(&g_CustomGradientLock);
        return;
    }

    /* Update name buffer */
    strncpy(g_CustomNameBuffer, snapshot.name, sizeof(g_CustomNameBuffer) - 1);
    g_CustomNameBuffer[sizeof(g_CustomNameBuffer) - 1] = '\0';

    int count = snapshot.info.paletteCount;
    memcpy(g_CustomPalette, snapshot.palette, (size_t)count * sizeof(g_CustomPalette[0]));

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
    ReleaseSRWLockExclusive(&g_CustomGradientLock);
}

uint32_t GetCustomGradientVersion(void) {
    AcquireSRWLockShared(&g_CustomGradientLock);
    uint32_t version = g_CustomVersion;
    ReleaseSRWLockShared(&g_CustomGradientLock);
    return version;
}

static DWORD ColorRefToDibRgb(COLORREF color) {
    return ((DWORD)GetBValue(color)) |
           ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

static COLORREF InterpolateGradientColor(const GradientInfo* info, float t) {
    if (!info) return RGB(0, 0, 0);

    if (info->palette && info->paletteCount > 1) {
        int colorCount = info->paletteCount;
        float scaledT = t * (float)(colorCount - 1);
        int idx = (int)scaledT;
        int nextIdx = idx + 1;

        if (idx < 0) idx = 0;
        if (idx >= colorCount) idx = colorCount - 1;
        if (nextIdx >= colorCount) nextIdx = colorCount - 1;

        float frac = scaledT - (float)idx;
        COLORREF c1 = info->palette[idx];
        COLORREF c2 = info->palette[nextIdx];

        int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * frac);
        int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * frac);
        int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * frac);
        return RGB(r, g, b);
    }

    int r1 = GetRValue(info->startColor);
    int g1 = GetGValue(info->startColor);
    int b1 = GetBValue(info->startColor);

    int r2 = GetRValue(info->endColor);
    int g2 = GetGValue(info->endColor);
    int b2 = GetBValue(info->endColor);

    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);
    return RGB(r, g, b);
}

#define GRADIENT_STACK_PIXELS 256

/* ============================================================================
 * Public API
 * ============================================================================ */

const GradientInfo* GetGradientInfo(GradientType type) {
    if (type == GRADIENT_CUSTOM) return &g_CustomGradient;
    return FindGradientInfoByType(type);
}

BOOL GetGradientInfoSnapshot(GradientType type, GradientInfoSnapshot* out) {
    if (!out) return FALSE;

    if (type == GRADIENT_CUSTOM) {
        AcquireSRWLockShared(&g_CustomGradientLock);
        CopyGradientInfoToSnapshot(&g_CustomGradient, out);
        ReleaseSRWLockShared(&g_CustomGradientLock);
        return out->info.paletteCount > 0 || out->info.startColor != 0 || out->info.endColor != 0;
    }

    const GradientInfo* info = FindGradientInfoByType(type);
    if (!info) return FALSE;
    CopyGradientInfoToSnapshot(info, out);
    return TRUE;
}

GradientType GetGradientInfoSnapshotByName(const char* name, GradientInfoSnapshot* out) {
    if (!name || !out) return GRADIENT_NONE;

    const GradientInfo* info = FindGradientInfoByName(name);
    if (info) {
        CopyGradientInfoToSnapshot(info, out);
        return info->type;
    }

    if (BuildCustomGradientSnapshot(name, out)) {
        return GRADIENT_CUSTOM;
    }

    return GRADIENT_NONE;
}

GradientType GetGradientTypeByName(const char* name) {
    if (!name) return GRADIENT_NONE;

    /* 1. Check registry first */
    const GradientInfo* info = FindGradientInfoByName(name);
    if (info) {
        return info->type;
    }
    
    /* 2. Check for custom gradient format (contains '_') */
    if (strchr(name, '_')) {
        GradientInfoSnapshot snapshot;
        if (!BuildCustomGradientSnapshot(name, &snapshot)) {
            return GRADIENT_NONE;
        }
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
    GradientInfoSnapshot snapshot;
    return GetGradientInfoSnapshot(type, &snapshot) ? snapshot.info.isAnimated : FALSE;
}

BOOL IsGradientNameAnimated(const char* name) {
    GradientInfoSnapshot snapshot;
    return GetGradientInfoSnapshotByName(name, &snapshot) != GRADIENT_NONE
        ? snapshot.info.isAnimated
        : FALSE;
}

void DrawGradientRect(HDC hdc, const RECT* rect, const GradientInfo* info) {
    if (!hdc || !rect || !info) return;

    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0) return;

    DWORD stackPixels[GRADIENT_STACK_PIXELS];
    DWORD* pixels = (width <= GRADIENT_STACK_PIXELS)
        ? stackPixels
        : (DWORD*)malloc((size_t)width * sizeof(DWORD));
    if (!pixels) return;

    for (int x = 0; x < width; x++) {
        float t = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;
        pixels[x] = ColorRefToDibRgb(InterpolateGradientColor(info, t));
    }

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = 1;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(hdc,
                  rect->left, rect->top, width, height,
                  0, 0, width, 1,
                  pixels, &bi, DIB_RGB_COLORS, SRCCOPY);

    if (pixels != stackPixels) {
        free(pixels);
    }
}
