#include "utils/time_parser.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TIME_INPUT_CAPACITY 256
#define TIME_SHORTHAND_PARTS 3
#define TIME_TOKEN_DELIMITERS " \t"

static BOOL ParseLongPart(const char* part, long minimum, long maximum,
                          long* value) {
    char* end = NULL;
    long parsed = strtol(part, &end, 10);
    if (!end || *end != '\0' || parsed < minimum || parsed > maximum) {
        return FALSE;
    }
    *value = parsed;
    return TRUE;
}

static BOOL ParseShorthandParts(char* input, char** parts, int* count) {
    *count = 0;
    char* token = strtok(input, TIME_TOKEN_DELIMITERS);
    while (token && *count < TIME_SHORTHAND_PARTS) {
        parts[(*count)++] = token;
        token = strtok(NULL, TIME_TOKEN_DELIMITERS);
    }
    return token == NULL;
}

BOOL TimeParser_ParseAdvanced(const char* input, int* seconds) {
    if (!input || !seconds || !TimeParser_Validate(input)) return FALSE;

    char inputCopy[TIME_INPUT_CAPACITY];
    strncpy(inputCopy, input, sizeof(inputCopy) - 1);
    inputCopy[sizeof(inputCopy) - 1] = '\0';

    size_t length = strlen(inputCopy);
    if (length > 0 &&
        (inputCopy[length - 1] == 't' || inputCopy[length - 1] == 'T')) {
        return FALSE;
    }
    if (TimeParser_HasUnits(inputCopy)) {
        return TimeParser_ParseBasic(inputCopy, seconds);
    }

    char* parts[TIME_SHORTHAND_PARTS];
    int count = 0;
    if (!ParseShorthandParts(inputCopy, parts, &count)) return FALSE;

    long hours = 0;
    long minutes = 0;
    long remainingSeconds = 0;
    if (count == 1) {
        if (!ParseLongPart(parts[0], 1, INT_MAX / SECONDS_PER_MINUTE,
                           &minutes)) {
            return FALSE;
        }
    } else if (count == 2) {
        if (!ParseLongPart(parts[0], 0, INT_MAX / SECONDS_PER_MINUTE,
                           &minutes) ||
            !ParseLongPart(parts[1], 0, 59, &remainingSeconds)) {
            return FALSE;
        }
    } else if (count == 3) {
        if (!ParseLongPart(parts[0], 0, INT_MAX / SECONDS_PER_HOUR, &hours) ||
            !ParseLongPart(parts[1], 0, 59, &minutes) ||
            !ParseLongPart(parts[2], 0, 59, &remainingSeconds)) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    long long total = (long long)hours * SECONDS_PER_HOUR +
                      (long long)minutes * SECONDS_PER_MINUTE +
                      remainingSeconds;
    if (total <= 0 || total > INT_MAX) return FALSE;

    *seconds = (int)total;
    return TRUE;
}
