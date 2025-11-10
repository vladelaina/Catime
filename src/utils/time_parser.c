/**
 * @file time_parser.c
 * @brief Unified time parsing and formatting implementation
 */

#include "utils/time_parser.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Skip whitespace characters
 * @param pos Pointer to current position
 * @return Updated position after skipping whitespace
 */
static const char* SkipWhitespace(const char* pos) {
    while (*pos == ' ' || *pos == '\t') {
        pos++;
    }
    return pos;
}

/**
 * @brief Parse integer from string and advance position
 * @param pos Pointer to current position (will be updated)
 * @return Parsed integer value, or -1 on overflow
 */
static int ParseNumber(const char** pos) {
    long long value = 0;
    while (isdigit((unsigned char)**pos)) {
        value = value * 10 + (**pos - '0');
        if (value > INT_MAX) {
            return -1;
        }
        (*pos)++;
    }
    return (int)value;
}

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

BOOL TimeParser_Validate(const char* input) {
    if (!input || !*input) return FALSE;

    int len = strlen(input);
    int digit_count = 0;

    for (int i = 0; i < len; i++) {
        char c = tolower((unsigned char)input[i]);
        if (isdigit(input[i])) {
            digit_count++;
        } else if (c == ' ' || c == '\t') {
            continue;
        } else if (c == 'h' || c == 'm' || c == 's' || c == 't') {
            /* Time units are allowed anywhere in the string */
            continue;
        } else {
            /* Invalid character */
            return FALSE;
        }
    }

    return digit_count > 0;
}

BOOL TimeParser_HasUnits(const char* input) {
    if (!input) return FALSE;

    for (const char* p = input; *p; p++) {
        char c = tolower((unsigned char)*p);
        if (c == 'h' || c == 'm' || c == 's') {
            return TRUE;
        }
    }
    return FALSE;
}

/* ============================================================================
 * Core Parsing Functions
 * ============================================================================ */

/**
 * @brief Time component structure for intelligent unit inference
 */
typedef struct {
    int value;
    char unit;  /* 'h', 'm', 's', or '\0' for unspecified */
} TimeComponent;

/**
 * @brief Get higher-level unit for smart inference
 * @param unit Current unit ('s' or 'm')
 * @return Higher-level unit ('m' for 's', 'h' for 'm', '\0' otherwise)
 */
static char GetHigherUnit(char unit) {
    switch (unit) {
        case 's': return 'm';
        case 'm': return 'h';
        default: return '\0';
    }
}

/**
 * @brief Infer units for components with smart detection
 * @param components Array of time components
 * @param count Number of components
 *
 * @details Rules:
 * - If component has explicit unit, keep it
 * - If component has no unit but next component has unit X:
 *   Use higher-level unit than X (e.g., if next is 'm', this is 'h')
 * - If all components have no units, apply positional rules:
 *   1 component: m
 *   2 components: m, s
 *   3 components: h, m, s
 */
static void InferUnits(TimeComponent* components, int count) {
    BOOL has_any_unit = FALSE;
    for (int i = 0; i < count; i++) {
        if (components[i].unit != '\0') {
            has_any_unit = TRUE;
            break;
        }
    }

    if (!has_any_unit) {
        /* All components have no units - use positional inference */
        if (count == 1) {
            components[0].unit = 'm';
        } else if (count == 2) {
            components[0].unit = 'm';
            components[1].unit = 's';
        } else if (count == 3) {
            components[0].unit = 'h';
            components[1].unit = 'm';
            components[2].unit = 's';
        }
        return;
    }

    /* Mixed format: infer missing units based on context */
    for (int i = 0; i < count; i++) {
        if (components[i].unit == '\0') {
            /* Look ahead to find next explicit unit */
            char next_unit = '\0';
            for (int j = i + 1; j < count; j++) {
                if (components[j].unit != '\0') {
                    next_unit = components[j].unit;
                    break;
                }
            }

            if (next_unit != '\0') {
                /* Use higher-level unit than the next component */
                components[i].unit = GetHigherUnit(next_unit);
                if (components[i].unit == '\0') {
                    /* No higher unit available, default to 'h' */
                    components[i].unit = 'h';
                }
            } else {
                /* No explicit unit found after this, default to minutes */
                components[i].unit = 'm';
            }
        }
    }
}

BOOL TimeParser_ParseBasic(const char* input, int* seconds) {
    if (!input || !seconds || !TimeParser_Validate(input)) {
        return FALSE;
    }

    *seconds = 0;

    char buffer[256];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* First pass: extract all components */
    TimeComponent components[10];
    int comp_count = 0;
    const char* pos = buffer;

    while (*pos && comp_count < 10) {
        pos = SkipWhitespace(pos);
        if (*pos == '\0') break;

        if (!isdigit((unsigned char)*pos)) {
            return FALSE;
        }

        int value = ParseNumber(&pos);
        if (value < 0) {
            return FALSE;
        }

        pos = SkipWhitespace(pos);

        char unit = '\0';
        char c = tolower((unsigned char)*pos);
        if (c == 'h' || c == 'm' || c == 's') {
            unit = c;
            pos++;
        }

        components[comp_count].value = value;
        components[comp_count].unit = unit;
        comp_count++;
    }

    if (comp_count == 0) {
        return FALSE;
    }

    /* Second pass: infer missing units */
    InferUnits(components, comp_count);

    /* Third pass: calculate total */
    int total = 0;
    for (int i = 0; i < comp_count; i++) {
        int multiplier = TimeParser_GetUnitMultiplier(components[i].unit);
        if (multiplier == 0) {
            return FALSE;
        }

        long long new_total = (long long)total + (long long)components[i].value * multiplier;
        if (new_total > INT_MAX) {
            return FALSE;
        }
        total = (int)new_total;
    }

    if (total <= 0) {
        return FALSE;
    }

    *seconds = total;
    return TRUE;
}

