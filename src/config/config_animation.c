/**
 * @file config_animation.c
 * @brief Animation configuration management
 * 
 * Manages animation speed settings, percent icon colors, and animation-related configuration.
 */
#include "config.h"
#include "config/config_writer.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_percent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <wctype.h>
#ifdef _MSC_VER
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#include <windows.h>

typedef struct {
    int percent;
    double scalePercent;
} AnimSpeedPoint;

#define ANIM_SPEED_POINT_CAPACITY 128
#define ANIM_SPEED_SCALE_MIN_PERCENT 1.0
#define ANIM_SPEED_SCALE_MAX_PERCENT 1000.0

static AnimSpeedPoint g_animSpeedPoints[ANIM_SPEED_POINT_CAPACITY];
static int g_animSpeedPointCount = 0;
static double g_animSpeedDefaultScalePercent = 100.0;
static AnimationSpeedMetric g_animSpeedMetric = ANIMATION_SPEED_MEMORY;
static int g_animMinIntervalMs = 0;
static SRWLOCK g_animSpeedLock = SRWLOCK_INIT;

static int ClampAnimationBaseIntervalConfig(int intervalMs) {
    if (intervalMs <= 0) return (int)TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
    if (intervalMs < (int)TRAY_ANIMATION_MIN_INTERVAL_MS) return (int)TRAY_ANIMATION_MIN_INTERVAL_MS;
    if (intervalMs > (int)TRAY_ANIMATION_MAX_INTERVAL_MS) return (int)TRAY_ANIMATION_MAX_INTERVAL_MS;
    return intervalMs;
}

static int ClampAnimationMinIntervalConfig(int intervalMs) {
    if (intervalMs <= 0) return 0;
    return ClampAnimationBaseIntervalConfig(intervalMs);
}

static const char* AnimationSpeedMetricToString(AnimationSpeedMetric metric) {
    switch (metric) {
        case ANIMATION_SPEED_ORIGINAL: return "ORIGINAL";
        case ANIMATION_SPEED_CPU: return "CPU";
        case ANIMATION_SPEED_TIMER: return "TIMER";
        case ANIMATION_SPEED_MEMORY:
        default: return "MEMORY";
    }
}

static AnimationSpeedMetric NormalizeAnimationSpeedMetric(AnimationSpeedMetric metric) {
    switch (metric) {
        case ANIMATION_SPEED_ORIGINAL:
        case ANIMATION_SPEED_MEMORY:
        case ANIMATION_SPEED_CPU:
        case ANIMATION_SPEED_TIMER:
            return metric;
        default:
            return ANIMATION_SPEED_MEMORY;
    }
}

static int CmpAnimSpeedPoint(const void* a, const void* b) {
    const AnimSpeedPoint* pa = (const AnimSpeedPoint*)a;
    const AnimSpeedPoint* pb = (const AnimSpeedPoint*)b;
    if (pa->percent < pb->percent) return -1;
    if (pa->percent > pb->percent) return 1;
    return 0;
}

static BOOL AnimationSpeedPointsMatch(const AnimSpeedPoint* a,
                                      const AnimSpeedPoint* b,
                                      int count) {
    if (count < 0 || count > ANIM_SPEED_POINT_CAPACITY) return FALSE;

    for (int i = 0; i < count; i++) {
        if (a[i].percent != b[i].percent ||
            fabs(a[i].scalePercent - b[i].scalePercent) > 0.000001) {
            return FALSE;
        }
    }

    return TRUE;
}

static void TrimWideInPlace(wchar_t** value) {
    if (!value || !*value) return;
    while (iswspace(**value)) (*value)++;
    wchar_t* end = *value + wcslen(*value);
    while (end > *value && iswspace(end[-1])) {
        *--end = L'\0';
    }
}

static BOOL ParseAnimationPercentToken(const wchar_t* token, int* percent) {
    if (!token || !percent) return FALSE;

    while (iswspace(*token)) token++;
    if (*token == L'\0') return FALSE;

    errno = 0;
    wchar_t* end = NULL;
    long parsed = wcstol(token, &end, 10);
    if (end == token || errno == ERANGE || parsed < 0 || parsed > 100) {
        return FALSE;
    }

    while (end && iswspace(*end)) end++;
    if (end && *end != L'\0') return FALSE;

    *percent = (int)parsed;
    return TRUE;
}

