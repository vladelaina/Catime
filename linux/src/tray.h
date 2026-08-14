/**
 * @file tray.h
 * @brief System tray (AppIndicator) interface.
 */
#ifndef CATIME_LINUX_TRAY_H
#define CATIME_LINUX_TRAY_H

void tray_create(void);
void tray_update(void);
void tray_set_edit_active(int on);

#endif /* CATIME_LINUX_TRAY_H */
