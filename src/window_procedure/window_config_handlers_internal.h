/**
 * @file window_config_handlers_internal.h
 * @brief Shared helpers for configuration reload handler modules.
 */

#ifndef WINDOW_CONFIG_HANDLERS_INTERNAL_H
#define WINDOW_CONFIG_HANDLERS_INTERNAL_H

#include "window_procedure/window_config_handlers.h"

#include <stdbool.h>
#include <stddef.h>

BOOL WindowConfigInternal_ReadStringExact(const char* section, const char* key,
                                          const char* def, char* target,
                                          DWORD targetSize);
BOOL WindowConfigInternal_LoadAndCompareString(const char* section, const char* key,
                                               char* target, size_t size,
                                               const char* def);
BOOL WindowConfigInternal_LoadAndCompareBool(const char* section, const char* key,
                                             bool* target, bool def);
int WindowConfigInternal_NormalizeBaseFontSize(int fontSize);
int WindowConfigInternal_NormalizeDefaultStartTime(int seconds);
int WindowConfigInternal_ClampInt(const char* key, int value,
                                  int minValue, int maxValue);
void WindowConfigInternal_NormalizeTextColor(const char* color, char* output,
                                             size_t outputSize);
void WindowConfigInternal_NormalizeStartupMode(const char* mode, char* output,
                                               size_t outputSize);
BOOL WindowConfigInternal_ApplyFont(const char* configFont);
BOOL WindowConfigInternal_ParseQuickCountdownOptions(char* optionsStr,
                                                     int* parsedOptions,
                                                     int* parsedCount);
BOOL WindowConfigInternal_ParsePomodoroTimeOptions(char* optionsStr,
                                                   int* parsedOptions,
                                                   int* parsedCount);
BOOL WindowConfigInternal_ParseScaleFactor(const char* text, float* scale);
int WindowConfigInternal_ClampNotificationWidth(int width);
int WindowConfigInternal_ClampNotificationHeight(int height);

#endif /* WINDOW_CONFIG_HANDLERS_INTERNAL_H */
