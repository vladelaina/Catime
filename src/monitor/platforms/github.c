#include "monitor/platforms/github.h"
#include "monitor/utils/http_client.h"
#include "monitor/utils/json_parser.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wincrypt.h>

long long GitHub_FetchValue(const MonitorConfig* config) {
    if (!config) return -1;

    // param1: user/repo (e.g. "vladelaina/Catime")
    // param2: key (e.g. "star", "fork")
    
    wchar_t path[256];
    // Construct path: /repos/user/repo
    swprintf(path, 256, L"/repos/%hs", config->param1);
    
    // GitHub API requires a User-Agent
    const wchar_t* userAgent = L"Catime Monitor/1.0";
    
    // Use provided token for authorization if available
    wchar_t authHeader[256] = {0};
    const wchar_t* headers = NULL;
    
    // Check if we have an encrypted token (scan for any non-zero byte)
    // We can't use string functions because the encrypted blob might contain null bytes
    BOOL hasToken = FALSE;
    for (size_t i = 0; i < sizeof(config->token); i++) {
        if (config->token[i] != 0) {
            hasToken = TRUE;
            break;
        }
    }
    
    if (hasToken) {
        // Copy encrypted token to stack buffer
        char tempToken[128];
        memcpy(tempToken, config->token, sizeof(tempToken));
        
        // Decrypt in-place on stack
        // Note: This matches the encryption done in Monitor_LoadConfig
        if (CryptUnprotectMemory(tempToken, sizeof(tempToken), CRYPTPROTECTMEMORY_SAME_PROCESS)) {
            wchar_t wToken[128];
            MultiByteToWideChar(CP_UTF8, 0, tempToken, -1, wToken, 128);
            
            // Sanitize token: remove any non-printable chars (especially newlines)
            // and trim whitespace
            size_t len = wcslen(wToken);
            wchar_t cleanToken[128] = {0};
            int cleanIdx = 0;
            for (size_t k = 0; k < len && cleanIdx < 127; k++) {
                if (!iswspace(wToken[k]) && iswprint(wToken[k])) {
                    cleanToken[cleanIdx++] = wToken[k];
                }
            }
            cleanToken[cleanIdx] = L'\0';
            
            // Basic validation of token format (should be alphanumeric)
            if (wcslen(cleanToken) < 10) {
                 LOG_WARNING("Token seems too short or empty after decryption. CleanLen: %zu", wcslen(cleanToken));
            }
            
            // Append CRLF as required by some WinHttp versions/proxies
            swprintf(authHeader, 256, L"Authorization: token %ls\r\n", cleanToken);
            headers = authHeader;
            SecureZeroMemory(wToken, sizeof(wToken));
            SecureZeroMemory(cleanToken, sizeof(cleanToken)); // Also wipe clean buffer
        } else {
            LOG_ERROR("Failed to decrypt token in GitHub_FetchValue");
        }
        
        // Immediate cleanup of the raw token buffers
        SecureZeroMemory(tempToken, sizeof(tempToken));
    }

    // Buffer for JSON response (8KB is usually enough for repo info)
    char response[8192]; 
    
    BOOL success = HttpClient_Get(L"api.github.com", path, userAgent, headers, response, sizeof(response));
    
    // Cleanup auth header immediately after use
    if (headers) {
        SecureZeroMemory(authHeader, sizeof(authHeader));
    }

    if (success) {
        // Simple JSON parsing using helper
        const char* key = NULL;
        if (strcmp(config->param2, "star") == 0) {
            key = "\"stargazers_count\":";
        } else if (strcmp(config->param2, "fork") == 0) {
            key = "\"forks_count\":";
        } else if (strcmp(config->param2, "watcher") == 0) {
            key = "\"subscribers_count\":";
        } else if (strcmp(config->param2, "issue") == 0) {
            key = "\"open_issues_count\":";
        } else {
            // Default to star if unknown
             key = "\"stargazers_count\":";
        }
        
        long long value = 0;
        if (Json_ExtractInt64(response, key, &value)) {
            return value;
        } else {
            LOG_WARNING("GitHub response did not contain key: %s", key);
        }
    }
    
    return -1;
}

int GitHub_GetOptions(MonitorOption* outOptions, int maxCount) {
    // List of supported options
    static const struct {
        const wchar_t* label;
        const char* value;
    } options[] = {
        { L"Stars", "star" },
        { L"Forks", "fork" },
        { L"Watchers", "watcher" },
        { L"Open Issues", "issue" }
    };
    
    int count = sizeof(options) / sizeof(options[0]);
    
    if (!outOptions) return count;
    
    int actualCount = (count < maxCount) ? count : maxCount;
    
    for (int i = 0; i < actualCount; i++) {
        wcsncpy(outOptions[i].label, options[i].label, 32);
        strncpy(outOptions[i].value, options[i].value, 32);
    }
    
    return count;
}
