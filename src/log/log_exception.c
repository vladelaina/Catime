/**
 * @file log_exception.c
 * @brief Exception handling and signal processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <windows.h>
#include "../../include/log/log_exception.h"
#include "../../include/log/log_core.h"
#include "../../include/log.h"

static const SignalInfo SIGNAL_TABLE[] = {
    {SIGFPE,   "Floating point exception"},
    {SIGILL,   "Illegal instruction"},
    {SIGSEGV,  "Segmentation fault/memory access error"},
    {SIGTERM,  "Termination signal"},
    {SIGABRT,  "Abnormal termination/abort"},
    {SIGINT,   "User interrupt"},
};

/** Atomic flag prevents deadlock in crash handler */
static volatile LONG inCrashHandler = 0;

static const char* GetSignalDescription(int signal) {
    const size_t tableSize = sizeof(SIGNAL_TABLE) / sizeof(SIGNAL_TABLE[0]);
    
    for (size_t i = 0; i < tableSize; i++) {
        if (SIGNAL_TABLE[i].signal == signal) {
            return SIGNAL_TABLE[i].description;
        }
    }
    
    return "Unknown signal";
}

/**
 * Lock-free crash handler prevents deadlock
 * @note Skips critical section since crash may occur while holding it
 */
static void SignalHandler(int signal) {
    if (InterlockedExchange(&inCrashHandler, 1) != 0) {
        exit(signal);
    }
    
    const char* signalDesc = GetSignalDescription(signal);
    
    /** No critical section to avoid deadlock */
    FILE* logFile = GetLogFileHandle();
    if (logFile) {
        fprintf(logFile, "[FATAL] Fatal signal occurred: %s (signal number: %d)\n", 
                signalDesc, signal);
        fflush(logFile);
        fclose(logFile);
    }
    
    MessageBoxW(NULL, 
                L"The program encountered a serious error. Please check the log file for details.", 
                L"Fatal Error", 
                MB_ICONERROR | MB_OK);
    
    exit(signal);
}

void SetupExceptionHandler(void) {
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGINT, SignalHandler);
}

void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) {
        return;
    }
    
    LPWSTR messageBuffer = NULL;
    DWORD size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0, NULL);
    
    if (size > 0 && messageBuffer) {
        if (size >= 2 && messageBuffer[size-2] == L'\r' && messageBuffer[size-1] == L'\n') {
            messageBuffer[size-2] = L'\0';
        }
        
        WideCharToMultiByte(CP_UTF8, 0, messageBuffer, -1, buffer, bufferSize, NULL, NULL);
        LocalFree(messageBuffer);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "Unknown error (code: %lu)", errorCode);
    }
}

