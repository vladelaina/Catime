#include "utils/url_safety.h"
#include <ctype.h>
#include <string.h>
#include <wctype.h>

#define CATIME_RELEASE_DOWNLOAD_PREFIX "https://github.com/vladelaina/Catime/releases/download/"

static const char* SkipAsciiWhitespaceA(const char* url) {
    while (*url && isspace((unsigned char)*url)) {
        url++;
    }
    return url;
}

BOOL IsSafeOpenUrlA(const char* url) {
    if (!url) return FALSE;

    url = SkipAsciiWhitespaceA(url);

    return _strnicmp(url, "http://", 7) == 0 || _strnicmp(url, "https://", 8) == 0;
}

BOOL IsSafeOpenUrlW(const wchar_t* url) {
    if (!url) return FALSE;

    while (*url && iswspace(*url)) {
        url++;
    }

    return _wcsnicmp(url, L"http://", 7) == 0 || _wcsnicmp(url, L"https://", 8) == 0;
}

BOOL IsSafeUpdateDownloadUrlA(const char* url) {
    if (!url) return FALSE;

    url = SkipAsciiWhitespaceA(url);
    if (_strnicmp(url, CATIME_RELEASE_DOWNLOAD_PREFIX,
                  strlen(CATIME_RELEASE_DOWNLOAD_PREFIX)) != 0) {
        return FALSE;
    }

    const char* assetName = strrchr(url, '/');
    if (!assetName || _strnicmp(assetName + 1, "catime_", 7) != 0) {
        return FALSE;
    }

    const char* dot = strrchr(assetName, '.');
    if (!dot || _stricmp(dot, ".exe") != 0) {
        return FALSE;
    }

    for (const unsigned char* p = (const unsigned char*)url; *p; p++) {
        if (*p <= 0x20 || *p == '\\') {
            return FALSE;
        }
    }

    return TRUE;
}
