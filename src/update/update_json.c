#include "update/update_internal.h"
#include "log.h"
#include "utils/url_safety.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

static BOOL ExtractJsonString(const char* json, const char* fieldName,
                              char* output, size_t maxLength,
                              BOOL allowTruncate) {
    if (!output || maxLength == 0) return FALSE;
    output[0] = '\0';
    if (!json || !fieldName) return FALSE;

    char pattern[128];
    int patternLength =
        snprintf(pattern, sizeof(pattern), "\"%s\":", fieldName);
    if (patternLength < 0 || (size_t)patternLength >= sizeof(pattern)) {
        return FALSE;
    }

    const char* field = strstr(json, pattern);
    if (!field) return FALSE;
    const char* valueStart = strchr(field + patternLength, '\"');
    if (!valueStart) return FALSE;
    valueStart++;

    const char* valueEnd = valueStart;
    int backslashes = 0;
    while (*valueEnd) {
        if (*valueEnd == '\\') {
            backslashes++;
        } else if (*valueEnd == '\"' && (backslashes % 2 == 0)) {
            break;
        } else {
            backslashes = 0;
        }
        valueEnd++;
    }
    if (*valueEnd != '\"') return FALSE;

    size_t valueLength = (size_t)(valueEnd - valueStart);
    if (valueLength >= maxLength) {
        if (!allowTruncate) return FALSE;
        valueLength = maxLength - 1;
    }
    memcpy(output, valueStart, valueLength);
    output[valueLength] = '\0';
    return TRUE;
}

static BOOL ExtractSafeDownloadUrl(const char* json, char* output,
                                   size_t maxLength) {
    static const char fieldName[] = "browser_download_url";
    static const char pattern[] = "\"browser_download_url\":";
    if (!output || maxLength == 0) return FALSE;
    output[0] = '\0';
    if (!json) return FALSE;

    const char* cursor = json;
    while ((cursor = strstr(cursor, pattern)) != NULL) {
        char candidate[URL_BUFFER_SIZE] = {0};
        if (ExtractJsonString(cursor, fieldName, candidate, sizeof(candidate),
                              FALSE) &&
            IsSafeUpdateDownloadUrlA(candidate)) {
            return SUCCEEDED(StringCbCopyA(output, maxLength, candidate));
        }
        cursor += sizeof(pattern) - 1;
    }
    LOG_ERROR("No safe update download URL found");
    return FALSE;
}

static void DecodeJsonEscapes(const char* input, char* output,
                              size_t maxLength) {
    if (!output || maxLength == 0) return;
    output[0] = '\0';
    if (!input) return;

    size_t writePosition = 0;
    for (size_t i = 0; input[i] && writePosition < maxLength - 1; i++) {
        if (input[i] != '\\' || input[i + 1] == '\0') {
            output[writePosition++] = input[i];
            continue;
        }
        char escaped = input[++i];
        if (escaped == 'n') {
            output[writePosition++] = '\r';
            if (writePosition < maxLength - 1) {
                output[writePosition++] = '\n';
            }
        } else if (escaped == 'r') {
            output[writePosition++] = '\r';
        } else if (escaped == '\"' || escaped == '\\') {
            output[writePosition++] = escaped;
        } else {
            output[writePosition++] = '\\';
            if (writePosition < maxLength - 1) {
                output[writePosition++] = escaped;
            }
        }
    }
    output[writePosition] = '\0';
}

BOOL ParseGitHubRelease(const char* jsonResponse,
                        char* latestVersion, size_t versionMaxLen,
                        char* downloadUrl, size_t urlMaxLen,
                        char* releaseNotes, size_t notesMaxLen) {
    if (latestVersion && versionMaxLen) latestVersion[0] = '\0';
    if (downloadUrl && urlMaxLen) downloadUrl[0] = '\0';
    if (releaseNotes && notesMaxLen) releaseNotes[0] = '\0';
    if (!latestVersion || !downloadUrl || !releaseNotes) return FALSE;

    if (!ExtractJsonString(jsonResponse, "tag_name", latestVersion,
                           versionMaxLen, FALSE)) {
        return FALSE;
    }
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, strlen(latestVersion));
    }
    if (!ExtractSafeDownloadUrl(jsonResponse, downloadUrl, urlMaxLen)) {
        return FALSE;
    }

    char* rawNotes = (char*)malloc(NOTES_BUFFER_SIZE);
    if (!rawNotes) return FALSE;
    if (ExtractJsonString(jsonResponse, "body", rawNotes,
                          NOTES_BUFFER_SIZE, TRUE)) {
        DecodeJsonEscapes(rawNotes, releaseNotes, notesMaxLen);
    } else {
        LOG_WARNING("Release notes not found, using default text");
        StringCbCopyA(releaseNotes, notesMaxLen,
                      "No release notes available.");
    }
    free(rawNotes);
    return TRUE;
}
