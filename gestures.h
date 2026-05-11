#ifndef GESTURES_H
#define GESTURES_H

#include <stdint.h>
#include <stdbool.h>

/* Инициализация HID и жестов */
int gestures_init(void);

/* Обработка жестов */
void gestures_handle_accel(int ax, int ay, int gx, int gy, int *cooldown);

/* Обработка скролла - возвращает true если скролл был активирован */
bool gestures_handle_scroll(int flexSum, int dy, int r_state, int l_state, 
                           int isConnectedL, int isConnectedR);

/* Обработка движения/гироскопа -> выдаёт смещение курсора */
void process_motion(int gx, int gy, int *out_dx, int *out_dy);

#endif // GESTURES_H
