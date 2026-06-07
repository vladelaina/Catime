/**
 * @file update_parser.c
 * @brief JSON parsing and version comparison logic
 */
#include "update/update_internal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static const PreReleaseType PRE_RELEASE_TYPES[] = {
    {"alpha", 5, 1},
    {"beta", 4, 2},
    {"rc", 2, 3}
};

static const int PRE_RELEASE_TYPE_COUNT = sizeof(PRE_RELEASE_TYPES) / sizeof(PreReleaseType);

static BOOL ParseNonNegativeIntBounded(const char* text, int* value, const char** endOut) {
    if (!text || !value) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (!isdigit((unsigned char)*text)) return FALSE;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || parsed < 0 || parsed > INT_MAX) {
        return FALSE;
    }

    *value = (int)parsed;
    if (endOut) {
        *endOut = end;
    }
    return TRUE;
}

static BOOL ParseVersionCore(const char* version, int* major, int* minor, int* patch) {
    if (!version || !major || !minor || !patch) return FALSE;

    const char* cursor = version;
    int parsedMajor = 0;
    int parsedMinor = 0;
    int parsedPatch = 0;

    if (!ParseNonNegativeIntBounded(cursor, &parsedMajor, &cursor) || *cursor != '.') {
        return FALSE;
    }
    cursor++;

    if (!ParseNonNegativeIntBounded(cursor, &parsedMinor, &cursor) || *cursor != '.') {
        return FALSE;
    }
    cursor++;

    if (!ParseNonNegativeIntBounded(cursor, &parsedPatch, &cursor)) {
        return FALSE;
    }

    while (isspace((unsigned char)*cursor)) cursor++;
    if (*cursor != '\0' && *cursor != '-') {
        return FALSE;
    }

    *major = parsedMajor;
    *minor = parsedMinor;
    *patch = parsedPatch;
    return TRUE;
}

/**
 * @brief Extract string field from GitHub JSON response
 * @param fieldName Field to extract (e.g., "tag_name", "body")
 * @return TRUE if field found and extracted
 */
static BOOL ExtractJsonStringField(const char* json, const char* fieldName, char* output, size_t maxLen) {
    if (!output || maxLen == 0) return FALSE;
    output[0] = '\0';
    if (!json || !fieldName) return FALSE;

    char pattern[128];
    int patternLen = snprintf(pattern, sizeof(pattern), "\"%s\":", fieldName);
    if (patternLen < 0 || (size_t)patternLen >= sizeof(pattern)) {
        return FALSE;
    }

    const char* fieldPos = strstr(json, pattern);
    if (!fieldPos) {
        LOG_ERROR("JSON field not found: %s", fieldName);
        return FALSE;
    }

    const char* valueStart = strchr(fieldPos + patternLen, '\"');
    if (!valueStart) return FALSE;
    valueStart++;
    
    const char* valueEnd = valueStart;
    int escapeCount = 0;
    while (*valueEnd) {
        if (*valueEnd == '\\') {
            escapeCount++;
        } else if (*valueEnd == '\"' && (escapeCount % 2 == 0)) {
            break;
        } else {
            escapeCount = 0;
        }
        valueEnd++;
    }
    
    if (*valueEnd != '\"') return FALSE;
    
    size_t valueLen = valueEnd - valueStart;
    if (valueLen >= maxLen) valueLen = maxLen - 1;
    strncpy(output, valueStart, valueLen);
    output[valueLen] = '\0';
    
    return TRUE;
}

/** @brief Process JSON escape sequences (\n, \r, \", \\) */
static void ProcessJsonEscapes(const char* input, char* output, size_t maxLen) {
    if (!output || maxLen == 0) return;
    output[0] = '\0';
    if (!input) return;

    size_t writePos = 0;

    for (size_t i = 0; input[i] != '\0' && writePos < maxLen - 1; i++) {
        if (input[i] == '\\' && input[i + 1] != '\0') {
            switch (input[i + 1]) {
                case 'n':
                    output[writePos++] = '\r';
                    if (writePos < maxLen - 1) output[writePos++] = '\n';
                    i++;
                    break;
                case 'r':
                    output[writePos++] = '\r';
                    i++;
                    break;
                case '\"':
                    output[writePos++] = '\"';
                    i++;
                    break;
                case '\\':
                    output[writePos++] = '\\';
                    i++;
                    break;
                default:
                    output[writePos++] = input[i];
            }
        } else {
            output[writePos++] = input[i];
        }
    }
    output[writePos] = '\0';
}

/**
 * @brief Parse pre-release type and number
 * @param preRelease String like "alpha2", "beta1", "rc3"
 * @param outType Priority: 1=alpha, 2=beta, 3=rc, 0=unknown
 * @param outNum Version number after prefix
 */