BOOL TimeParser_ParseAdvanced(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;

    /* Validate input first */
    if (!TimeParser_Validate(input)) {
        return FALSE;
    }

    /* Make a mutable copy */
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    int len = strlen(input_copy);
    int result = 0;

    /* Check for absolute time format (ends with 't' or 'T') */
    if (len > 0 && (input_copy[len - 1] == 't' || input_copy[len - 1] == 'T')) {
        input_copy[len - 1] = '\0';
        /* For now, delegate to ParseBasic as we don't implement absolute time in core */
        /* This can be extended in the future if needed */
        return FALSE;
    }

    /* Check if input has time units */
    if (TimeParser_HasUnits(input_copy)) {
        return TimeParser_ParseBasic(input_copy, seconds);
    }

    /* Parse numeric shorthand: "25" (minutes), "130 20" (130m 20s), "1 30 15" (1h 30m 15s) */
    char* parts[3];
    int part_count = 0;

    char* token = strtok(input_copy, " ");
    while (token && part_count < 3) {
        parts[part_count++] = token;
        token = strtok(NULL, " ");
    }

    /* Check for extra tokens */
    if (token != NULL) {
        return FALSE;
    }

    if (part_count == 1) {
        char* endptr;
        long val = strtol(parts[0], &endptr, 10);
        if (*endptr != '\0' || val <= 0 || val > INT_MAX / SECONDS_PER_MINUTE) {
            return FALSE;
        }
        result = (int)val * SECONDS_PER_MINUTE;
    } else if (part_count == 2) {
        char* endptr;
        long mins = strtol(parts[0], &endptr, 10);
        if (*endptr != '\0' || mins < 0 || mins > INT_MAX / SECONDS_PER_MINUTE) {
            return FALSE;
        }
        long secs = strtol(parts[1], &endptr, 10);
        if (*endptr != '\0' || secs < 0 || secs >= 60) {
            return FALSE;
        }
        result = (int)mins * SECONDS_PER_MINUTE + (int)secs;
    } else if (part_count == 3) {
        char* endptr;
        long hours = strtol(parts[0], &endptr, 10);
        if (*endptr != '\0' || hours < 0 || hours > INT_MAX / SECONDS_PER_HOUR) {
            return FALSE;
        }
        long mins = strtol(parts[1], &endptr, 10);
        if (*endptr != '\0' || mins < 0 || mins >= 60) {
            return FALSE;
        }
        long secs = strtol(parts[2], &endptr, 10);
        if (*endptr != '\0' || secs < 0 || secs >= 60) {
            return FALSE;
        }
        result = (int)hours * SECONDS_PER_HOUR + (int)mins * SECONDS_PER_MINUTE + (int)secs;
    } else {
        return FALSE;
    }

    if (result <= 0 || result > INT_MAX) {
        return FALSE;
    }

    *seconds = result;
    return TRUE;
}

/* ============================================================================
 * Formatting Functions
 * ============================================================================ */

void TimeParser_FormatToString(int seconds, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;

    if (seconds < 0) {
        snprintf(buffer, bufferSize, "0s");
        return;
    }

    int hours = seconds / SECONDS_PER_HOUR;
    int minutes = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int secs = seconds % SECONDS_PER_MINUTE;

    if (hours > 0 && minutes > 0 && secs > 0) {
        snprintf(buffer, bufferSize, "%dh%dm%ds", hours, minutes, secs);
    } else if (hours > 0 && minutes > 0) {
        snprintf(buffer, bufferSize, "%dh%dm", hours, minutes);
    } else if (hours > 0 && secs > 0) {
        snprintf(buffer, bufferSize, "%dh%ds", hours, secs);
    } else if (minutes > 0 && secs > 0) {
        snprintf(buffer, bufferSize, "%dm%ds", minutes, secs);
    } else if (hours > 0) {
        snprintf(buffer, bufferSize, "%dh", hours);
    } else if (minutes > 0) {
        snprintf(buffer, bufferSize, "%dm", minutes);
    } else {
        snprintf(buffer, bufferSize, "%ds", secs);
    }
}

void TimeParser_FormatToHMS(int seconds, int* hours, int* mins, int* secs) {
    if (hours) *hours = seconds / SECONDS_PER_HOUR;
    if (mins) *mins = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    if (secs) *secs = seconds % SECONDS_PER_MINUTE;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int TimeParser_GetUnitMultiplier(char unit) {
    char c = tolower((unsigned char)unit);
    switch (c) {
        case 'h': return SECONDS_PER_HOUR;
        case 'm': return SECONDS_PER_MINUTE;
        case 's': return 1;
        default: return 0;
    }
}
