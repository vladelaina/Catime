/**
 * @file window_preview_policy.c
 * @brief Command policy for previews owned by modeless UI.
 */

#include "window_procedure/window_preview_policy.h"
#include "../../resource/resource.h"

BOOL WindowPreview_ShouldKeepForCommand(UINT command,
                                        PreviewSource source) {
    return command == CLOCK_IDC_EDIT_MODE &&
           source == PREVIEW_SOURCE_FONT_PICKER;
}
