/**
 * @file markdown_inline_tag_count.c
 * @brief Pre-counting for color and font tags.
 */

#include "markdown_inline_internal.h"

int CountMarkdownColorTags(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (wcsncmp(p, L"<color:", 7) == 0) {
            const wchar_t* end = wcsstr(p, L"</color>");
            if (!end) {
                break;
            }
            MarkdownInline_IncrementCountSaturated(&count);
            p = end + 8;
            continue;
        }
        p++;
    }

    return count;
}

int CountMarkdownFontTags(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (wcsncmp(p, L"<font:", 6) == 0) {
            const wchar_t* end = wcsstr(p, L"</font>");
            if (!end) {
                break;
            }
            MarkdownInline_IncrementCountSaturated(&count);
            p = end + 7;
            continue;
        }
        p++;
    }

    return count;
}
