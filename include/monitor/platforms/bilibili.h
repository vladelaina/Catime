#ifndef MONITOR_PLATFORM_BILIBILI_H
#define MONITOR_PLATFORM_BILIBILI_H

#include "monitor/monitor_types.h"

// Fetch value for Bilibili User (follower, following, etc.)
// config->param1: UID
// config->param2: key
long long Bilibili_FetchUserValue(const MonitorConfig* config);

// Fetch value for Bilibili Video (view, like, etc.)
// config->param1: BV ID (e.g. BV1xx411c7mD)
// config->param2: key
long long Bilibili_FetchVideoValue(const MonitorConfig* config);

// Get options based on type (User vs Video)
int Bilibili_GetOptions(MonitorPlatformType type, MonitorOption* outOptions, int maxCount);

#endif
