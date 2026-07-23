/**
 * @file markdown_inline_count.c
 * @brief Pre-counting for links, headings, styles, and blocks.
 */

#include "markdown_inline_internal.h"

void MarkdownInline_IncrementCountSaturated(int* count) {
    if (count && *count < INT_MAX) {
        (*count)++;
    }
}

/* ============================================================================
 * Pre-count Functions (for efficient allocation)
 * ============================================================================ */

int CountMarkdownLinks(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'[') {
            const wchar_t* textEnd = wcschr(p + 1, L']');
            if (!textEnd) {
                break;
            }
            if (textEnd[1] != L'(') {
                p = textEnd + 1;
                continue;
            }
            const wchar_t* parenEnd = wcschr(textEnd + 2, L')');
            if (!parenEnd) {
                break;
            }
            MarkdownInline_IncrementCountSaturated(&count);
            p = parenEnd + 1;
            continue;
        }
        p++;
    }

    return count;
}

int CountMarkdownHeadings(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'#') {
            const wchar_t* hashEnd = p;
            while (*hashEnd == L'#' && (hashEnd - p) < 6) {
                hashEnd++;
            }
            if (*hashEnd == L' ' && (hashEnd - p) >= 1 && (hashEnd - p) <= 6) {
                MarkdownInline_IncrementCountSaturated(&count);
            }
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

int CountMarkdownStyles(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'`') {
            p++;
            BOOL hasContent = FALSE;
            while (*p && *p != L'`') {
                hasContent = TRUE;
                p++;
            }
            if (*p == L'`' && hasContent) {
                MarkdownInline_IncrementCountSaturated(&count);
            }
        } else if (*p == L'*' || *p == L'_') {
            wchar_t marker = *p;
            int markerCount = 0;
            while (*p == marker && markerCount < 3) {
                markerCount++;
                p++;
            }
            if (markerCount > 0 && *p != L' ' && *p != L'\0') {
                MarkdownInline_IncrementCountSaturated(&count);
            }
            continue;
        }
        p++;
    }

    return count;
}

int CountMarkdownListItems(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart) {
            // Skip leading spaces
            while (*p == L' ') p++;
            // Unordered: -, *, +
            if ((*p == L'-' || *p == L'*' || *p == L'+') && *(p + 1) == L' ') {
                MarkdownInline_IncrementCountSaturated(&count);
            }
            // Ordered: digit(s) + '.' + ' '
            else {
                const wchar_t* numCheck = p;
                while (*numCheck >= L'0' && *numCheck <= L'9') numCheck++;
                if (numCheck > p && *numCheck == L'.' && *(numCheck + 1) == L' ') {
                    MarkdownInline_IncrementCountSaturated(&count);
                }
            }
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}

int CountMarkdownBlockquotes(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;
    BOOL atLineStart = TRUE;

    while (*p) {
        if (atLineStart && *p == L'>') {
            MarkdownInline_IncrementCountSaturated(&count);
        }
        atLineStart = (*p == L'\n' || *p == L'\r');
        p++;
    }

    return count;
}
