/**
 * @file tray_animation_speed_input.c
 * @brief User-input parsing for fixed tray animation speed
 */

#include "tray/tray_animation_speed_input.h"

#include "config.h"
#include "utils/finite_double.h"

#include <stdlib.h>
#include <wctype.h>

static BOOL NormalizeFixedAnimationSpeedInput(const wchar_t* input,
                                              wchar_t* normalized,
                                              size_t normalizedCount) {
    if (!input || !normalized || normalizedCount == 0) return FALSE;

    size_t outputIndex = 0;
    for (; *input != L'\0'; input++) {
        wchar_t ch = *input;
        if (ch >= L'\uff10' && ch <= L'\uff19') {
            ch = L'0' + (ch - L'\uff10');
        } else if (ch == L'\u3002' || ch == L'\uff0e' || ch == L'\u00b7' ||
                   ch == L',' || ch == L'\uff0c' || ch == L'\u066b') {
            ch = L'.';
        } else if (ch == L'\uff0b') {
            ch = L'+';
        } else if (ch == L'\uff0d' || ch == L'\u2212') {
            ch = L'-';
        } else if (ch == L'\uff58') {
            ch = L'x';
        } else if (ch == L'\uff38') {
            ch = L'X';
        } else if (ch == L'\u3000') {
            ch = L' ';
        } else if (ch == L'\"' || ch == L'\'' ||
                   ch == L'\u2018' || ch == L'\u2019' ||
                   ch == L'\u201c' || ch == L'\u201d' ||
                   ch == L'\u300c' || ch == L'\u300d') {
            ch = L' ';
        }

        if (outputIndex + 1 >= normalizedCount) return FALSE;
        normalized[outputIndex++] = ch;
    }
    normalized[outputIndex] = L'\0';
    return TRUE;
}

BOOL TryParseFixedAnimationSpeed(const wchar_t* input, double* multiplier) {
    if (!input || !multiplier) return FALSE;

    wchar_t normalized[64] = {0};
    if (!NormalizeFixedAnimationSpeedInput(input, normalized,
                                           sizeof(normalized) / sizeof(normalized[0]))) {
        return FALSE;
    }

    const wchar_t* cursor = normalized;
    while (iswspace(*cursor)) cursor++;
    wchar_t* end = NULL;
    double parsed = wcstod(cursor, &end);
    if (end == cursor || DoubleIsNaNStrict(parsed)) return FALSE;

    while (iswspace(*end)) end++;
    BOOL isPercent = FALSE;
    if (*end == L'%' || *end == L'\uff05') {
        isPercent = TRUE;
        end++;
    } else if (*end == L'x' || *end == L'X' ||
               *end == L'\u00d7' || *end == L'\u500d') {
        BOOL chineseMultiplier = (*end == L'\u500d');
        end++;
        if (chineseMultiplier && *end == L'\u901f') end++;
    }
    while (iswspace(*end)) end++;
    if (*end != L'\0') return FALSE;

    if (isPercent) parsed /= 100.0;
    if (parsed < ANIMATION_FIXED_SPEED_MIN_MULTIPLIER) return FALSE;
    if (!DoubleIsFiniteStrict(parsed) ||
        parsed > ANIMATION_FIXED_SPEED_MAX_MULTIPLIER) {
        parsed = ANIMATION_FIXED_SPEED_MAX_MULTIPLIER;
    }

    *multiplier = parsed;
    return TRUE;
}
