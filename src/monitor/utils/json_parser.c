#include "monitor/utils/json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BOOL Json_ExtractInt64(const char* json, const char* key, long long* outValue) {
    if (!json || !key || !outValue) return FALSE;

    const char* pos = strstr(json, key);
    if (pos) {
        // Move past the key
        pos += strlen(key);
        
        // Skip whitespace and colon if present
        // JSON standard allows whitespace around the colon
        while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
            pos++;
        }
        
        // Parse number
        char* endPtr;
        *outValue = strtoll(pos, &endPtr, 10);
        
        // Check if we actually parsed a number (endPtr should have moved)
        if (pos == endPtr) return FALSE;
        
        return TRUE;
    }
    
    return FALSE;
}
