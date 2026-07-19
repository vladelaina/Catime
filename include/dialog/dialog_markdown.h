/**
 * @file dialog_markdown.h
 * @brief Window-owned Markdown content for modern dialogs.
 */

#ifndef DIALOG_MARKDOWN_H
#define DIALOG_MARKDOWN_H

#include <windows.h>

typedef struct DialogMarkdownState DialogMarkdownState;

DialogMarkdownState* DialogMarkdown_Create(void);
BOOL DialogMarkdown_Parse(DialogMarkdownState* state, const wchar_t* text,
                          BOOL wrapAsMarkdown);
BOOL DialogMarkdown_HandleClick(DialogMarkdownState* state, POINT point);
void DialogMarkdown_Render(DialogMarkdownState* state, HDC hdc, RECT rect,
                           COLORREF accentColor, COLORREF textColor);
void DialogMarkdown_Destroy(DialogMarkdownState* state);

#endif /* DIALOG_MARKDOWN_H */
