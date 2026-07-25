        BOOL isColorOrFontPreview = FALSE;
        BOOL isAnimationPreview = FALSE;
        BOOL isPreviewItem = IsPreviewMenuItem(menuItem, &isColorOrFontPreview,
                                               &isAnimationPreview);

        if (isAnimationPreview != g_lastWasAnimationPreview) {
            if (g_lastWasAnimationPreview && !isAnimationPreview) {
                CancelActiveMenuPreview(hwnd);
            }
        }

        if (isColorOrFontPreview != g_lastWasColorOrFontPreview) {
            if (g_lastWasColorOrFontPreview && !isColorOrFontPreview) {
                CancelActiveMenuPreview(hwnd);
            }
        }

        if (isColorOrFontPreview) {
            ShowWindowForPreview(hwnd);
        }

        g_lastWasColorOrFontPreview = isColorOrFontPreview;
        g_lastWasAnimationPreview = isAnimationPreview;

        if (isPreviewItem && menuItem != g_lastPreviewMenuItem) {
            g_lastPreviewMenuItem = menuItem;

            ClearPendingMenuPreview(hwnd);
            g_pendingAnimationPreviewItem = menuItem;
            StartAnimationPreviewDelayTimer(hwnd);
        } else if (!isPreviewItem) {
            ClearPendingMenuPreview(hwnd);
            g_lastPreviewMenuItem = menuItem;
        }
    } else {
        ClearPendingMenuPreview(hwnd);
        if (g_lastWasColorOrFontPreview) {
            CancelActiveMenuPreview(hwnd);
            g_lastWasColorOrFontPreview = FALSE;
        }
        if (g_lastWasAnimationPreview) {
            CancelActiveMenuPreview(hwnd);
            g_lastWasAnimationPreview = FALSE;
        }
        g_lastPreviewMenuItem = 0;
    }

    return 0;
}