static BOOL ParseAnimationScalePercent(wchar_t* value, double* scalePercent) {
    if (!value || !scalePercent) return FALSE;

    TrimWideInPlace(&value);
    size_t len = wcslen(value);
    if (len > 0 && value[len - 1] == L'%') {
        value[len - 1] = L'\0';
        TrimWideInPlace(&value);
    }
    if (*value == L'\0') return FALSE;

    errno = 0;
    wchar_t* end = NULL;
    double parsed = wcstod(value, &end);
    if (end == value || errno == ERANGE || !isfinite(parsed) ||
        parsed < ANIM_SPEED_SCALE_MIN_PERCENT ||
        parsed > ANIM_SPEED_SCALE_MAX_PERCENT) {
        return FALSE;
    }

    while (end && iswspace(*end)) end++;
    if (end && *end != L'\0') return FALSE;

    *scalePercent = parsed;
    return TRUE;
}

static void ParseAnimationSpeedFixedKeys(const char* configPathUtf8,
                                         AnimSpeedPoint* points,
                                         int* pointCount,
                                         double* defaultScalePercent) {
    if (!points || !pointCount || !defaultScalePercent) return;
    *pointCount = 0;
    *defaultScalePercent = 100.0;
    if (!configPathUtf8 || !*configPathUtf8) return;

    {
        int def = ReadIniInt("Animation", "ANIMATION_SPEED_DEFAULT", 100, configPathUtf8);
        if (def <= 0) def = 100;
        *defaultScalePercent = (double)def;
    }

    wchar_t wSection[64];
    wchar_t wfilePath[MAX_PATH];
    const char* sectionUtf8 = "Animation";
    if (MultiByteToWideChar(CP_UTF8, 0, sectionUtf8, -1, wSection, 64) == 0 ||
        MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wfilePath, MAX_PATH) == 0) {
        return;
    }

    enum {
        STACK_BUF_CHARS = 8192,
        HEAP_BUF_CHARS = 64 * 1024
    };
    wchar_t stackBuf[STACK_BUF_CHARS];
    wchar_t* wbuf = stackBuf;
    DWORD bufChars = STACK_BUF_CHARS;

    DWORD copied = GetPrivateProfileSectionW(wSection, wbuf, bufChars, wfilePath);
    if (copied >= bufChars - 2) {
        wbuf = (wchar_t*)malloc(sizeof(wchar_t) * HEAP_BUF_CHARS);
        if (!wbuf) return;
        bufChars = HEAP_BUF_CHARS;
        copied = GetPrivateProfileSectionW(wSection, wbuf, bufChars, wfilePath);
    }
    if (copied == 0) {
        if (wbuf != stackBuf) free(wbuf);
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

            if (wcsncmp(key, prefix, prefixLen) == 0) {
                const wchar_t* token = key + prefixLen;
                const wchar_t* dash = wcschr(token, L'-');
                if (!dash) {
                    /** New-style: ANIMATION_SPEED_MAP_PERCENT=VALUE */
                    wchar_t* value = eq + 1;
                    double scale = 0.0;
                    int percent = 0;
                    if (!ParseAnimationPercentToken(token, &percent) ||
                        !ParseAnimationScalePercent(value, &scale)) {
                        p += wcslen(p) + 1;
                        continue;
                    }
                    if (*pointCount < ANIM_SPEED_POINT_CAPACITY) {
                        points[*pointCount].percent = percent;
                        points[*pointCount].scalePercent = scale;
                        (*pointCount)++;
                    }
                }
            }
        }
        p += wcslen(p) + 1;
    }

    /** Sort breakpoints by percent ascending */
    if (*pointCount > 1) {
        qsort(points, *pointCount, sizeof(AnimSpeedPoint), CmpAnimSpeedPoint);
    }

    if (wbuf != stackBuf) free(wbuf);
}

