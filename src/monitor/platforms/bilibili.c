#include "monitor/platforms/bilibili.h"
#include "monitor/utils/http_client.h"
#include "monitor/utils/json_parser.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wincrypt.h>
#include <ctype.h>

// Bilibili API requires a browser-like User-Agent
static const wchar_t* USER_AGENT = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36";

long long Bilibili_FetchUserValue(const MonitorConfig* config) {
    if (!config) return -1;

    // param1: UID
    // param2: key (follower, likes)

    wchar_t path[256];
    char response[8192];
    const wchar_t* host = L"api.bilibili.com";
    
    // Basic headers for public API
    const wchar_t* headers = L"Referer: https://www.bilibili.com/\r\n"
                             L"Accept: application/json, text/plain, */*\r\n"
                             L"Connection: keep-alive\r\n"
                             L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n";

    // Determine endpoint based on key
    if (strcmp(config->param2, "follower") == 0) {
        swprintf(path, 256, L"/x/relation/stat?vmid=%hs", config->param1);
        
        DWORD statusCode = 0;
        if (HttpClient_Get(host, path, USER_AGENT, headers, response, sizeof(response), &statusCode)) {
            long long code = 0;
            if (Json_ExtractInt64(response, "\"code\":", &code) && code != 0) {
                LOG_WARNING("Bilibili API error code: %lld", code);
                return code > 0 ? -code : code;
            }
            
            long long value = 0;
            if (Json_ExtractInt64(response, "\"follower\":", &value)) {
                return value;
            }
        } else {
             if (statusCode > 0) return -(long long)statusCode;
        }
    } else if (strcmp(config->param2, "likes") == 0) {
        // Use /x/web-interface/card for likes (public)
        swprintf(path, 256, L"/x/web-interface/card?mid=%hs", config->param1);
        
        DWORD statusCode = 0;
        if (HttpClient_Get(host, path, USER_AGENT, headers, response, sizeof(response), &statusCode)) {
            long long code = 0;
            if (Json_ExtractInt64(response, "\"code\":", &code) && code != 0) {
                LOG_WARNING("Bilibili API error code: %lld", code);
                return code > 0 ? -code : code;
            }
            
            long long value = 0;
            if (Json_ExtractInt64(response, "\"like_num\":", &value)) {
                return value;
            }
            LOG_WARNING("Failed to parse like_num from Bilibili response: %.200s...", response);
            return -401; // Needs login
        } else {
             if (statusCode > 0) return -(long long)statusCode;
        }
    }
    
    return -1;
}

long long Bilibili_FetchVideoValue(const MonitorConfig* config) {
    if (!config) return -1;

    // param1: BVID
    // param2: like, coin, favorite, share
    
    wchar_t path[256];
    char response[16384]; // Video info can be large
    const wchar_t* host = L"api.bilibili.com";
    const wchar_t* headers = L"Referer: https://www.bilibili.com/\r\nAccept: application/json, text/plain, */*\r\nConnection: keep-alive\r\nCookie: \r\n";
    
    swprintf(path, 256, L"/x/web-interface/view?bvid=%hs", config->param1);
    
    DWORD statusCode = 0;
    if (HttpClient_Get(host, path, USER_AGENT, headers, response, sizeof(response), &statusCode)) {
        // JSON structure: { "data": { "stat": { "view": ..., "danmaku": ..., "reply": ..., "favorite": ..., "coin": ..., "share": ..., "like": ... } } }
        
        const char* key = NULL;
        if (strcmp(config->param2, "like") == 0) key = "\"like\":";
        else if (strcmp(config->param2, "coin") == 0) key = "\"coin\":";
        else if (strcmp(config->param2, "favorite") == 0) key = "\"favorite\":";
        else if (strcmp(config->param2, "share") == 0) key = "\"share\":";
        else return -1;
        
        long long value = 0;
        if (Json_ExtractInt64(response, key, &value)) {
            return value;
        }
    } else {
        if (statusCode > 0) return -(long long)statusCode;
    }
    
    return -1;
}

int Bilibili_GetOptions(MonitorPlatformType type, MonitorOption* outOptions, int maxCount) {
    if (!outOptions) return 0;
    
    if (type == MONITOR_PLATFORM_BILIBILI_USER) {
        static const struct {
            const wchar_t* label;
            const char* value;
        } userOptions[] = {
            { L"Followers", "follower" },
            { L"Likes", "likes" }
        };
        
        int count = sizeof(userOptions) / sizeof(userOptions[0]);
        int actualCount = (count < maxCount) ? count : maxCount;
        
        for (int i = 0; i < actualCount; i++) {
            wcsncpy(outOptions[i].label, userOptions[i].label, 32);
            strncpy(outOptions[i].value, userOptions[i].value, 32);
        }
        return count;
    } else if (type == MONITOR_PLATFORM_BILIBILI_VIDEO) {
        static const struct {
            const wchar_t* label;
            const char* value;
        } videoOptions[] = {
            { L"Likes", "like" },
            { L"Coins", "coin" },
            { L"Favorites", "favorite" },
            { L"Shares", "share" }
        };
        
        int count = sizeof(videoOptions) / sizeof(videoOptions[0]);
        int actualCount = (count < maxCount) ? count : maxCount;
        
        for (int i = 0; i < actualCount; i++) {
            wcsncpy(outOptions[i].label, videoOptions[i].label, 32);
            strncpy(outOptions[i].value, videoOptions[i].value, 32);
        }
        return count;
    }
    
    return 0;
}
