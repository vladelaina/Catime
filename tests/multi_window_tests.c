#include <assert.h>
#include <stdio.h>

#include "multi_window.h"

int main(void) {
    assert(CATIME_MAX_TIMER_WINDOWS == 20);
    assert(MultiWindow_GetOffsetForWindowIndex(0) == 28);
    assert(MultiWindow_GetOffsetForWindowIndex(7) == 224);
    assert(MultiWindow_GetOffsetForWindowIndex(8) == 28);
    assert(MultiWindow_GetOffsetForWindowIndex(-1) == 28);
    puts("multi-window tests passed");
    return 0;
}
