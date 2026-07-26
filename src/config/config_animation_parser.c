/**
 * @file config_animation_parser.c
 * @brief Strict parsing of animation speed configuration
 */
#include "config_animation_internal.h"

#include "log.h"
#include "tray/tray_animation_core.h"
#include "utils/finite_double.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static int ComparePoints(const void* first, const void* second) {
    const AnimationSpeedPoint* a = (const AnimationSpeedPoint*)first;
    const AnimationSpeedPoint* b = (const AnimationSpeedPoint*)second;
    return a->percent < b->percent ? -1 : a->percent > b->percent;
}

static void TrimWide(wchar_t** value) {
    wchar_t* end;

    if (!value || !*value) return;
    while (iswspace(**value)) ++(*value);
    end = *value + wcslen(*value);
    while (end > *value && iswspace(end[-1])) *--end = L'\0';
}

static BOOL ParsePercent(const wchar_t* value, int* percent) {
    wchar_t* end = NULL;
    long parsed;

    if (!value || !percent) return FALSE;
    while (iswspace(*value)) ++value;
    if (*value == L'\0') return FALSE;
    errno = 0;
    parsed = wcstol(value, &end, 10);
    if (end == value || errno == ERANGE || parsed < 0 || parsed > 100) {
        return FALSE;
    }
    while (end && iswspace(*end)) ++end;
    if (end && *end != L'\0') return FALSE;
    *percent = (int)parsed;
    return TRUE;
}

static BOOL ParseScale(wchar_t* value, double* scalePercent) {
    wchar_t* end = NULL;
    size_t length;
    double parsed;

    if (!value || !scalePercent) return FALSE;
    TrimWide(&value);
    length = wcslen(value);
    if (length > 0 && value[length - 1] == L'%') {
        value[length - 1] = L'\0';
        TrimWide(&value);
    }
    if (*value == L'\0') return FALSE;

    errno = 0;
    parsed = wcstod(value, &end);
    if (end == value || errno == ERANGE ||
        !DoubleIsFiniteStrict(parsed) ||
        parsed < ANIMATION_SPEED_SCALE_MIN_PERCENT ||
        parsed > ANIMATION_SPEED_SCALE_MAX_PERCENT) {
        return FALSE;
    }
    while (end && iswspace(*end)) ++end;
    if (end && *end != L'\0') return FALSE;
    *scalePercent = parsed;
    return TRUE;
}

static wchar_t* ReadAnimationSection(const char* configPath,
                                     wchar_t stackBuffer[],
                                     DWORD stackCapacity) {
    enum { HEAP_CAPACITY = 64 * 1024 };
    const wchar_t section[] = L"Animation";
    wchar_t path[MAX_PATH];
    wchar_t* buffer = stackBuffer;
    DWORD capacity = stackCapacity;
    DWORD copied;

    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, path,
                            _countof(path)) == 0) {
        return NULL;
    }
    copied = GetPrivateProfileSectionW(section, buffer, capacity, path);
    if (copied >= capacity - 2) {
        buffer = (wchar_t*)malloc(sizeof(wchar_t) * HEAP_CAPACITY);
        if (!buffer) return NULL;
        capacity = HEAP_CAPACITY;
        copied = GetPrivateProfileSectionW(section, buffer, capacity, path);
    }
    if (copied >= capacity - 2) {
        LOG_WARNING("Animation speed section is too large; ignoring its speed map");
        if (buffer != stackBuffer) free(buffer);
        return NULL;
    }
    return buffer;
}

static void ParseSpeedMap(const char* configPath,
                          AnimationSpeedSnapshot* snapshot) {
    enum { STACK_CAPACITY = 8192 };
    static const wchar_t prefix[] = L"ANIMATION_SPEED_MAP_";
    wchar_t stackBuffer[STACK_CAPACITY];
    wchar_t* buffer;

    snapshot->pointCount = 0;
    snapshot->defaultScalePercent = AnimationConfig_NormalizeScalePercent(
        (double)ReadIniInt("Animation", "ANIMATION_SPEED_DEFAULT", 100,
                           configPath),
        100.0);
    buffer = ReadAnimationSection(configPath, stackBuffer, STACK_CAPACITY);
    if (!buffer) return;

    for (wchar_t* entry = buffer; *entry;) {
        wchar_t* next = entry + wcslen(entry) + 1;
        wchar_t* separator = wcschr(entry, L'=');
        if (separator) {
            *separator = L'\0';
            if (wcsncmp(entry, prefix, _countof(prefix) - 1) == 0 &&
                !wcschr(entry + _countof(prefix) - 1, L'-')) {
                int percent;
                double scale;
                if (ParsePercent(entry + _countof(prefix) - 1, &percent) &&
                    ParseScale(separator + 1, &scale) &&
                    snapshot->pointCount < ANIMATION_SPEED_POINT_CAPACITY) {
                    AnimationSpeedPoint* point =
                        &snapshot->points[snapshot->pointCount++];
                    point->percent = percent;
                    point->scalePercent = scale;
                }
            }
        }
        entry = next;
    }
    if (snapshot->pointCount > 1) {
        qsort(snapshot->points, snapshot->pointCount,
              sizeof(snapshot->points[0]), ComparePoints);
    }
    if (buffer != stackBuffer) free(buffer);
}

static double ReadFixedScalePercent(const char* configPath) {
    char fallback[32];
    char value[64] = {0};
    char* end = NULL;
    double parsed;

    snprintf(fallback, sizeof(fallback), "%g",
             ANIMATION_FIXED_SPEED_DEFAULT_MULTIPLIER * 100.0);
    ReadIniString("Animation", "ANIMATION_FIXED_SPEED_PERCENT", fallback,
                  value, sizeof(value), configPath);
    errno = 0;
    parsed = strtod(value, &end);
    while (end && isspace((unsigned char)*end)) ++end;
    if (end == value || !end || *end != '\0' || errno == ERANGE) {
        return ANIMATION_FIXED_SPEED_DEFAULT_MULTIPLIER * 100.0;
    }
    return AnimationConfig_NormalizeFixedPercent(parsed);
}

BOOL AnimationConfig_LoadSnapshot(const char* configPath,
                                  AnimationSpeedSnapshot* snapshot) {
    char metric[32] = {0};

    if (!configPath || !snapshot) return FALSE;
    ZeroMemory(snapshot, sizeof(*snapshot));
    ReadIniString("Animation", "ANIMATION_SPEED_METRIC", "MEMORY",
                  metric, sizeof(metric), configPath);
    snapshot->metric = AnimationConfig_MetricFromString(metric);
    snapshot->fixedScalePercent = ReadFixedScalePercent(configPath);
    ParseSpeedMap(configPath, snapshot);
    snapshot->folderIntervalMs = AnimationConfig_ClampFolderInterval(
        ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS",
                   (int)TRAY_ANIMATION_DEFAULT_INTERVAL_MS, configPath));
    snapshot->minIntervalMs = AnimationConfig_ClampMinInterval(
        ReadIniInt("Animation", "ANIMATION_MIN_INTERVAL_MS", 0,
                   configPath));
    return TRUE;
}
