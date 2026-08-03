#include "tray/tray_animation_selection.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(const char* name, BOOL actual, BOOL expected) {
    if (!!actual != !!expected) {
        fprintf(stderr, "%s: expected %d, got %d\n",
                name, !!expected, !!actual);
        g_failures++;
    }
}

int main(void) {
    Expect("same animation without preview can be reused",
           TrayAnimationSelection_CanReuseCurrent(
               "walking.gif", "WALKING.GIF", FALSE, FALSE),
           TRUE);
    Expect("active preview requires commit",
           TrayAnimationSelection_CanReuseCurrent(
               "walking.gif", "walking.gif", TRUE, FALSE),
           FALSE);
    Expect("pending preview requires commit",
           TrayAnimationSelection_CanReuseCurrent(
               "walking.gif", "walking.gif", FALSE, TRUE),
           FALSE);
    Expect("different animation requires reload",
           TrayAnimationSelection_CanReuseCurrent(
               "walking.gif", "running.gif", FALSE, FALSE),
           FALSE);
    Expect("empty animation is not reusable",
           TrayAnimationSelection_CanReuseCurrent(
               "", "", FALSE, FALSE),
           FALSE);

    if (g_failures != 0) {
        fprintf(stderr, "%d tray animation selection test(s) failed\n",
                g_failures);
        return 1;
    }
    return 0;
}