static double InterpolateAnimationSpeedScale(double percent,
                                             const AnimSpeedPoint* points,
                                             int pointCount,
                                             double defaultScalePercent) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    /** New-style breakpoints with linear interpolation */
    if (pointCount > 0 && points) {
        /** If below first breakpoint, interpolate from default to first */
        if (percent <= (double)points[0].percent) {
            double p0 = 0.0;
            double s0 = defaultScalePercent;
            double p1 = (double)points[0].percent;
            double s1 = points[0].scalePercent;
            if (p1 <= p0) return s1;
            double t = (percent - p0) / (p1 - p0);
            return s0 + (s1 - s0) * t;
        }
        /** Find segment [i, i+1] that contains percent */
        for (int i = 0; i < pointCount - 1; ++i) {
            double p0 = (double)points[i].percent;
            double p1 = (double)points[i + 1].percent;
            if (percent >= p0 && percent <= p1) {
                double s0 = points[i].scalePercent;
                double s1 = points[i + 1].scalePercent;
                if (p1 <= p0) return s1;
                double t = (percent - p0) / (p1 - p0);
                return s0 + (s1 - s0) * t;
            }
        }
        /** Above last breakpoint: clamp to last scale */
        return points[pointCount - 1].scalePercent;
    }

    /** No breakpoints: return default */
    return defaultScalePercent;
}

AnimationSpeedMetric GetAnimationSpeedMetric(void) {
    AnimationSpeedMetric metric;
    AcquireSRWLockShared(&g_animSpeedLock);
    metric = g_animSpeedMetric;
    ReleaseSRWLockShared(&g_animSpeedLock);
    return metric;
}

BOOL WriteConfigAnimationSpeedMetric(AnimationSpeedMetric metric) {
    metric = NormalizeAnimationSpeedMetric(metric);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    const char* metricStr = AnimationSpeedMetricToString(metric);

    AnimationSpeedMetric currentMetric;
    AcquireSRWLockShared(&g_animSpeedLock);
    currentMetric = g_animSpeedMetric;
    ReleaseSRWLockShared(&g_animSpeedLock);

    char currentValue[32];
    ReadIniString("Animation", "ANIMATION_SPEED_METRIC", "__missing__",
                  currentValue, sizeof(currentValue), config_path);

    BOOL configMatches = (_stricmp(currentValue, metricStr) == 0);
    if (currentMetric == metric && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString("Animation", "ANIMATION_SPEED_METRIC", metricStr, config_path)) {
        return FALSE;
    }

    if (currentMetric != metric) {
        AcquireSRWLockExclusive(&g_animSpeedLock);
        g_animSpeedMetric = metric;
        ReleaseSRWLockExclusive(&g_animSpeedLock);
    }

    return TRUE;
}

double GetAnimationSpeedScaleForPercent(double percent) {
    AnimSpeedPoint points[ANIM_SPEED_POINT_CAPACITY] = {0};
    int pointCount = 0;
    double defaultScalePercent = 100.0;

    AcquireSRWLockShared(&g_animSpeedLock);
    pointCount = g_animSpeedPointCount;
    if (pointCount < 0) {
        pointCount = 0;
    } else if (pointCount > ANIM_SPEED_POINT_CAPACITY) {
        pointCount = ANIM_SPEED_POINT_CAPACITY;
    }
    defaultScalePercent = g_animSpeedDefaultScalePercent;
    if (pointCount > 0) {
        memcpy(points, g_animSpeedPoints, sizeof(points[0]) * (size_t)pointCount);
    }
    ReleaseSRWLockShared(&g_animSpeedLock);

    return InterpolateAnimationSpeedScale(percent, points, pointCount, defaultScalePercent);
}

