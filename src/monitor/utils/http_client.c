#include "monitor/utils/http_client.h"
#include <winhttp.h>
#include <stdio.h>
#include "log.h"

BOOL HttpClient_Get(const wchar_t* server, const wchar_t* path, const wchar_t* userAgent, const wchar_t* additionalHeaders, char* outBuffer, DWORD outBufferSize) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bResults = FALSE;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    DWORD totalRead = 0;
    BOOL success = FALSE;

    // Initialize buffer
    if (outBufferSize > 0 && outBuffer) {
        outBuffer[0] = '\0';
    } else {
        return FALSE;
    }

    // Open Session
    hSession = WinHttpOpen(userAgent,  
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, 
                          WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        LOG_ERROR("WinHttpOpen failed: %lu", GetLastError());
        goto cleanup;
    }

    // Connect
    hConnect = WinHttpConnect(hSession, server, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        LOG_ERROR("WinHttpConnect failed: %lu", GetLastError());
        goto cleanup;
    }

    // Create Request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
                                 NULL, WINHTTP_NO_REFERER, 
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                 WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        LOG_ERROR("WinHttpOpenRequest failed: %lu", GetLastError());
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
        LOG_ERROR("WinHttpSendRequest failed: %lu", GetLastError());
        goto cleanup;
    }

    if (bResults) {
        // Check status code
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
                           WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        
        if (dwStatusCode != 200) {
            LOG_WARNING("HTTP Request returned status code: %lu", dwStatusCode);
            goto cleanup;
        }

        // Read Data
        char tempBuffer[8192]; // Use stack buffer to avoid frequent malloc/free
        while (TRUE) {
            DWORD dataSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dataSize)) {
                LOG_ERROR("WinHttpQueryDataAvailable failed: %lu", GetLastError());
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
                break;
            }
        }
                              
        success = (totalRead > 0);
    } else {
        LOG_ERROR("WinHttpReceiveResponse failed: %lu", GetLastError());
    }

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return success;
}
