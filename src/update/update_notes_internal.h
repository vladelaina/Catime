#ifndef UPDATE_NOTES_INTERNAL_H
#define UPDATE_NOTES_INTERNAL_H

#include "markdown/markdown_parser.h"
#include "update/update_internal.h"

extern wchar_t* g_notesDisplayText;
extern MarkdownLink* g_notesLinks;
extern int g_notesLinkCount;
extern MarkdownHeading* g_notesHeadings;
extern int g_notesHeadingCount;
extern MarkdownStyle* g_notesStyles;
extern int g_notesStyleCount;
extern MarkdownListItem* g_notesListItems;
extern int g_notesListItemCount;
extern MarkdownBlockquote* g_notesBlockquotes;
extern int g_notesBlockquoteCount;
extern MarkdownColorTag* g_notesColorTags;
extern int g_notesColorTagCount;
extern MarkdownFontTag* g_notesFontTags;
extern int g_notesFontTagCount;
extern int g_notesTextHeight;

BOOL UpdateNotes_Initialize(HWND dialog, const char* releaseNotes);
void UpdateNotes_Cleanup(HWND dialog);
void UpdateNotes_Recalculate(HWND dialog);
BOOL UpdateNotes_Paint(HWND dialog, const DRAWITEMSTRUCT* item);
void UpdateNotes_ReleasePaintBuffer(void);

#endif
