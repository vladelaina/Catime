#ifndef CATIME_UTILS_NETWORK_RATE_H
#define CATIME_UTILS_NETWORK_RATE_H

#include <wchar.h>

typedef struct {
    double value;
    const wchar_t* unit;
} FormattedNetworkRate;

/**
 * Scale a byte-per-second value using the units shown by File Explorer.
 * Network rates use KB/s as the minimum unit for a stable compact display.
 */
FormattedNetworkRate FormatNetworkBytesPerSecond(double bytesPerSecond);

#endif /* CATIME_UTILS_NETWORK_RATE_H */
