/**
 * @file color_feedback.h
 * @brief Reusable parsing state and inline rendering for color input feedback.
 */

#ifndef COLOR_FEEDBACK_H
#define COLOR_FEEDBACK_H

#include <windows.h>
#include "color/color_parser.h"
#include "color/gradient.h"

typedef enum {
    COLOR_FEEDBACK_EMPTY = 0,
    COLOR_FEEDBACK_INVALID,
    COLOR_FEEDBACK_SOLID,
    COLOR_FEEDBACK_GRADIENT
} ColorFeedbackKind;

typedef struct {
    ColorFeedbackKind kind;
    char normalized[COLOR_HEX_BUFFER];
    COLORREF solidColor;
    GradientInfoSnapshot gradient;
} ColorFeedbackResult;

/** Parse a user-entered color into the exact value used by configuration. */
void ColorFeedback_Evaluate(const char* input, ColorFeedbackResult* result);

/** Return TRUE when the result can be previewed and submitted. */
BOOL ColorFeedback_IsValid(const ColorFeedbackResult* result);

/** Draw a theme-aware inline swatch or localized validation message. */
void ColorFeedback_DrawInline(HWND hwndOwner, const DRAWITEMSTRUCT* item,
                              const ColorFeedbackResult* result,
                              const wchar_t* invalidText);

#endif /* COLOR_FEEDBACK_H */
