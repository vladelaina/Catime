/**
 * @file window_preview_policy.h
 * @brief Command policy for previews owned by modeless UI.
 */

#ifndef WINDOW_PREVIEW_POLICY_H
#define WINDOW_PREVIEW_POLICY_H

#include <windows.h>
#include "menu_preview.h"

BOOL WindowPreview_ShouldKeepForCommand(UINT command,
                                        PreviewSource source);

#endif /* WINDOW_PREVIEW_POLICY_H */
