#include "main_initialization_internal.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

#include <stdio.h>
#include <string.h>

#define LAST_CHECK_DATE_KEY "AUTO_UPDATE_LAST_CHECK_DATE"
#define LAST_CHECK_VERSION_KEY "AUTO_UPDATE_LAST_CHECK_VERSION"
#define DATE_BUFFER_SIZE 16

static BOOL FormatLocalDate(char* date, size_t size) {
    if (!date || size < DATE_BUFFER_SIZE) return FALSE;
    SYSTEMTIME now;
    GetLocalTime(&now);
    return snprintf(date, size, "%04u-%02u-%02u",
                    (unsigned)now.wYear, (unsigned)now.wMonth,
                    (unsigned)now.wDay) > 0;
}

static BOOL ParseDate(const char* text, int* year, int* month, int* day) {
    if (!text || !year || !month || !day) return FALSE;
    char tail = '\0';
    int parsedYear = 0;
    int parsedMonth = 0;
    int parsedDay = 0;
    if (sscanf(text, "%4d-%2d-%2d%c", &parsedYear, &parsedMonth,
               &parsedDay, &tail) != 3 ||
        parsedYear < 2000 || parsedYear > 9999 ||
        parsedMonth < 1 || parsedMonth > 12 ||
        parsedDay < 1 || parsedDay > 31) {
        return FALSE;
    }
    *year = parsedYear;
    *month = parsedMonth;
    *day = parsedDay;
    return TRUE;
}

static int CompareDates(const char* left, const char* right) {
    int leftYear = 0, leftMonth = 0, leftDay = 0;
    int rightYear = 0, rightMonth = 0, rightDay = 0;
    if (!ParseDate(left, &leftYear, &leftMonth, &leftDay) ||
        !ParseDate(right, &rightYear, &rightMonth, &rightDay)) return 0;
    if (leftYear != rightYear) return leftYear > rightYear ? 1 : -1;
    if (leftMonth != rightMonth) return leftMonth > rightMonth ? 1 : -1;
    if (leftDay != rightDay) return leftDay > rightDay ? 1 : -1;
    return 0;
}

BOOL Main_ShouldRunStartupUpdateCheck(char* today, size_t todaySize) {
    char configPath[MAX_PATH] = {0};
    char lastDate[DATE_BUFFER_SIZE] = {0};
    char lastVersion[64] = {0};
    if (!FormatLocalDate(today, todaySize)) return TRUE;
    GetConfigPath(configPath, sizeof(configPath));
    if (!configPath[0]) return TRUE;

    ReadIniString(INI_SECTION_GENERAL, LAST_CHECK_DATE_KEY, "",
                  lastDate, sizeof(lastDate), configPath);
    ReadIniString(INI_SECTION_GENERAL, LAST_CHECK_VERSION_KEY, "",
                  lastVersion, sizeof(lastVersion), configPath);
    int year = 0, month = 0, day = 0;
    if (!ParseDate(lastDate, &year, &month, &day)) return TRUE;
    int comparison = CompareDates(lastDate, today);
    if (comparison > 0) {
        LOG_WARNING("Stored update check date is in the future: %s", lastDate);
        return TRUE;
    }
    return strcmp(lastVersion, CATIME_VERSION) != 0 || comparison < 0;
}

void Main_MarkStartupUpdateCheckAttempt(const char* today) {
    if (!today || !*today) return;
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, sizeof(configPath));
    if (!configPath[0]) return;
    IniKeyValue updates[] = {
        {INI_SECTION_GENERAL, LAST_CHECK_DATE_KEY, today},
        {INI_SECTION_GENERAL, LAST_CHECK_VERSION_KEY, CATIME_VERSION}
    };
    if (!WriteIniMultipleAtomic(
            configPath, updates, _countof(updates))) {
        LOG_WARNING("Failed to record startup update check attempt");
    }
}
