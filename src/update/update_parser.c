/**
 * @file update_parser.c
 * @brief JSON parsing and version comparison logic
 */
#include "update/update_internal.h"
#include <string.h>
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
    if (end == text || errno == ERANGE || parsed < 0) {
        return FALSE;
    }
#if LONG_MAX > INT_MAX
    if (parsed > INT_MAX) {
        return FALSE;
    }
#endif

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
    if (!preRelease || maxLen == 0) {
        return FALSE;
    }
    preRelease[0] = '\0';
    if (!version) {
        return FALSE;
    }

    const char* dash = strchr(version, '-');
    if (dash && *(dash + 1)) {
        size_t len = strlen(dash + 1);
        if (len >= maxLen) len = maxLen - 1;
        strncpy(preRelease, dash + 1, len);
        preRelease[len] = '\0';
        return TRUE;
    }
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
