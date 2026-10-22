/**
 * @file window.h
 * @brief The transparent always-on-top countdown window (GTK3).
 */
#ifndef CATIME_LINUX_WINDOW_H
#define CATIME_LINUX_WINDOW_H

void window_create(void);

void window_invalidate(void);   /* queue a redraw */
void window_update_size(void);  /* recompute size from font/scale */
void window_apply_config(void); /* position, topmost, opacity, click-through */

void window_show(void);
void window_hide(void);
void window_toggle_visibility(void);
int  window_get_visible(void);

void window_set_edit_mode(int on);
int  window_get_edit_mode(void);
void window_set_topmost(int on);

void window_save_position(void);

#endif /* CATIME_LINUX_WINDOW_H */
