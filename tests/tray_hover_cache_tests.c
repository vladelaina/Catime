#include "tray/tray_hover_cache.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(const char* name, BOOL value) {
    if (!value) {
        fprintf(stderr, "%s\n", name);
        g_failures++;
    }
}

int main(void) {
    TrayHoverRectCache cache;
    TrayHoverRectCache_Reset(&cache);

    Expect("fresh cache should require a query",
           TrayHoverRectCache_NeedsRefresh(&cache, 1000, 250));

    RECT rect = {10, 20, 30, 40};
    POINT inside = {15, 25};
    POINT outside = {40, 50};
    TrayHoverRectCache_RecordQuery(&cache, TRUE, &rect, 1000, 2000);
    Expect("successful query did not become valid", cache.valid);
    Expect("inside point was rejected",
           TrayHoverRectCache_Contains(&cache, inside));
    Expect("outside point was accepted",
           !TrayHoverRectCache_Contains(&cache, outside));
    Expect("cache refreshed too early",
           !TrayHoverRectCache_NeedsRefresh(&cache, 1250, 250));
    Expect("cache did not expire on schedule",
           TrayHoverRectCache_NeedsRefresh(&cache, 1251, 250));

    TrayHoverRectCache_RecordQuery(&cache, FALSE, NULL, 2500, 2000);
    Expect("transient failure invalidated recent geometry", cache.valid);
    Expect("transient failure lost containment",
           TrayHoverRectCache_Contains(&cache, inside));

    RECT emptyRect = {0};
    TrayHoverRectCache_RecordQuery(&cache, TRUE, &emptyRect, 2600, 2000);
    Expect("empty transient result invalidated recent geometry", cache.valid);

    TrayHoverRectCache_RecordQuery(&cache, FALSE, NULL, 3001, 2000);
    Expect("stale geometry survived beyond the grace period", !cache.valid);

    TrayHoverRectCache_RecordQuery(&cache, TRUE, &rect, 0xFFFFFF00u, 2000);
    TrayHoverRectCache_RecordQuery(&cache, FALSE, NULL, 0x00000100u, 2000);
    Expect("tick-count wrap invalidated recent geometry", cache.valid);

    TrayHoverRectCache_Reset(&cache);
    TrayHoverRectCache_RecordQuery(&cache, FALSE, NULL, 5000, 2000);
    Expect("failed first query created a valid cache", !cache.valid);

    if (g_failures != 0) {
        fprintf(stderr, "%d tray hover cache test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
