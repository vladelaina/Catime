/**
 * @file dialog_input_options.h
 * @brief Parsing and formatting for quick countdown preset inputs.
 */

#ifndef DIALOG_INPUT_OPTIONS_H
#define DIALOG_INPUT_OPTIONS_H

#include <windows.h>
#include <stddef.h>

#define QUICK_TIME_OPTIONS_MAX_INPUT_CHARS 2048
#define QUICK_TIME_OPTIONS_MAX_INPUT_BYTES \
    ((QUICK_TIME_OPTIONS_MAX_INPUT_CHARS * 4) + 1)

BOOL DialogInputOptions_BuildDisplay(char* dest, size_t destSize);
BOOL DialogInputOptions_ParseConfig(char* inputUtf8, char* options,
                                    size_t optionsSize, int* parsedSeconds,
                                    int* parsedCount);

#endif /* DIALOG_INPUT_OPTIONS_H */