void ReloadAnimationSpeedFromConfig(void) {
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, MAX_PATH);

    AnimationSpeedMetric newMetric;
    char metric[32] = {0};
    ReadIniString("Animation", "ANIMATION_SPEED_METRIC", "MEMORY", metric, sizeof(metric), config_path);
    if (_stricmp(metric, "ORIGINAL") == 0) {
        newMetric = ANIMATION_SPEED_ORIGINAL;
    } else if (_stricmp(metric, "CPU") == 0) {
        newMetric = ANIMATION_SPEED_CPU;
    } else if (_stricmp(metric, "TIMER") == 0 || _stricmp(metric, "COUNTDOWN") == 0) {
        newMetric = ANIMATION_SPEED_TIMER;
    } else {
        newMetric = ANIMATION_SPEED_MEMORY;
    }

    AnimSpeedPoint newPoints[ANIM_SPEED_POINT_CAPACITY] = {0};
    int newPointCount = 0;
    double newDefaultScalePercent = 100.0;
    ParseAnimationSpeedFixedKeys(config_path, newPoints, &newPointCount, &newDefaultScalePercent);

    /** Base animation playback speed */
    int folderInterval = ClampAnimationBaseIntervalConfig(
        ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS",
                   (int)TRAY_ANIMATION_DEFAULT_INTERVAL_MS, config_path));

    /** Optional minimum speed limit */
    int minInterval = ClampAnimationMinIntervalConfig(
        ReadIniInt("Animation", "ANIMATION_MIN_INTERVAL_MS", 0, config_path));

    AcquireSRWLockExclusive(&g_animSpeedLock);
    BOOL speedStateChanged =
        g_animSpeedMetric != newMetric ||
        g_animSpeedPointCount != newPointCount ||
        fabs(g_animSpeedDefaultScalePercent - newDefaultScalePercent) > 0.000001 ||
        g_animMinIntervalMs != minInterval ||
        !AnimationSpeedPointsMatch(g_animSpeedPoints, newPoints, newPointCount);

    if (speedStateChanged) {
        g_animSpeedMetric = newMetric;
        g_animSpeedPointCount = newPointCount;
        g_animSpeedDefaultScalePercent = newDefaultScalePercent;
        if (newPointCount > 0) {
            memcpy(g_animSpeedPoints, newPoints, sizeof(newPoints[0]) * (size_t)newPointCount);
        }
        g_animMinIntervalMs = minInterval;
    }
    ReleaseSRWLockExclusive(&g_animSpeedLock);

    TrayAnimation_SetBaseIntervalMs((UINT)folderInterval);
    TrayAnimation_SetMinIntervalMs((UINT)minInterval);
}

static int HexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static BOOL ParseHexByte(const char* s, int* out) {
    int hi = HexDigitValue(s[0]);
    int lo = HexDigitValue(s[1]);
    if (hi < 0 || lo < 0) return FALSE;
    *out = hi * 16 + lo;
    return TRUE;
}

static BOOL ParseRgbIntComponent(const char** cursor, int* out) {
    const char* p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(p, &end, 10);
    if (end == p || errno == ERANGE || parsed < 0 || parsed > 255) {
        return FALSE;
    }

    *cursor = end;
    *out = (int)parsed;
    return TRUE;
}

static BOOL ConsumeComma(const char** cursor) {
    const char* p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ',') return FALSE;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    *cursor = p;
    return TRUE;
}

static BOOL ParseColorString(const char* str, COLORREF* out) {
    if (!str || !out) return FALSE;
    if (str[0] == '#') {
        if (strlen(str) == 7) {
            int r = 0, g = 0, b = 0;
            if (!ParseHexByte(str + 1, &r) ||
                !ParseHexByte(str + 3, &g) ||
                !ParseHexByte(str + 5, &b)) {
                return FALSE;
            }
            *out = RGB(r, g, b);
            return TRUE;
        }
        return FALSE;
    }

    int r = 0, g = 0, b = 0;
    const char* p = str;
    if (ParseRgbIntComponent(&p, &r) &&
        ConsumeComma(&p) &&
        ParseRgbIntComponent(&p, &g) &&
        ConsumeComma(&p) &&
        ParseRgbIntComponent(&p, &b)) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '\0') return FALSE;
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

static BOOL AppendAnimationConfigItem(ConfigWriteItem* items, int itemCapacity, int* count,
                                      const char* key, const char* value) {
    if (!items || !count || !key || !value || itemCapacity <= 0 ||
        *count < 0 || *count >= itemCapacity) {
        return FALSE;
    }

    ConfigWriteItem* item = &items[*count];
    int sectionWritten = snprintf(item->section, sizeof(item->section), "%s", "Animation");
    int keyWritten = snprintf(item->key, sizeof(item->key), "%s", key);
    int valueWritten = snprintf(item->value, sizeof(item->value), "%s", value);
    if (sectionWritten < 0 || sectionWritten >= (int)sizeof(item->section) ||
        keyWritten < 0 || keyWritten >= (int)sizeof(item->key) ||
        valueWritten < 0 || valueWritten >= (int)sizeof(item->value)) {
        ZeroMemory(item, sizeof(*item));
        return FALSE;
    }

    (*count)++;
    return TRUE;
}

