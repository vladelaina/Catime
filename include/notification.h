#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"

extern int NOTIFICATION_TIMEOUT_MS;

void ShowNotification(HWND hwnd, const wchar_t* message);

void ShowToastNotification(HWND hwnd, const wchar_t* message);

void ShowModalNotification(HWND hwnd, const wchar_t* message);

void CloseAllNotifications(void);

#endif