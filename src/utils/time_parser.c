/**
 * @file time_parser.c
 * @brief Unified time parsing and formatting implementation
 */

#include "utils/time_parser.h"
#include <ctype.h>
#include <string.h>
#include <limits.h>

#define TIME_PARSER_INPUT_BUFFER_SIZE 256
#define TIME_PARSER_MAX_COMPONENTS 10

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

static BOOL GetInputLengthWithinLimit(const char* input, size_t* length) {
    if (!input) {
        return FALSE;
    }

    for (size_t i = 0; i < TIME_PARSER_INPUT_BUFFER_SIZE; i++) {
        if (input[i] == '\0') {
            if (length) {
                *length = i;
            }
            return TRUE;
        }
    }

    return FALSE;
}

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

BOOL TimeParser_Validate(const char* input) {
    size_t len = 0;
    if (!GetInputLengthWithinLimit(input, &len) || len == 0) {
        return FALSE;
    }

    int digit_count = 0;

    for (size_t i = 0; i < len; i++) {
        int c = tolower((unsigned char)input[i]);
        if (isdigit((unsigned char)input[i])) {
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

    for (size_t i = 0; i < TIME_PARSER_INPUT_BUFFER_SIZE && input[i]; i++) {
        const char* p = input + i;
        int c = tolower((unsigned char)*p);
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
 * @brief Get lower-level unit for smart inference
 * @param unit Current unit ('h' or 'm')
 * @return Lower-level unit ('m' for 'h', 's' for 'm', '\0' otherwise)
 */
static char GetLowerUnit(char unit) {
    switch (unit) {
        case 'h': return 'm';
        case 'm': return 's';
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
 * - If there is no later explicit unit, infer one level below the previous
 *   component (e.g., "2h3" is 2 hours 3 minutes and "23m3" is
 *   23 minutes 3 seconds)
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
                /* Continue downward from the preceding component. */
                char previous_unit = i > 0 ? components[i - 1].unit : '\0';
                components[i].unit = GetLowerUnit(previous_unit);
                if (components[i].unit == '\0') {
                    /* Preserve the standalone/default unit behavior. */
                    components[i].unit = 'm';
                }
            }
        }
    }
}

BOOL TimeParser_ParseBasic(const char* input, int* seconds) {
    if (!input || !seconds || !TimeParser_Validate(input)) {
        return FALSE;
    }

    *seconds = 0;

    char buffer[TIME_PARSER_INPUT_BUFFER_SIZE];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* First pass: extract all components */
    TimeComponent components[TIME_PARSER_MAX_COMPONENTS];
    int comp_count = 0;
    const char* pos = buffer;

    while (*pos) {
        pos = SkipWhitespace(pos);
        if (*pos == '\0') break;

        if (comp_count >= TIME_PARSER_MAX_COMPONENTS) {
            return FALSE;
        }

        if (!isdigit((unsigned char)*pos)) {
            return FALSE;
        }

        int value = ParseNumber(&pos);
        if (value < 0) {
            return FALSE;
        }

        pos = SkipWhitespace(pos);

        char unit = '\0';
        int c = tolower((unsigned char)*pos);
        if (c == 'h' || c == 'm' || c == 's') {
            unit = (char)c;
            pos++;
        }

        components[comp_count].value = value;
        components[comp_count].unit = unit;
        comp_count++;
    }

    pos = SkipWhitespace(pos);
    if (*pos != '\0') {
        return FALSE;
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
