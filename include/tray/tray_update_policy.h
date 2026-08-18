/**
 * @file tray_update_policy.h
 * @brief Pure policy for Shell tray icon and tooltip submissions.
 */

#ifndef CATIME_TRAY_UPDATE_POLICY_H
#define CATIME_TRAY_UPDATE_POLICY_H

#include <windows.h>

typedef enum {
    TRAY_UPDATE_DEFER = 0,
    TRAY_UPDATE_ICON_ONLY,
    TRAY_UPDATE_TOOLTIP_ONLY,
    TRAY_UPDATE_ICON_AND_TOOLTIP
} TrayPresentationUpdateMode;

TrayPresentationUpdateMode TrayUpdatePolicy_Select(
    BOOL tooltipActive, BOOL hasTooltipUpdate);

#endif
