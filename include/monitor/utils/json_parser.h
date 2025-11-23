#ifndef MONITOR_UTILS_JSON_PARSER_H
#define MONITOR_UTILS_JSON_PARSER_H

#include <windows.h>

/**
 * @brief Extract a 64-bit integer value from a simple JSON string
 * 
 * This is a lightweight parser that looks for "key": value pattern.
 * It is not a full JSON parser but sufficient for flat API responses.
 * 
 * @param json The JSON string to search
 * @param key The key to look for (should include quotes if strict matching is desired, e.g. "\"count\":")
 * @param outValue Pointer to store the result
 * @return TRUE if found and parsed successfully
 */
BOOL Json_ExtractInt64(const char* json, const char* key, long long* outValue);

#endif
