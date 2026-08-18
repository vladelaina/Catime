#include "tray/tray_update_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(TrayUpdatePolicy_Select(FALSE, FALSE) ==
           TRAY_UPDATE_ICON_ONLY);
    assert(TrayUpdatePolicy_Select(FALSE, TRUE) ==
           TRAY_UPDATE_ICON_AND_TOOLTIP);
    assert(TrayUpdatePolicy_Select(TRUE, FALSE) ==
           TRAY_UPDATE_DEFER);
    assert(TrayUpdatePolicy_Select(TRUE, TRUE) ==
           TRAY_UPDATE_TOOLTIP_ONLY);
    puts("tray update policy tests passed");
    return 0;
}
