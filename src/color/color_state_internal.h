#ifndef COLOR_STATE_INTERNAL_H
#define COLOR_STATE_INTERNAL_H

#include "color/color_state.h"

void FreeColorOptionArray(PredefinedColor* options, size_t count);
BOOL BuildColorOptionsFromConfigValue(const char* colorOptions,
                                      PredefinedColor** outOptions,
                                      size_t* outCount);
BOOL BuildColorOptionsConfigValueFromArray(PredefinedColor* options,
                                           size_t count, char* outValue,
                                           size_t outSize);
BOOL NormalizeGradientConfigValue(const char* colorInput, char* outValue,
                                 size_t outSize);
char ToUpperHexDigit(char ch);

#endif
