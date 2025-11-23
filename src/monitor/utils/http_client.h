#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <windows.h>

/**
 * @brief Execute simple HTTPS GET request
 * @param server Server domain (e.g. "api.github.com")
 * @param path Request path (e.g. "/repos/user/repo")
 * @param userAgent User-Agent header
 * @param additionalHeaders Optional additional headers (can be NULL)
 * @param outBuffer Output buffer
 * @param outBufferSize Size of output buffer
 * @return BOOL Success or failure
 */
BOOL HttpClient_Get(const wchar_t* server, const wchar_t* path, const wchar_t* userAgent, const wchar_t* additionalHeaders, char* outBuffer, DWORD outBufferSize);

#endif
