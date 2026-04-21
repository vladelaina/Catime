#include "utils/url_safety.h"
#include <ctype.h>
#include <string.h>
#include <wctype.h>

BOOL IsSafeOpenUrlA(const char* url) {
    if (!url) return FALSE;

    while (*url && isspace((unsigned char)*url)) {
        url++;
    }

    return _strnicmp(url, "http://", 7) == 0 || _strnicmp(url, "https://", 8) == 0;
}

BOOL IsSafeOpenUrlW(const wchar_t* url) {
    if (!url) return FALSE;

    while (*url && iswspace(*url)) {
        url++;
    }

    return _wcsnicmp(url, L"http://", 7) == 0 || _wcsnicmp(url, L"https://", 8) == 0;
}
