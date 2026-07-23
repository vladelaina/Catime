#include "update/update_internal.h"
#include "log.h"
#include "utils/string_convert.h"

#include <stdlib.h>
#include <string.h>

#define MAX_HTTP_RESPONSE_SIZE (1024u * 1024u)
#define HTTP_TIMEOUT_MS 10000u

static BOOL InitializeHttp(HttpResources* resources) {
    memset(resources, 0, sizeof(*resources));
    wchar_t userAgent[256];
    if (!Utf8ToWide(USER_AGENT, userAgent, _countof(userAgent))) return FALSE;

    resources->hInternet = InternetOpenW(
        userAgent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!resources->hInternet) {
        LOG_ERROR("Failed to create Internet session (error=%lu)",
                  GetLastError());
        return FALSE;
    }
    UpdateHttp_TrackInternet(resources->hInternet);
    if (UpdateHttp_IsCancelRequested()) {
        UpdateHttp_CloseTracked(&resources->hInternet);
        return FALSE;
    }

    DWORD timeout = HTTP_TIMEOUT_MS;
    InternetSetOptionW(resources->hInternet, INTERNET_OPTION_CONNECT_TIMEOUT,
                       &timeout, sizeof(timeout));
    InternetSetOptionW(resources->hInternet, INTERNET_OPTION_SEND_TIMEOUT,
                       &timeout, sizeof(timeout));
    InternetSetOptionW(resources->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                       &timeout, sizeof(timeout));
    return TRUE;
}

static BOOL ConnectToReleaseApi(HttpResources* resources) {
    wchar_t url[URL_BUFFER_SIZE];
    if (!Utf8ToWide(GITHUB_API_URL, url, _countof(url))) return FALSE;
    resources->hConnect = InternetOpenUrlW(
        resources->hInternet, url, NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!resources->hConnect) {
        if (!UpdateHttp_IsCancelRequested()) {
            LOG_ERROR("Failed to connect to GitHub API (error=%lu)",
                      GetLastError());
        }
        return FALSE;
    }
    UpdateHttp_TrackConnect(resources->hConnect);
    if (!UpdateHttp_IsCancelRequested()) return TRUE;
    UpdateHttp_CloseTracked(&resources->hConnect);
    return FALSE;
}

static BOOL GrowResponseBuffer(HttpResources* resources, size_t* capacity,
                               size_t bytesRead) {
    size_t maximumCapacity = MAX_HTTP_RESPONSE_SIZE + 1u;
    size_t newCapacity = *capacity * 2u;
    if (newCapacity > maximumCapacity) newCapacity = maximumCapacity;
    if (newCapacity <= *capacity || newCapacity <= bytesRead + 1u) return FALSE;
    char* resized = (char*)realloc(resources->buffer, newCapacity);
    if (!resized) return FALSE;
    resources->buffer = resized;
    *capacity = newCapacity;
    return TRUE;
}

static BOOL ReadResponse(HttpResources* resources) {
    size_t capacity = INITIAL_HTTP_BUFFER_SIZE;
    resources->buffer = (char*)malloc(capacity);
    if (!resources->buffer) return FALSE;

    size_t total = 0;
    for (;;) {
        if (UpdateHttp_IsCancelRequested()) return FALSE;
        if (total >= MAX_HTTP_RESPONSE_SIZE) {
            char extraByte = '\0';
            DWORD extraRead = 0;
            if (!InternetReadFile(resources->hConnect, &extraByte, 1,
                                  &extraRead) || extraRead > 0) {
                return FALSE;
            }
            break;
        }

        if (capacity - total <= 1u &&
            !GrowResponseBuffer(resources, &capacity, total)) {
            return FALSE;
        }
        size_t writable = capacity - total - 1u;
        size_t remaining = MAX_HTTP_RESPONSE_SIZE - total;
        if (writable > remaining) writable = remaining;

        DWORD chunk = 0;
        if (!InternetReadFile(resources->hConnect,
                              resources->buffer + total,
                              (DWORD)writable, &chunk)) {
            if (!UpdateHttp_IsCancelRequested()) {
                LOG_ERROR("Failed to read update response (error=%lu)",
                          GetLastError());
            }
            return FALSE;
        }
        if (chunk == 0) break;
        total += chunk;
    }
    if (UpdateHttp_IsCancelRequested()) return FALSE;
    resources->buffer[total] = '\0';
    return TRUE;
}

static void CleanupHttp(HttpResources* resources) {
    free(resources->buffer);
    resources->buffer = NULL;
    UpdateHttp_CloseTracked(&resources->hConnect);
    UpdateHttp_CloseTracked(&resources->hInternet);
}

UpdateHttpResult UpdateHttp_FetchRelease(char** response) {
    if (!response) return UPDATE_HTTP_READ_FAILED;
    *response = NULL;
    HttpResources resources;
    if (!InitializeHttp(&resources)) {
        return UpdateHttp_IsCancelRequested()
            ? UPDATE_HTTP_CANCELLED : UPDATE_HTTP_INIT_FAILED;
    }
    if (!ConnectToReleaseApi(&resources)) {
        CleanupHttp(&resources);
        return UpdateHttp_IsCancelRequested()
            ? UPDATE_HTTP_CANCELLED : UPDATE_HTTP_CONNECT_FAILED;
    }
    if (!ReadResponse(&resources)) {
        CleanupHttp(&resources);
        return UpdateHttp_IsCancelRequested()
            ? UPDATE_HTTP_CANCELLED : UPDATE_HTTP_READ_FAILED;
    }
    *response = resources.buffer;
    resources.buffer = NULL;
    CleanupHttp(&resources);
    return UPDATE_HTTP_OK;
}
