/**
 * @file tray_icon_lifetime.c
 * @brief Lifetime management for icons submitted to Explorer
 */

#include "tray/tray_icon_lifetime.h"

static SRWLOCK g_submittedIconLock = SRWLOCK_INIT;
/* Explorer may finish painting a submitted icon after Shell_NotifyIconW
 * returns.  Retaining the current and previous generations gives it at
 * least one complete replacement interval without allowing handle growth. */
static HICON g_submittedIconCurrent = NULL;
static HICON g_submittedIconPrevious = NULL;

void TrayIconLifetime_Retain(HICON icon) {
    if (!icon) return;

    AcquireSRWLockExclusive(&g_submittedIconLock);
    HICON iconToDestroy = NULL;
    if (icon == g_submittedIconCurrent) {
        ReleaseSRWLockExclusive(&g_submittedIconLock);
        return;
    }

    if (icon == g_submittedIconPrevious) {
        g_submittedIconPrevious = g_submittedIconCurrent;
        g_submittedIconCurrent = icon;
    } else {
        iconToDestroy = g_submittedIconPrevious;
        g_submittedIconPrevious = g_submittedIconCurrent;
        g_submittedIconCurrent = icon;
    }
    ReleaseSRWLockExclusive(&g_submittedIconLock);

    if (iconToDestroy) DestroyIcon(iconToDestroy);
}

void TrayIconLifetime_ReleaseAll(void) {
    AcquireSRWLockExclusive(&g_submittedIconLock);
    HICON current = g_submittedIconCurrent;
    HICON previous = g_submittedIconPrevious;
    g_submittedIconCurrent = NULL;
    g_submittedIconPrevious = NULL;
    ReleaseSRWLockExclusive(&g_submittedIconLock);

    if (current) DestroyIcon(current);
    if (previous && previous != current) DestroyIcon(previous);
}
