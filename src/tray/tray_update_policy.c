/**
 * @file tray_update_policy.c
 * @brief Pure policy for stable native tray tooltip presentation.
 */

#include "tray/tray_update_policy.h"

TrayPresentationUpdateMode TrayUpdatePolicy_Select(
    BOOL tooltipActive, BOOL hasTooltipUpdate) {
    if (tooltipActive) {
        return hasTooltipUpdate ? TRAY_UPDATE_TOOLTIP_ONLY
                                : TRAY_UPDATE_DEFER;
    }
    return hasTooltipUpdate ? TRAY_UPDATE_ICON_AND_TOOLTIP
                            : TRAY_UPDATE_ICON_ONLY;
}
