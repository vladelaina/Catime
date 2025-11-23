#include "monitor/platforms/github.h"
#include "monitor/utils/http_client.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    
    if (config->token[0] != '\0') {
        wchar_t wToken[128];
        MultiByteToWideChar(CP_UTF8, 0, config->token, -1, wToken, 128);
        swprintf(authHeader, 256, L"Authorization: token %ls", wToken);
        headers = authHeader;
    }

    // Buffer for JSON response (8KB is usually enough for repo info)
    char response[8192]; 
    
    if (HttpClient_Get(L"api.github.com", path, userAgent, headers, response, sizeof(response))) {
        // Simple JSON parsing
        // We look for "stargazers_count": 123,
        
        const char* key = NULL;
        if (strcmp(config->param2, "star") == 0) {
            key = "\"stargazers_count\":";
        } else if (strcmp(config->param2, "fork") == 0) {
            key = "\"forks_count\":";
        } else if (strcmp(config->param2, "watcher") == 0) {
            key = "\"subscribers_count\":";
        } else {
            // Default to star if unknown
             key = "\"stargazers_count\":";
        }
        
        char* pos = strstr(response, key);
        if (pos) {
            // Move past the key
            pos += strlen(key);
            
            // Skip whitespace and colon if present (standard JSON format)
            while (*pos == ' ' || *pos == ':') pos++;
            
            // Parse number
            return atoll(pos);
        } else {
            LOG_WARNING("GitHub response did not contain key: %s", key);
        }
    }
    
    return -1;
}
