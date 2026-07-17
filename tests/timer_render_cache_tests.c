#include "timer/timer_render_cache.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(const char* name, BOOL value) {
    if (!value) {
        fprintf(stderr, "%s\n", name);
        g_failures++;
    }
}

int main(void) {
    wchar_t lastPainted[32] = {0};
    BOOL hasLastPainted = FALSE;

    Expect("fresh cache should request the first frame",
           TimerRenderCache_NeedsRepaint(lastPainted, hasLastPainted, L"9:38"));

    TimerRenderCache_CommitPaint(lastPainted, _countof(lastPainted),
                                 &hasLastPainted, L"9:38");
    Expect("committed frame should not repaint again",
           !TimerRenderCache_NeedsRepaint(lastPainted, hasLastPainted, L"9:38"));

    Expect("minute transition should request repaint",
           TimerRenderCache_NeedsRepaint(lastPainted, hasLastPainted, L"9:39"));

    /* Simulate a skipped or failed paint: no commit occurs. */
    Expect("failed minute-transition paint must retry on the next tick",
           TimerRenderCache_NeedsRepaint(lastPainted, hasLastPainted, L"9:39"));

    TimerRenderCache_CommitPaint(lastPainted, _countof(lastPainted),
                                 &hasLastPainted, L"9:39");
    Expect("successful retry should settle the cache",
           !TimerRenderCache_NeedsRepaint(lastPainted, hasLastPainted, L"9:39"));

    if (g_failures != 0) {
        fprintf(stderr, "%d timer render cache test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
