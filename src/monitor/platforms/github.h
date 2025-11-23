#ifndef MONITOR_PLATFORM_GITHUB_H
#define MONITOR_PLATFORM_GITHUB_H

#include "monitor/monitor_types.h"

/**
 * @brief Fetch data from GitHub API
 * @param config Configuration object containing repo info
 * @return Raw value, -1 on failure
 */
long long GitHub_FetchValue(const MonitorConfig* config);

#endif
