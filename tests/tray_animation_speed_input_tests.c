#include "tray/tray_animation_speed_input.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

static void ExpectParsed(const wchar_t* input, double expected) {
    double actual = -1.0;
    if (!TryParseFixedAnimationSpeed(input, &actual) ||
        fabs(actual - expected) > 0.0000001) {
        fwprintf(stderr, L"expected '%ls' to parse as %.10g, got %.10g\n",
                 input, expected, actual);
        g_failures++;
    }
}

static void ExpectRejected(const wchar_t* input) {
    double actual = -1.0;
    if (TryParseFixedAnimationSpeed(input, &actual)) {
        fwprintf(stderr, L"expected '%ls' to be rejected, got %.10g\n",
                 input, actual);
        g_failures++;
    }
}

int main(void) {
    ExpectParsed(L"2", 2.0);
    ExpectParsed(L"1.2", 1.2);
    ExpectParsed(L"1\u30022", 1.2);
    ExpectParsed(L"\uff11\uff0e\uff12", 1.2);
    ExpectParsed(L"1,2", 1.2);
    ExpectParsed(L"1\u00b72", 1.2);
    ExpectParsed(L"\u201c1.2\u201d", 1.2);
    ExpectParsed(L"2x", 2.0);
    ExpectParsed(L"2\u00d7", 2.0);
    ExpectParsed(L"2\u500d", 2.0);
    ExpectParsed(L"2\u500d\u901f", 2.0);
    ExpectParsed(L"200%", 2.0);
    ExpectParsed(L"\u3000\uff12\uff10\uff10\uff05\u3000", 2.0);
    ExpectParsed(L"3e1", 30.0);
    ExpectParsed(L"999999999", 30.0);
    ExpectParsed(L"1e9999", 30.0);

    ExpectRejected(L"");
    ExpectRejected(L"0.09");
    ExpectRejected(L"-2");
    ExpectRejected(L"nan");
    ExpectRejected(L"2xx");
    ExpectRejected(L"two");

    if (g_failures != 0) {
        fprintf(stderr, "%d fixed animation speed input test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
