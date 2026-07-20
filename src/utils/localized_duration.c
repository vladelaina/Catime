/**
 * @file localized_duration.c
 * @brief Locale-aware human-readable duration formatting.
 */

#include "utils/localized_duration.h"
#include "utils/time_parser.h"
#include "language.h"
#include <strsafe.h>
#include <wchar.h>

typedef enum {
    DURATION_PLURAL_ONE,
    DURATION_PLURAL_FEW,
    DURATION_PLURAL_OTHER
} DurationPluralCategory;

typedef enum {
    DURATION_UNIT_HOUR,
    DURATION_UNIT_MINUTE,
    DURATION_UNIT_SECOND
} DurationUnit;

static DurationPluralCategory DurationGetPluralCategory(
    AppLanguage language, int value) {
    if (language == APP_LANG_RUSSIAN) {
        int lastDigit = value % 10;
        int lastTwoDigits = value % 100;
        if (lastDigit == 1 && lastTwoDigits != 11) {
            return DURATION_PLURAL_ONE;
        }
        if (lastDigit >= 2 && lastDigit <= 4 &&
            (lastTwoDigits < 12 || lastTwoDigits > 14)) {
            return DURATION_PLURAL_FEW;
        }
        return DURATION_PLURAL_OTHER;
    }

    return value == 1 ? DURATION_PLURAL_ONE : DURATION_PLURAL_OTHER;
}

static const wchar_t* DurationResolvePattern(const wchar_t* key,
                                             const wchar_t* fallback) {
    const wchar_t* pattern = GetLocalizedString(NULL, key);
    if (!pattern || !pattern[0] || wcscmp(pattern, key) == 0) {
        return fallback;
    }
    return pattern;
}

static const wchar_t* DurationGetPattern(DurationUnit unit,
                                         DurationPluralCategory category) {
    switch (unit) {
        case DURATION_UNIT_HOUR:
            if (category == DURATION_PLURAL_ONE) {
                return DurationResolvePattern(L"DurationHourOne",
                                              L"%d hour");
            }
            if (category == DURATION_PLURAL_FEW) {
                return DurationResolvePattern(L"DurationHourFew",
                                              L"%d hours");
            }
            return DurationResolvePattern(L"DurationHourOther",
                                          L"%d hours");

        case DURATION_UNIT_MINUTE:
            if (category == DURATION_PLURAL_ONE) {
                return DurationResolvePattern(L"DurationMinuteOne",
                                              L"%d minute");
            }
            if (category == DURATION_PLURAL_FEW) {
                return DurationResolvePattern(L"DurationMinuteFew",
                                              L"%d minutes");
            }
            return DurationResolvePattern(L"DurationMinuteOther",
                                          L"%d minutes");

        case DURATION_UNIT_SECOND:
        default:
            if (category == DURATION_PLURAL_ONE) {
                return DurationResolvePattern(L"DurationSecondOne",
                                              L"%d second");
            }
            if (category == DURATION_PLURAL_FEW) {
                return DurationResolvePattern(L"DurationSecondFew",
                                              L"%d seconds");
            }
            return DurationResolvePattern(L"DurationSecondOther",
                                          L"%d seconds");
    }
}

static BOOL DurationUsesCompactSpacing(AppLanguage language) {
    return language == APP_LANG_CHINESE_SIMP ||
           language == APP_LANG_CHINESE_TRAD ||
           language == APP_LANG_JAPANESE;
}

static BOOL DurationAppendComponent(wchar_t* buffer, size_t bufferCount,
                                    BOOL compactSpacing, BOOL* hasComponent,
                                    DurationUnit unit, int value,
                                    AppLanguage language) {
    if (*hasComponent && !compactSpacing &&
        FAILED(StringCchCatW(buffer, bufferCount, L" "))) {
        return FALSE;
    }

    wchar_t component[64] = {0};
    const wchar_t* pattern = DurationGetPattern(
        unit, DurationGetPluralCategory(language, value));
    if (FAILED(StringCchPrintfW(component, _countof(component),
                               pattern, value)) ||
        FAILED(StringCchCatW(buffer, bufferCount, component))) {
        return FALSE;
    }

    *hasComponent = TRUE;
    return TRUE;
}

BOOL LocalizedDuration_Format(int totalSeconds, wchar_t* buffer,
                              size_t bufferCount) {
    if (!buffer || bufferCount == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }

    int hours = totalSeconds / SECONDS_PER_HOUR;
    int minutes = (totalSeconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int seconds = totalSeconds % SECONDS_PER_MINUTE;
    AppLanguage language = GetCurrentLanguage();
    BOOL compactSpacing = DurationUsesCompactSpacing(language);
    BOOL hasComponent = FALSE;

    if (hours > 0 &&
        !DurationAppendComponent(buffer, bufferCount, compactSpacing,
                                 &hasComponent, DURATION_UNIT_HOUR,
                                 hours, language)) {
        return FALSE;
    }
    if (minutes > 0 &&
        !DurationAppendComponent(buffer, bufferCount, compactSpacing,
                                 &hasComponent, DURATION_UNIT_MINUTE,
                                 minutes, language)) {
        return FALSE;
    }
    if ((seconds > 0 || !hasComponent) &&
        !DurationAppendComponent(buffer, bufferCount, compactSpacing,
                                 &hasComponent, DURATION_UNIT_SECOND,
                                 seconds, language)) {
        return FALSE;
    }

    return TRUE;
}
