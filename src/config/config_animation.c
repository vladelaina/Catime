/**
 * @file config_animation.c
 * @brief Animation configuration management
 * 
 * Manages animation speed settings, percent icon colors, and animation-related configuration.
 */
#include "config.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_percent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <windows.h>

typedef struct {
    int percent;
    double scalePercent;
} AnimSpeedPoint;

static AnimSpeedPoint g_animSpeedPoints[128];
static int g_animSpeedPointCount = 0;
static double g_animSpeedDefaultScalePercent = 100.0;
static AnimationSpeedMetric g_animSpeedMetric = ANIMATION_SPEED_MEMORY;
static int g_animMinIntervalMs = 0;

static int CmpAnimSpeedPoint(const void* a, const void* b) {
    const AnimSpeedPoint* pa = (const AnimSpeedPoint*)a;
    const AnimSpeedPoint* pb = (const AnimSpeedPoint*)b;
    if (pa->percent < pb->percent) return -1;
    if (pa->percent > pb->percent) return 1;
    return 0;
}

static void TrimSpaces(char* s) {
    if (!s) return;
    size_t len = strlen(s);
    size_t i = 0; while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i > 0) memmove(s, s + i, len - i + 1);
    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = '\0';
}

static void ParseAnimationSpeedFixedKeys(const char* configPathUtf8) {
    g_animSpeedPointCount = 0;
    if (!configPathUtf8 || !*configPathUtf8) return;

    {
        int def = ReadIniInt("Animation", "ANIMATION_SPEED_DEFAULT", 100, configPathUtf8);
        if (def <= 0) def = 100;
        g_animSpeedDefaultScalePercent = (double)def;
    }

    wchar_t wSection[64];
    wchar_t wfilePath[MAX_PATH];
    const char* sectionUtf8 = "Animation";
    MultiByteToWideChar(CP_UTF8, 0, sectionUtf8, -1, wSection, 64);
    MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wfilePath, MAX_PATH);

    const DWORD kBufChars = 64 * 1024;
    wchar_t* wbuf = (wchar_t*)malloc(sizeof(wchar_t) * kBufChars);
    if (!wbuf) return;
    ZeroMemory(wbuf, sizeof(wchar_t) * kBufChars);

    DWORD copied = GetPrivateProfileSectionW(wSection, wbuf, kBufChars, wfilePath);
    if (copied == 0) {
        free(wbuf);
        return;
    }

    const wchar_t* prefix = L"ANIMATION_SPEED_MAP_";
    size_t prefixLen = wcslen(prefix);

    wchar_t* p = wbuf;
    while (*p) {
        wchar_t* eq = wcschr(p, L'=');
        if (eq) {
            *eq = L'\0';
            const wchar_t* key = p;
            wchar_t* value = eq + 1;

            if (wcsncmp(key, prefix, prefixLen) == 0) {
                const wchar_t* token = key + prefixLen;
                const wchar_t* dash = wcschr(token, L'-');
                if (!dash) {
                    /** New-style: ANIMATION_SPEED_MAP_PERCENT=VALUE */
                    while (*value == L' ' || *value == L'\t') value++;
                    wchar_t* end = value + wcslen(value);
                    while (end > value && (end[-1] == L' ' || end[-1] == L'\t' || end[-1] == L'\r' || end[-1] == L'\n')) { *--end = L'\0'; }
                    if (end > value && end[-1] == L'%') { end[-1] = L'\0'; }

                    double scale = _wtof(value);
                    if (scale <= 0.0) scale = 100.0;

                    int percent = _wtoi(token);
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                    if (g_animSpeedPointCount < (int)(sizeof(g_animSpeedPoints)/sizeof(g_animSpeedPoints[0]))) {
                        g_animSpeedPoints[g_animSpeedPointCount].percent = percent;
                        g_animSpeedPoints[g_animSpeedPointCount].scalePercent = scale;
                        g_animSpeedPointCount++;
                    }
                }
            }
        }
        p += wcslen(p) + 1;
    }

    /** Sort breakpoints by percent ascending */
    if (g_animSpeedPointCount > 1) {
        qsort(g_animSpeedPoints, g_animSpeedPointCount, sizeof(AnimSpeedPoint), CmpAnimSpeedPoint);
    }

    free(wbuf);
}

