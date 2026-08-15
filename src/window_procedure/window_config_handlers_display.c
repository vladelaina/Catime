/**
 * @file window_config_handlers_display.c
 * @brief Preserves process-local display state during config reloads.
 */

#include "window_procedure/window_config_handlers_internal.h"

LRESULT HandleAppDisplayChanged(HWND hwnd) {
    (void)hwnd;
    /* Display state is loaded once at startup. Explicit UI commands update
     * this process and persist the latest choice for future instances. */
    return 0;
}
