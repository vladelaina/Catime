#include "monitor/utils/http_client.h"
#include <winhttp.h>
#include <stdio.h>
#include "log.h"

BOOL HttpClient_Get(const wchar_t* server, const wchar_t* path, const wchar_t* userAgent, const wchar_t* additionalHeaders, char* outBuffer, DWORD outBufferSize, DWORD* outStatusCode) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bResults = FALSE;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    DWORD totalRead = 0;
    BOOL success = FALSE;

    // Initialize status code to 0 (unknown)
    if (outStatusCode) *outStatusCode = 0;

    // Initialize buffer
    if (outBufferSize > 0 && outBuffer) {
        outBuffer[0] = '\0';
    } else {
        return FALSE;
    }

    // Open Session
    // Try WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (4) first for better system proxy support (Win 8.1+)
    hSession = WinHttpOpen(userAgent,  
                          4, 
                          WINHTTP_NO_PROXY_NAME, 
                          WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        // Fallback to legacy default proxy
        hSession = WinHttpOpen(userAgent,  
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, 
                              WINHTTP_NO_PROXY_BYPASS, 0);
    }

    if (!hSession) {
        DWORD err = GetLastError();
        LOG_ERROR("WinHttpOpen failed: %lu", err);
        if (outStatusCode) *outStatusCode = err;
        goto cleanup;
    }

    // Set Timeouts (Resolve, Connect, Send, Receive) - 15 seconds each
    WinHttpSetTimeouts(hSession, 15000, 15000, 15000, 15000);

    // Enable TLS 1.2 and 1.3
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    
    // Enable GZIP decompression
    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

    // Connect
    hConnect = WinHttpConnect(hSession, server, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        DWORD err = GetLastError();
        LOG_ERROR("WinHttpConnect failed: %lu", err);
        if (outStatusCode) *outStatusCode = err;
        goto cleanup;
    }

    // Create Request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
                                 NULL, WINHTTP_NO_REFERER, 
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                 WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        DWORD err = GetLastError();
        LOG_ERROR("WinHttpOpenRequest failed: %lu", err);
        if (outStatusCode) *outStatusCode = err;
        goto cleanup;
    }

    // Send Request
    DWORD headerLen = additionalHeaders ? (DWORD)wcslen(additionalHeaders) : 0;
    
    bResults = WinHttpSendRequest(hRequest,
                                 additionalHeaders ? additionalHeaders : WINHTTP_NO_ADDITIONAL_HEADERS, 
                                 headerLen,
                                 WINHTTP_NO_REQUEST_DATA, 0, 
                                 0, 0);
    
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    } else {
        DWORD err = GetLastError();
        LOG_ERROR("WinHttpSendRequest failed: %lu", err);
        if (outStatusCode) *outStatusCode = err;
        goto cleanup;
    }

    if (bResults) {
        // Check status code
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
                           WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) {
             DWORD err = GetLastError();
             LOG_ERROR("WinHttpQueryHeaders failed: %lu", err);
             if (outStatusCode) *outStatusCode = err;
             goto cleanup;
        }
        
        if (outStatusCode) *outStatusCode = dwStatusCode;

        if (dwStatusCode != 200) {
            LOG_WARNING("HTTP Request returned status code: %lu", dwStatusCode);
            goto cleanup;
        }

        // Read Data
        char tempBuffer[8192]; // Use stack buffer to avoid frequent malloc/free
        while (TRUE) {
            DWORD dataSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dataSize)) {
                DWORD err = GetLastError();
                LOG_ERROR("WinHttpQueryDataAvailable failed: %lu", err);
                // Don't overwrite status code if we already have a 200 OK, but here we failed reading
                // We could set it to error, but maybe partial data is useless anyway.
                if (outStatusCode) *outStatusCode = err;
                success = FALSE; 
                break;
            }

            if (dataSize == 0) break;

            // Ensure we don't overflow output buffer
            if (totalRead + dataSize >= outBufferSize - 1) {
                LOG_WARNING("HttpClient response buffer overflow (max: %lu), truncating.", outBufferSize);
                dataSize = outBufferSize - 1 - totalRead;
                if (dataSize == 0) break; // Buffer full
            }
            
            // Cap reading to temp buffer size
            if (dataSize > sizeof(tempBuffer)) {
                dataSize = sizeof(tempBuffer);
            }

            if (WinHttpReadData(hRequest, (LPVOID)tempBuffer, dataSize, &dwDownloaded)) {
                memcpy(outBuffer + totalRead, tempBuffer, dwDownloaded);
                totalRead += dwDownloaded;
                outBuffer[totalRead] = '\0';
            } else {
                DWORD err = GetLastError();
                if (outStatusCode) *outStatusCode = err;
                success = FALSE;
                break;
            }
        }
                              
        // Only success if we read something (or if empty body is allowed? Assuming we expect JSON)
        if (totalRead > 0) success = TRUE;
    } else {
        DWORD err = GetLastError();
        LOG_ERROR("WinHttpReceiveResponse failed: %lu", err);
        if (outStatusCode) *outStatusCode = err;
    }

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return success;
}
