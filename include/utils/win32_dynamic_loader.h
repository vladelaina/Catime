#ifndef UTILS_WIN32_DYNAMIC_LOADER_H
#define UTILS_WIN32_DYNAMIC_LOADER_H

#include <string.h>
#include <windows.h>

#define CATIME_LOAD_PROC_ADDRESS(module, name, target) \
    do { \
        FARPROC catime_proc_address = GetProcAddress((module), (name)); \
        memcpy(&(target), &catime_proc_address, sizeof(target)); \
    } while (0)

#endif /* UTILS_WIN32_DYNAMIC_LOADER_H */
