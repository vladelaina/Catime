/**
 * @file markdown_inline_internal.h
 * @brief Shared helpers for inline Markdown parser modules.
 */

#ifndef CATIME_MARKDOWN_INLINE_INTERNAL_H
#define CATIME_MARKDOWN_INLINE_INTERNAL_H

#include "markdown/markdown_parser.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MARKDOWN_LINK_TEXT_MAX_CHARS 2048
#define MARKDOWN_LINK_URL_MAX_CHARS 2048

void MarkdownInline_IncrementCountSaturated(int* count);
BOOL MarkdownInline_ExtractStyleBounded(const wchar_t** src,
                                        ParseState* state,
                                        const wchar_t* limit);
BOOL MarkdownInline_ExtractStrikethroughBounded(const wchar_t** src,
                                                ParseState* state,
                                                const wchar_t* limit);

#endif /* CATIME_MARKDOWN_INLINE_INTERNAL_H */
