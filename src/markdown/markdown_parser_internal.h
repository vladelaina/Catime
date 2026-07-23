#ifndef MARKDOWN_PARSER_INTERNAL_H
#define MARKDOWN_PARSER_INTERNAL_H

#include "markdown/markdown_parser.h"

BOOL MarkdownParser_CalculateDisplayBufferCapacity(size_t textLen,
                                                   size_t* capacity);
BOOL ParseMarkdownRichText(const wchar_t* input, wchar_t** displayText,
                           MarkdownLink** links, int* linkCount,
                           MarkdownHeading** headings, int* headingCount,
                           MarkdownStyle** styles, int* styleCount,
                           MarkdownListItem** listItems, int* listItemCount,
                           MarkdownBlockquote** blockquotes, int* blockquoteCount,
                           MarkdownColorTag** colorTags, int* colorTagCount,
                           MarkdownFontTag** fontTags, int* fontTagCount);

#endif
