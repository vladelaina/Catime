/**
 * @file plugin_process_tree.c
 * @brief Bounded process-tree and Job Object cleanup.
 */

#include "plugin_process_internal.h"

static BOOL GrowSnapshot(ProcessTreeEntry** entries, DWORD* capacity,
                         DWORD count, const ProcessTreeEntry* stackEntries) {
    if (!entries || !capacity || !*capacity ||
        *capacity > ((DWORD)~(DWORD)0) / 2) return FALSE;
    DWORD newCapacity = *capacity * 2;
    size_t size = (size_t)newCapacity * sizeof(**entries);
    if (size / sizeof(**entries) != newCapacity) return FALSE;
    ProcessTreeEntry* resized = *entries == stackEntries
        ? (ProcessTreeEntry*)malloc(size)
        : (ProcessTreeEntry*)realloc(*entries, size);
    if (!resized) return FALSE;
    if (*entries == stackEntries) {
        memcpy(resized, stackEntries, (size_t)count * sizeof(*resized));
    }
    *entries = resized;
    *capacity = newCapacity;
    return TRUE;
}

static BOOL HasVisited(const DWORD* visited, DWORD count, DWORD pid) {
    for (DWORD i = 0; visited && i < count; ++i) {
        if (visited[i] == pid) return TRUE;
    }
    return FALSE;
}

static void TerminateOne(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!process) return;
    TerminateProcess(process, 0);
    WaitForSingleObject(process, 500);
    CloseHandle(process);
}

static void TerminateSlowVisited(DWORD pid, int depth, DWORD* visited,
                                 DWORD visitedCount) {
    if (!pid || pid == GetCurrentProcessId() ||
        depth > PROCESS_TREE_MAX_DEPTH || HasVisited(visited, visitedCount, pid)) {
        return;
    }
    if (visitedCount < PROCESS_TREE_VISITED_CAPACITY) visited[visitedCount++] = pid;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = {0};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (entry.th32ParentProcessID == pid &&
                    entry.th32ProcessID != pid) {
                    TerminateSlowVisited(entry.th32ProcessID, depth + 1,
                                         visited, visitedCount);
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    TerminateOne(pid);
}

static void TerminateSnapshotVisited(const ProcessTreeEntry* entries,
                                     DWORD count, DWORD pid, int depth,
                                     DWORD* visited, DWORD visitedCount) {
    if (!pid || pid == GetCurrentProcessId() ||
        depth > PROCESS_TREE_MAX_DEPTH || HasVisited(visited, visitedCount, pid)) {
        return;
    }
    if (visitedCount < PROCESS_TREE_VISITED_CAPACITY) visited[visitedCount++] = pid;
    for (DWORD i = 0; i < count; ++i) {
        if (entries[i].parentProcessId == pid && entries[i].processId != pid) {
            TerminateSnapshotVisited(entries, count, entries[i].processId,
                                     depth + 1, visited, visitedCount);
        }
    }
    TerminateOne(pid);
}

void PluginProcess_TerminateTree(DWORD pid, int depth) {
    if (!pid || pid == GetCurrentProcessId() ||
        depth > PROCESS_TREE_MAX_DEPTH) return;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        TerminateOne(pid);
        return;
    }

    ProcessTreeEntry stackEntries[PROCESS_TREE_STACK_CAPACITY];
    ProcessTreeEntry* entries = stackEntries;
    DWORD capacity = PROCESS_TREE_STACK_CAPACITY;
    DWORD count = 0;
    BOOL complete = TRUE;
    PROCESSENTRY32W process = {0};
    process.dwSize = sizeof(process);
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (count >= capacity &&
                !GrowSnapshot(&entries, &capacity, count, stackEntries)) {
                complete = FALSE;
                break;
            }
            entries[count].processId = process.th32ProcessID;
            entries[count].parentProcessId = process.th32ParentProcessID;
            ++count;
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);

    DWORD visited[PROCESS_TREE_VISITED_CAPACITY] = {0};
    if (complete) TerminateSnapshotVisited(entries, count, pid, depth,
                                           visited, 0);
    else TerminateSlowVisited(pid, depth, visited, 0);
    if (entries != stackEntries) free(entries);
}

static BOOL GetJobListSize(DWORD processCount, size_t* size) {
    if (!size) return FALSE;
    size_t extra = processCount ? (size_t)processCount - 1 : 0;
    if (extra > (((size_t)-1) - sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)) /
                    sizeof(ULONG_PTR)) return FALSE;
    *size = sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
            extra * sizeof(ULONG_PTR);
    return *size <= (size_t)((DWORD)~(DWORD)0);
}

void PluginProcess_TerminateAllJobProcesses(void) {
    if (!g_pluginJob) return;
    JOBOBJECT_BASIC_PROCESS_ID_LIST countInfo = {0};
    DWORD returnLength = 0;
    QueryInformationJobObject(g_pluginJob, JobObjectBasicProcessIdList,
                              &countInfo, sizeof(countInfo), &returnLength);
    if (!countInfo.NumberOfAssignedProcesses) return;
    size_t size = 0;
    if (!GetJobListSize(countInfo.NumberOfAssignedProcesses, &size)) return;

    union {
        JOBOBJECT_BASIC_PROCESS_ID_LIST list;
        BYTE bytes[sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
                   ((JOB_PROCESS_STACK_CAPACITY - 1) * sizeof(ULONG_PTR))];
    } stackList;
    JOBOBJECT_BASIC_PROCESS_ID_LIST* list =
        countInfo.NumberOfAssignedProcesses <= JOB_PROCESS_STACK_CAPACITY
            ? (JOBOBJECT_BASIC_PROCESS_ID_LIST*)stackList.bytes
            : (JOBOBJECT_BASIC_PROCESS_ID_LIST*)malloc(size);
    if (!list) return;
    list->NumberOfAssignedProcesses = countInfo.NumberOfAssignedProcesses;
    list->NumberOfProcessIdsInList = 0;
    if (QueryInformationJobObject(g_pluginJob, JobObjectBasicProcessIdList,
                                  list, (DWORD)size, &returnLength)) {
        DWORD currentPid = GetCurrentProcessId();
        for (DWORD i = 0; i < list->NumberOfProcessIdsInList; ++i) {
            DWORD pid = (DWORD)list->ProcessIdList[i];
            if (pid && pid != currentPid) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (process) {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
        }
    }
    if (list != (JOBOBJECT_BASIC_PROCESS_ID_LIST*)stackList.bytes) free(list);
}