AnimationSpeedMetric GetAnimationSpeedMetric(void) {
    return g_animSpeedMetric;
}

void WriteConfigAnimationSpeedMetric(AnimationSpeedMetric metric) {
    g_animSpeedMetric = metric;
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    const char* metricStr = (metric == ANIMATION_SPEED_CPU ? "CPU" :
                          (metric == ANIMATION_SPEED_TIMER ? "TIMER" : "MEMORY"));
    WriteIniString("Animation", "ANIMATION_SPEED_METRIC", metricStr, config_path);
}

double GetAnimationSpeedScaleForPercent(double percent) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    /** New-style breakpoints with linear interpolation */
    if (g_animSpeedPointCount > 0) {
        /** If below first breakpoint, interpolate from default to first */
        if (percent <= (double)g_animSpeedPoints[0].percent) {
            double p0 = 0.0;
            double s0 = g_animSpeedDefaultScalePercent;
            double p1 = (double)g_animSpeedPoints[0].percent;
            double s1 = g_animSpeedPoints[0].scalePercent;
            if (p1 <= p0) return s1;
            double t = (percent - p0) / (p1 - p0);
            return s0 + (s1 - s0) * t;
        }
        /** Find segment [i, i+1] that contains percent */
        for (int i = 0; i < g_animSpeedPointCount - 1; ++i) {
            double p0 = (double)g_animSpeedPoints[i].percent;
            double p1 = (double)g_animSpeedPoints[i + 1].percent;
            if (percent >= p0 && percent <= p1) {
                double s0 = g_animSpeedPoints[i].scalePercent;
                double s1 = g_animSpeedPoints[i + 1].scalePercent;
                if (p1 <= p0) return s1;
                double t = (percent - p0) / (p1 - p0);
                return s0 + (s1 - s0) * t;
            }
        }
        /** Above last breakpoint: clamp to last scale */
        return g_animSpeedPoints[g_animSpeedPointCount - 1].scalePercent;
    }

    /** No breakpoints: return default */
    return g_animSpeedDefaultScalePercent;
}

void ReloadAnimationSpeedFromConfig(void) {
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, MAX_PATH);
    char metric[32] = {0};
    ReadIniString("Animation", "ANIMATION_SPEED_METRIC", "MEMORY", metric, sizeof(metric), config_path);
    if (_stricmp(metric, "CPU") == 0) {
        g_animSpeedMetric = ANIMATION_SPEED_CPU;
    } else if (_stricmp(metric, "TIMER") == 0 || _stricmp(metric, "COUNTDOWN") == 0) {
        g_animSpeedMetric = ANIMATION_SPEED_TIMER;
    } else {
        g_animSpeedMetric = ANIMATION_SPEED_MEMORY;
    }
    ParseAnimationSpeedFixedKeys(config_path);

    /** Base animation playback speed */
    int folderInterval = ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS", 150, config_path);
    if (folderInterval <= 0) folderInterval = 150;
    TrayAnimation_SetBaseIntervalMs((UINT)folderInterval);

    /** Optional minimum speed limit */
    int minInterval = ReadIniInt("Animation", "ANIMATION_MIN_INTERVAL_MS", 0, config_path);
    if (minInterval < 0) minInterval = 0;
    g_animMinIntervalMs = minInterval;
    TrayAnimation_SetMinIntervalMs((UINT)g_animMinIntervalMs);
}

static int ParseHex2(const char* s) {
    int v = 0; sscanf(s, "%2x", &v); return v & 0xFF;
}

static BOOL ParseColorString(const char* str, COLORREF* out) {
    if (!str || !out) return FALSE;
    if (str[0] == '#') {
        if (strlen(str) == 7) {
            int r = ParseHex2(str + 1);
            int g = ParseHex2(str + 3);
            int b = ParseHex2(str + 5);
            *out = RGB(r, g, b);
            return TRUE;
        }
        return FALSE;
    }
    int r=0,g=0,b=0;
    if (sscanf(str, "%d,%d,%d", &r, &g, &b) == 3) {
        if (r<0) r=0; if (r>255) r=255;
        if (g<0) g=0; if (g>255) g=255;
        if (b<0) b=0; if (b>255) b=255;
        *out = RGB(r, g, b);
        return TRUE;
    }
    return FALSE;
}