BOOL CollectAnimationSpeedConfigItems(ConfigWriteItem* items, int itemCapacity, int* count) {
    if (!items || !count || itemCapacity <= 0 || *count < 0) {
        return FALSE;
    }

    AnimationSpeedMetric metric;
    AnimSpeedPoint points[ANIM_SPEED_POINT_CAPACITY] = {0};
    int pointCount = 0;
    double defaultScalePercent = 100.0;
    int minIntervalMs = 0;

    AcquireSRWLockShared(&g_animSpeedLock);
    metric = g_animSpeedMetric;
    pointCount = g_animSpeedPointCount;
    if (pointCount < 0) {
        pointCount = 0;
    } else if (pointCount > ANIM_SPEED_POINT_CAPACITY) {
        pointCount = ANIM_SPEED_POINT_CAPACITY;
    }
    defaultScalePercent = g_animSpeedDefaultScalePercent;
    minIntervalMs = g_animMinIntervalMs;
    if (pointCount > 0) {
        memcpy(points, g_animSpeedPoints, sizeof(points[0]) * (size_t)pointCount);
    }
    ReleaseSRWLockShared(&g_animSpeedLock);

    if (!AppendAnimationConfigItem(items, itemCapacity, count,
                                   "ANIMATION_SPEED_METRIC",
                                   AnimationSpeedMetricToString(metric))) {
        return FALSE;
    }

    char defaultScaleValue[32];
    if (snprintf(defaultScaleValue, sizeof(defaultScaleValue), "%d",
                 (int)(defaultScalePercent + 0.5)) < 0 ||
        !AppendAnimationConfigItem(items, itemCapacity, count,
                                   "ANIMATION_SPEED_DEFAULT", defaultScaleValue)) {
        return FALSE;
    }

    for (int i = 0; i < pointCount; ++i) {
        char speedMapKey[64];
        char speedMapValue[32];
        if (snprintf(speedMapKey, sizeof(speedMapKey),
                     "ANIMATION_SPEED_MAP_%d", points[i].percent) < 0 ||
            snprintf(speedMapValue, sizeof(speedMapValue),
                     "%g", points[i].scalePercent) < 0 ||
            !AppendAnimationConfigItem(items, itemCapacity, count,
                                       speedMapKey, speedMapValue)) {
            return FALSE;
        }
    }

    char minIntervalValue[32];
    if (snprintf(minIntervalValue, sizeof(minIntervalValue), "%d", minIntervalMs) < 0 ||
        !AppendAnimationConfigItem(items, itemCapacity, count,
                                   "ANIMATION_MIN_INTERVAL_MS", minIntervalValue)) {
        return FALSE;
    }

    COLORREF textColor = GetPercentIconTextColor();
    COLORREF bgColor = GetPercentIconBgColor();

    char textColorValue[16];
    if (bgColor == TRANSPARENT_BG_AUTO) {
        if (snprintf(textColorValue, sizeof(textColorValue), "auto") < 0) {
            return FALSE;
        }
    } else if (snprintf(textColorValue, sizeof(textColorValue), "#%02X%02X%02X",
                        GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)) < 0) {
        return FALSE;
    }
    if (!AppendAnimationConfigItem(items, itemCapacity, count,
                                   "PERCENT_ICON_TEXT_COLOR", textColorValue)) {
        return FALSE;
    }

    char bgColorValue[16];
    if (bgColor == TRANSPARENT_BG_AUTO) {
        if (snprintf(bgColorValue, sizeof(bgColorValue), "transparent") < 0) {
            return FALSE;
        }
    } else if (snprintf(bgColorValue, sizeof(bgColorValue), "#%02X%02X%02X",
                        GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor)) < 0) {
        return FALSE;
    }

    return AppendAnimationConfigItem(items, itemCapacity, count,
                                     "PERCENT_ICON_BG_COLOR", bgColorValue);
}

/** Animation speed persistence (called from WriteConfig) */
void WriteAnimationSpeedToConfig(const char* config_path) {
    if (!config_path) return;

    const int itemCapacity = ANIM_SPEED_POINT_CAPACITY + 6;
    ConfigWriteItem* items = (ConfigWriteItem*)calloc((size_t)itemCapacity,
                                                      sizeof(ConfigWriteItem));
    if (!items) {
        return;
    }

    int count = 0;
    if (!CollectAnimationSpeedConfigItems(items, itemCapacity, &count)) {
        free(items);
        return;
    }

    WriteConfigItems(config_path, items, count);
    free(items);
}
