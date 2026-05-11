#ifndef AKIRA_API_WRAPPER_H
#define AKIRA_API_WRAPPER_H

#include <stdint.h>

// для функций из akira_api.h 
extern int hid_init(int transport, int device_type);
extern int hid_mouse_btn_press(int32_t button);
extern int hid_mouse_btn_release(int32_t button);
extern int hid_action_register(const char *name, uint8_t modifiers, uint8_t key);
extern int hid_action_trigger(const char *action);
extern int hid_mouse_scroll(int value);
extern int printf(const char *fmt, ...);

//для инициализации BLE
#define HID_TRANSPORT_BLE  1
#define HID_DEVICE_COMBO    0x07

#endif // AKIRA_API_WRAPPER_H
