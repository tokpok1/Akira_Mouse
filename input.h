#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Пороги для флексов */
extern const int CLICK_ON;
extern const int CLICK_OFF;
extern const int FLEX_SUM_MIN;
extern const int FLEX_SUM_MAX;

/* Обработка flex сенсоров */
void flexClick(int value, int id, uint8_t btn);
bool flexConnected(int adc_value);
void handle_flex_or_button(int flex_value, int gpio_state, int button_index, uint8_t button_id);

/* Обработка кнопок */
extern int last_state[2];
void handle_button_state(int state, int button_index, uint8_t button_id);

#endif // INPUT_H
