#include "input.h"
#include "akira_api_wrapper.h"

const int CLICK_ON  = 3450;  // согнулся → клик
const int CLICK_OFF = 3600;  // выпрямился → отпускание
const int FLEX_SUM_MIN = 6400;
const int FLEX_SUM_MAX = 7548;

int last_state[2] = {-1, -1};

void flexClick(int value, int id, uint8_t btn)
{
    static int prev[2] = {0, 0};

    int now = (value < CLICK_ON);

    // переход 0 -> 1 (начало нажатия)
    if (now && !prev[id])
    {
        hid_mouse_btn_press(btn);
        prev[id] = 1;
    }

    // переход 1 -> 0 (отпускание)
    if (value > CLICK_OFF && prev[id])
    {
        hid_mouse_btn_release(btn);
        prev[id] = 0;
    }
}

bool flexConnected(int adc_value) {
  return adc_value > 1000;
}

void handle_flex_or_button(int flex_value, int gpio_state, int button_index, uint8_t button_id)
{
    if (flexConnected(flex_value))
    {
        flexClick(flex_value, button_index, button_id);
    }
    else
    {
        handle_button_state(gpio_state, button_index, button_id);
    }
}

void handle_button_state(int state, int button_index, uint8_t button_id)
{
    if (state < 0) {
        printf("Failed to read GPIO input!\n");
        return;
    }

    if (state != last_state[button_index]) {
        if (state == 1) {
            hid_mouse_btn_press(button_id);
        } else {
            hid_mouse_btn_release(button_id);
        }

        last_state[button_index] = state;
    }
}