void ReadPercentIconColorsConfig(void) {
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, MAX_PATH);
    char textBuf[32] = {0};
    char bgBuf[32] = {0};
    ReadIniString("Animation", "PERCENT_ICON_TEXT_COLOR", "auto", textBuf, sizeof(textBuf), config_path);
    ReadIniString("Animation", "PERCENT_ICON_BG_COLOR", "transparent", bgBuf, sizeof(bgBuf), config_path);

    COLORREF textColor = RGB(0, 0, 0);
    COLORREF bgColor = TRANSPARENT_BG_AUTO;

    /* Parse text color: empty or "auto" means theme-based (handled in CreatePercentIcon16) */
    if (textBuf[0] == '\0' || strcasecmp(textBuf, "auto") == 0) {
        textColor = RGB(0, 0, 0);  /* Placeholder, will be overridden by theme detection */
    } else if (!ParseColorString(textBuf, &textColor)) {
        textColor = RGB(0, 0, 0);  /* Fallback to black */
    }

    /* Parse background color: empty or "transparent" means transparent with theme-aware text */
    if (bgBuf[0] == '\0' || strcasecmp(bgBuf, "transparent") == 0) {
        bgColor = TRANSPARENT_BG_AUTO;
    } else if (!ParseColorString(bgBuf, &bgColor)) {
        bgColor = TRANSPARENT_BG_AUTO;  /* Fallback to transparent */
    }

    SetPercentIconColors(textColor, bgColor);
}

/** Animation speed persistence (called from WriteConfig) */
void WriteAnimationSpeedToConfig(const char* config_path) {
    if (!config_path) return;
    
    const char* metricStr = "MEMORY";
    if (g_animSpeedMetric == ANIMATION_SPEED_CPU) metricStr = "CPU";
    else if (g_animSpeedMetric == ANIMATION_SPEED_TIMER) metricStr = "TIMER";
    WriteIniString("Animation", "ANIMATION_SPEED_METRIC", metricStr, config_path);
    
    /** Write breakpoints */
    if (g_animSpeedPointCount > 0) {
        WriteIniInt("Animation", "ANIMATION_SPEED_DEFAULT", (int)(g_animSpeedDefaultScalePercent + 0.5), config_path);
        for (int i = 0; i < g_animSpeedPointCount; ++i) {
            char key[64];
            snprintf(key, sizeof(key), "ANIMATION_SPEED_MAP_%d", g_animSpeedPoints[i].percent);
            char val[32];
            snprintf(val, sizeof(val), "%g", g_animSpeedPoints[i].scalePercent);
            WriteIniString("Animation", key, val, config_path);
        }
    } else {
        WriteIniInt("Animation", "ANIMATION_SPEED_DEFAULT", (int)(g_animSpeedDefaultScalePercent + 0.5), config_path);
    }
    
    WriteIniInt("Animation", "ANIMATION_MIN_INTERVAL_MS", g_animMinIntervalMs, config_path);

    /** Persist percent icon colors */
    COLORREF textColor = GetPercentIconTextColor();
    COLORREF bgColor = GetPercentIconBgColor();

    /* Write text color: "auto" if using transparent background (theme-aware) */
    if (bgColor == TRANSPARENT_BG_AUTO) {
        WriteIniString("Animation", "PERCENT_ICON_TEXT_COLOR", "auto", config_path);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(textColor), GetGValue(textColor), GetBValue(textColor));
        WriteIniString("Animation", "PERCENT_ICON_TEXT_COLOR", buf, config_path);
    }

    /* Write background color: "transparent" if using auto-transparent mode */
    if (bgColor == TRANSPARENT_BG_AUTO) {
        WriteIniString("Animation", "PERCENT_ICON_BG_COLOR", "transparent", config_path);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
        WriteIniString("Animation", "PERCENT_ICON_BG_COLOR", buf, config_path);
    }
}

