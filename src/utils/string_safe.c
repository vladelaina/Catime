/**
 * @file string_safe.c
 * @brief Safe string manipulation implementation
 */

#include "utils/string_safe.h"
#include <string.h>
#include <wchar.h>

int safe_strncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return -1;
    }
    
    if (!src) {
        dest[0] = '\0';
        return 0;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    if (copy_len > 0) {
        memcpy(dest, src, copy_len);
    }
    dest[copy_len] = '\0';
    
    return (src_len < dest_size) ? (int)src_len : -1;
}

int safe_strncat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t dest_len = strnlen(dest, dest_size);
    if (dest_len >= dest_size) {
        return -1;
    }
    
    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    if (copy_len > 0) {
        memcpy(dest + dest_len, src, copy_len);
    }
    dest[dest_len + copy_len] = '\0';
    
    return (src_len <= remaining) ? (int)(dest_len + src_len) : -1;
}

int safe_wcsncpy(wchar_t* dest, const wchar_t* src, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return -1;
    }

    if (!src) {
        dest[0] = L'\0';
        return 0;
    }

    size_t src_len = wcslen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    if (copy_len > 0) {
        wmemcpy(dest, src, copy_len);
    }
    dest[copy_len] = L'\0';

    return (src_len < dest_size) ? (int)src_len : -1;
}
