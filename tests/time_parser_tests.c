#include "utils/time_parser.h"

#include <stdio.h>

static int g_failures = 0;

static void ExpectBasic(const char* input, int expected) {
    int actual = -1;
    if (!TimeParser_ParseBasic(input, &actual) || actual != expected) {
        fprintf(stderr, "expected basic input '%s' to parse as %d, got %d\n",
                input, expected, actual);
        g_failures++;
    }
}

static void ExpectAdvanced(const char* input, int expected) {
    int actual = -1;
    if (!TimeParser_ParseAdvanced(input, &actual) || actual != expected) {
        fprintf(stderr,
                "expected advanced input '%s' to parse as %d, got %d\n",
                input, expected, actual);
        g_failures++;
    }
}

int main(void) {
    /* An omitted unit after minutes means seconds. */
    ExpectBasic("23m3", 23 * SECONDS_PER_MINUTE + 3);
    ExpectBasic("23m 3", 23 * SECONDS_PER_MINUTE + 3);
    ExpectBasic("23M3", 23 * SECONDS_PER_MINUTE + 3);

    /* An omitted unit after hours means minutes. */
    ExpectBasic("2h3", 2 * SECONDS_PER_HOUR + 3 * SECONDS_PER_MINUTE);
    ExpectBasic("2h 3", 2 * SECONDS_PER_HOUR + 3 * SECONDS_PER_MINUTE);

    /* Multiple omitted trailing units continue from hours to minutes/seconds. */
    ExpectBasic("1h2m3", SECONDS_PER_HOUR + 2 * SECONDS_PER_MINUTE + 3);
    ExpectBasic("1h2 3", SECONDS_PER_HOUR + 2 * SECONDS_PER_MINUTE + 3);

    /* Existing inference directions remain supported. */
    ExpectBasic("25 30m", 25 * SECONDS_PER_HOUR + 30 * SECONDS_PER_MINUTE);
    ExpectBasic("25 30", 25 * SECONDS_PER_MINUTE + 30);
    ExpectBasic("2h3s", 2 * SECONDS_PER_HOUR + 3);

    /* The main countdown parser delegates unit-bearing input to basic parsing. */
    ExpectAdvanced("23m3", 23 * SECONDS_PER_MINUTE + 3);
    ExpectAdvanced("2h3", 2 * SECONDS_PER_HOUR + 3 * SECONDS_PER_MINUTE);

    if (g_failures != 0) {
        fprintf(stderr, "%d time parser test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