static void ParsePreReleaseInfo(const char* preRelease, int* outType, int* outNum) {
    *outType = 0;
    *outNum = 0;
    
    if (!preRelease || !preRelease[0]) return;
    
    for (int i = 0; i < PRE_RELEASE_TYPE_COUNT; i++) {
        const PreReleaseType* type = &PRE_RELEASE_TYPES[i];
        if (strncmp(preRelease, type->prefix, type->prefixLen) == 0) {
            *outType = type->priority;
            const char* suffix = preRelease + type->prefixLen;
            int parsedNum = 0;
            const char* end = NULL;
            if (ParseNonNegativeIntBounded(suffix, &parsedNum, &end)) {
                while (end && isspace((unsigned char)*end)) end++;
                if (end && *end == '\0') {
                    *outNum = parsedNum;
                }
            }
            return;
        }
    }
}

/**
 * @brief Extract pre-release tag from version
 * @param version Full version like "1.3.0-alpha2"
 * @return TRUE if pre-release tag found
 */
static BOOL ExtractPreRelease(const char* version, char* preRelease, size_t maxLen) {
    const char* dash = strchr(version, '-');
    if (dash && *(dash + 1)) {
        size_t len = strlen(dash + 1);
        if (len >= maxLen) len = maxLen - 1;
        strncpy(preRelease, dash + 1, len);
        preRelease[len] = '\0';
        return TRUE;
    }
    preRelease[0] = '\0';
    return FALSE;
}

/**
 * @brief Compare pre-release tags
 * @return 1 if pre1 > pre2, -1 if pre1 < pre2, 0 if equal
 * @note Stable > rc > beta > alpha
 */
static int ComparePreRelease(const char* pre1, const char* pre2) {
    if (!pre1[0] && !pre2[0]) return 0;
    
    if (!pre1[0]) return 1;
    if (!pre2[0]) return -1;
    
    int type1, num1, type2, num2;
    ParsePreReleaseInfo(pre1, &type1, &num1);
    ParsePreReleaseInfo(pre2, &type2, &num2);
    
    if (type1 != type2) {
        return (type1 > type2) ? 1 : -1;
    }
    
    if (num1 != num2) {
        return (num1 > num2) ? 1 : -1;
    }
    
    return strcmp(pre1, pre2);
}

/**
 * @brief Compare semantic versions (major.minor.patch-prerelease)
 * @return 1 if v1 > v2, -1 if v1 < v2, 0 if equal
 */
int CompareVersions(const char* version1, const char* version2) {
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    if (!ParseVersionCore(version1, &major1, &minor1, &patch1)) {
        major1 = minor1 = patch1 = 0;
    }
    if (!ParseVersionCore(version2, &major2, &minor2, &patch2)) {
        major2 = minor2 = patch2 = 0;
    }
    
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    
    char preRelease1[64] = {0};
    char preRelease2[64] = {0};
    ExtractPreRelease(version1, preRelease1, sizeof(preRelease1));
    ExtractPreRelease(version2, preRelease2, sizeof(preRelease2));
    
    return ComparePreRelease(preRelease1, preRelease2);
}

/**
 * @brief Parse GitHub release JSON
 * @return TRUE if all required fields extracted
 * @note Strips 'v' prefix from tag_name
 */
BOOL ParseGitHubRelease(const char* jsonResponse, char* latestVersion, size_t versionMaxLen,
                               char* downloadUrl, size_t urlMaxLen, char* releaseNotes, size_t notesMaxLen) {
    if (latestVersion && versionMaxLen > 0) latestVersion[0] = '\0';
    if (downloadUrl && urlMaxLen > 0) downloadUrl[0] = '\0';
    if (releaseNotes && notesMaxLen > 0) releaseNotes[0] = '\0';

    if (!ExtractJsonStringField(jsonResponse, "tag_name", latestVersion, versionMaxLen)) {
        return FALSE;
    }
    
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, strlen(latestVersion));
    }
    
    if (!ExtractJsonStringField(jsonResponse, "browser_download_url", downloadUrl, urlMaxLen)) {
        return FALSE;
    }
    
    char* rawNotes = (char*)malloc(NOTES_BUFFER_SIZE);
    if (!rawNotes) {
        return FALSE;
    }

    if (ExtractJsonStringField(jsonResponse, "body", rawNotes, NOTES_BUFFER_SIZE)) {
        ProcessJsonEscapes(rawNotes, releaseNotes, notesMaxLen);
    } else {
        LOG_WARNING("Release notes not found, using default text");
        StringCbCopyA(releaseNotes, notesMaxLen, "No release notes available.");
    }

    free(rawNotes);
    return TRUE;
}
