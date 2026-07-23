/**
 * @file system_monitor_network_api.c
 * @brief Dynamic 64-bit network counters with a validated 32-bit fallback.
 */

#include "system_monitor_internal.h"
#include "utils/win32_dynamic_loader.h"

typedef NETIO_STATUS (WINAPI *GetIfTable2Fn)(PMIB_IF_TABLE2* table);
typedef VOID (WINAPI *FreeMibTableFn)(PVOID memory);

static HMODULE s_iphlpapiModule = NULL;
static GetIfTable2Fn s_getIfTable2 = NULL;
static FreeMibTableFn s_freeMibTable = NULL;
static BOOL s_apiResolved = FALSE;
static BOOL s_moduleLoadedByUs = FALSE;

static BOOL ResolveNetworkApi64(void) {
    if (s_apiResolved) return s_getIfTable2 && s_freeMibTable;
    s_apiResolved = TRUE;
    s_iphlpapiModule = GetModuleHandleW(L"iphlpapi.dll");
    if (!s_iphlpapiModule) {
        s_iphlpapiModule = LoadLibraryW(L"iphlpapi.dll");
        s_moduleLoadedByUs = s_iphlpapiModule != NULL;
    }
    if (!s_iphlpapiModule) return FALSE;
    CATIME_LOAD_PROC_ADDRESS(s_iphlpapiModule, "GetIfTable2", s_getIfTable2);
    CATIME_LOAD_PROC_ADDRESS(s_iphlpapiModule, "FreeMibTable", s_freeMibTable);
    return s_getIfTable2 && s_freeMibTable;
}

void Monitor_ReleaseNetworkApiResources(void) {
    AcquireSRWLockExclusive(&g_networkApiLock);
    s_getIfTable2 = NULL;
    s_freeMibTable = NULL;
    s_apiResolved = FALSE;
    if (s_moduleLoadedByUs && s_iphlpapiModule) {
        FreeLibrary(s_iphlpapiModule);
    }
    s_iphlpapiModule = NULL;
    s_moduleLoadedByUs = FALSE;
    ReleaseSRWLockExclusive(&g_networkApiLock);
}

static BOOL CollectCounters64(NetInterfaceCounter* counters, DWORD* outCount) {
    if (!counters || !outCount) return FALSE;
    *outCount = 0;
    BOOL success = FALSE;

    AcquireSRWLockExclusive(&g_networkApiLock);
    do {
        if (Monitor_IsInitialized() == 0 || !ResolveNetworkApi64()) break;
        PMIB_IF_TABLE2 table = NULL;
        if (s_getIfTable2(&table) != NO_ERROR || !table) break;

        DWORD count = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (Monitor_IsInitialized() == 0) break;
            const MIB_IF_ROW2* row = &table->Table[i];
            if (row->Type == MONITOR_LOOPBACK_INTERFACE_TYPE ||
                row->OperStatus != IfOperStatusUp) {
                continue;
            }
            if (count < MONITOR_MAX_TRACKED_INTERFACES) {
                counters[count].index = row->InterfaceIndex;
                counters[count].inOctets = row->InOctets;
                counters[count].outOctets = row->OutOctets;
                ++count;
            }
        }
        s_freeMibTable(table);
        *outCount = count;
        success = TRUE;
    } while (0);
    ReleaseSRWLockExclusive(&g_networkApiLock);
    return success;
}

static BOOL CollectCounters32(NetInterfaceCounter* counters, DWORD* outCount) {
    if (!counters || !outCount || Monitor_IsInitialized() == 0) return FALSE;
    *outCount = 0;

    DWORD size = 0;
    if (GetIfTable(NULL, &size, TRUE) != ERROR_INSUFFICIENT_BUFFER ||
        size == 0 || size > MONITOR_MAX_IF_TABLE_BYTES) {
        return FALSE;
    }
    MIB_IFTABLE* table = (MIB_IFTABLE*)malloc(size);
    if (!table) return FALSE;
    if (Monitor_IsInitialized() == 0) {
        free(table);
        return FALSE;
    }
    DWORD result = GetIfTable(table, &size, TRUE);
    if (result != NO_ERROR || size > MONITOR_MAX_IF_TABLE_BYTES ||
        size < (DWORD)FIELD_OFFSET(MIB_IFTABLE, table)) {
        free(table);
        return FALSE;
    }

    DWORD maxRows = (size - (DWORD)FIELD_OFFSET(MIB_IFTABLE, table)) /
                    sizeof(MIB_IFROW);
    DWORD rows = table->dwNumEntries < maxRows ? table->dwNumEntries : maxRows;
    DWORD count = 0;
    for (DWORD i = 0; i < rows; ++i) {
        if (Monitor_IsInitialized() == 0) {
            free(table);
            return FALSE;
        }
        const MIB_IFROW* row = &table->table[i];
        if (row->dwType == MONITOR_LOOPBACK_INTERFACE_TYPE) continue;
#ifdef IF_OPER_STATUS_OPERATIONAL
        if (row->dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;
#endif
        if (count < MONITOR_MAX_TRACKED_INTERFACES) {
            counters[count].index = row->dwIndex;
            counters[count].inOctets = row->dwInOctets;
            counters[count].outOctets = row->dwOutOctets;
            ++count;
        }
    }
    free(table);
    *outCount = count;
    return TRUE;
}

BOOL Monitor_CollectNetworkCounters(NetInterfaceCounter* counters,
                                    DWORD* count, BOOL* is64Bit) {
    if (!counters || !count || !is64Bit || Monitor_IsInitialized() == 0) {
        return FALSE;
    }
    if (CollectCounters64(counters, count)) {
        *is64Bit = TRUE;
        return TRUE;
    }
    if (Monitor_IsInitialized() == 0) return FALSE;
    *is64Bit = FALSE;
    return CollectCounters32(counters, count);
}
