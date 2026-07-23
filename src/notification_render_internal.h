#ifndef NOTIFICATION_RENDER_INTERNAL_H
#define NOTIFICATION_RENDER_INTERNAL_H

#include "notification_internal.h"

BOOL EnsureNotificationTextMaskBuffer(HDC hdc, NotificationData* data,
                                      int width, int height, HDC* outMemDC);
BOOL EnsureNotificationPaintBuffer(HDC hdc, NotificationData* data,
                                   int width, int height, HDC* outMemDC);
void DrawNotificationTextWithCurrentColor(NotificationData* data, HDC memDC,
                                          void* destBits, int destStrideWidth,
                                          int destWidth, int destHeight,
                                          const wchar_t* text, RECT rect,
                                          HFONT font, DWORD flags);

#endif
