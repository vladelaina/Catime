#ifndef MAIN_INITIALIZATION_INTERNAL_H
#define MAIN_INITIALIZATION_INTERNAL_H

#include <windows.h>
#include <stddef.h>

void Main_DropPrivileges(void);
void Main_ScheduleCiSmokeExit(HWND hwnd, UINT delayMs);
void Main_ScheduleStartupWindowRecovery(HWND hwnd, BOOL topmost);
BOOL Main_ShouldRunStartupUpdateCheck(char* today, size_t todaySize);
void Main_MarkStartupUpdateCheckAttempt(const char* today);

#endif
