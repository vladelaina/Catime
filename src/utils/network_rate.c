#include "utils/network_rate.h"

FormattedNetworkRate FormatNetworkBytesPerSecond(double bytesPerSecond) {
    FormattedNetworkRate result = {0.0, L"KB/s"};
    if (!(bytesPerSecond > 0.0)) return result;

    result.value = bytesPerSecond / 1024.0;
    if (result.value >= 1024.0) {
        result.value /= 1024.0;
        result.unit = L"MB/s";
    }
    if (result.value >= 1024.0) {
        result.value /= 1024.0;
        result.unit = L"GB/s";
    }
    return result;
}
