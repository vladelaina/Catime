/**
 * @file preview_display.h
 * @brief Temporary display support for live previews.
 *
 * Keeps the main window visible during preview interactions, and creates
 * current-time content when there is no active timer content to preview.
 */

#ifndef PREVIEW_DISPLAY_H
#define PREVIEW_DISPLAY_H

#include <windows.h>
#include <stddef.h>

/**
 * @brief Temporarily show hidden window for a preview interaction.
 * @param hwnd Window handle
 *
 * @details If the window is hidden or has no active content, this will:
 *          - Show the window temporarily
 *          - Show current time when there is no active timer content
 */
void ShowWindowForPreview(HWND hwnd);

/**
 * @brief Restore window visibility and timer content to pre-preview state.
 * @param hwnd Window handle
 */
void RestoreWindowVisibility(HWND hwnd);

/**
 * @brief Get preview time text when edit mode needs placeholder content.
 * @param outText Output buffer
 * @param bufferSize Buffer size in wide characters
 * @return TRUE if preview text was generated, FALSE if no preview needed
 */
BOOL GetPreviewTimeText(wchar_t* outText, size_t bufferSize);

#endif /* PREVIEW_DISPLAY_H */
