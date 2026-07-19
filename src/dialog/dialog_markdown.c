/**
 * @file dialog_markdown.c
 * @brief Lifetime-safe Markdown parsing and rendering for dialogs.
 */

#include "dialog/dialog_markdown.h"
#include "markdown/markdown_parser.h"
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#define DIALOG_MARKDOWN_WRAPPER_EXTRA_CHARS 16

struct DialogMarkdownState {
    wchar_t* displayText;
    MarkdownLink* links;
    int linkCount;
    MarkdownHeading* headings;
    int headingCount;
    MarkdownStyle* styles;
    int styleCount;
    MarkdownListItem* listItems;
    int listItemCount;
    MarkdownBlockquote* blockquotes;
    int blockquoteCount;
    MarkdownColorTag* colorTags;
    int colorTagCount;
    MarkdownFontTag* fontTags;
    int fontTagCount;
};

static void DialogMarkdown_Clear(DialogMarkdownState* state) {
    if (!state) return;
    FreeMarkdownLinks(state->links, state->linkCount);
    free(state->headings);
    free(state->styles);
    free(state->listItems);
    free(state->blockquotes);
    free(state->colorTags);
    free(state->fontTags);
    free(state->displayText);
    ZeroMemory(state, sizeof(*state));
}

DialogMarkdownState* DialogMarkdown_Create(void) {
    return (DialogMarkdownState*)calloc(1, sizeof(DialogMarkdownState));
}

BOOL DialogMarkdown_Parse(DialogMarkdownState* state, const wchar_t* text,
                          BOOL wrapAsMarkdown) {
    if (!state || !text) return FALSE;
    DialogMarkdown_Clear(state);

    const wchar_t* input = text;
    wchar_t* wrapped = NULL;
    if (wrapAsMarkdown) {
        size_t textLength = wcslen(text);
        if (textLength > SIZE_MAX - DIALOG_MARKDOWN_WRAPPER_EXTRA_CHARS) {
            return FALSE;
        }
        size_t capacity = textLength + DIALOG_MARKDOWN_WRAPPER_EXTRA_CHARS;
        wrapped = (wchar_t*)calloc(capacity, sizeof(*wrapped));
        if (!wrapped) return FALSE;
        if (wcscpy_s(wrapped, capacity, L"<md>\n") != 0 ||
            wcscat_s(wrapped, capacity, text) != 0 ||
            wcscat_s(wrapped, capacity, L"\n</md>") != 0) {
            free(wrapped);
            return FALSE;
        }
        input = wrapped;
    }

    BOOL parsed = ParseMarkdownLinks(
        input, &state->displayText, &state->links, &state->linkCount,
        &state->headings, &state->headingCount,
        &state->styles, &state->styleCount,
        &state->listItems, &state->listItemCount,
        &state->blockquotes, &state->blockquoteCount,
        &state->colorTags, &state->colorTagCount,
        &state->fontTags, &state->fontTagCount);
    free(wrapped);
    if (!parsed) DialogMarkdown_Clear(state);
    return parsed;
}

BOOL DialogMarkdown_HandleClick(DialogMarkdownState* state, POINT point) {
    return state && HandleMarkdownClick(state->links, state->linkCount, point);
}

void DialogMarkdown_Render(DialogMarkdownState* state, HDC hdc, RECT rect,
                           COLORREF accentColor, COLORREF textColor) {
    if (!state || !state->displayText || !hdc) return;
    RenderMarkdownText(
        hdc, state->displayText, state->links, state->linkCount,
        state->headings, state->headingCount,
        state->styles, state->styleCount,
        state->listItems, state->listItemCount,
        state->blockquotes, state->blockquoteCount,
        rect, accentColor, textColor);
}

void DialogMarkdown_Destroy(DialogMarkdownState* state) {
    if (!state) return;
    DialogMarkdown_Clear(state);
    free(state);
}
