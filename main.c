
#include "akira_api.h"
#include "filtration.h"
/* ---- BLE event buffer: 4 header + up to 64 data bytes ---- */
#define EVT_BUF_LEN 68

#define FLEX1_CH 3
#define FLEX2_CH 4
//--------------------------------------
#define SIZE 8
int saturation(int value, int min, int max)
{
    int result = min;
    if (value < min)
    {
        result = min;
    }
    if (value > max)
    {
        result = max;
    }
    else
    {
        result = value;
    }
    return result;
}

int moving_average(int new_value)
{
    static int buffer[SIZE] = {0};
    static int index = 0;
    static int sum = 0;
    sum -= buffer[index];//отнимаем от всей суммы старое значение
    buffer[index] = new_value;
    sum += new_value;

    index = (index + 1) % SIZE;//делает буфер кольцевым

    return sum / SIZE;//возвращает среднее
}
//--------------------------------------------------
int main(void)
{
    int32_t sample;
    int adc_ok;
    int saturation_flex = 0;
    int filtered_flex = 0;

    ble_set_local_name("AkiraOS_Mouse");

    /* ---- Start BLE (lazy-inits the BT stack in BLE_APP mode) ---- */
    int ret = ble_init();
    if (ret < 0) {
        printf("ble: init failed (%d) — HID mode active?");
        return ret;
    }

    ble_advertise();
    printf("BLE mouse advertising...");

    int hid_ok = hid_init(HID_TRANSPORT_BLE, HID_DEVICE_MOUSE);

    if (hid_ok < 0) {
        printf("hid init failed");
    }

    /* ---- 6. Main event loop ---- */
    uint8_t evt_buf[EVT_BUF_LEN];

    while (1) {
        int evt = ble_event_pop(evt_buf, sizeof(evt_buf));

        if (evt == BLE_EVT_CONNECTED) {
            printf("ble_led: ble ok");

        } else if (evt == BLE_EVT_DISCONNECTED) {
            printf("akira_mouse: ble not ok — re-advertising");
            ble_advertise();
        }

        
        if (hid_is_connected()) {
            printf("akira_mouse: mouse ok");
            //hid_mouse_move(10, -5);
            //printf("flex1: %d  flex2: %d\n", flex1, flex2);
            delay(1000000);
            //hid_mouse_btn_press(0x01);
            //delay(50000);
            //hid_mouse_btn_release(0x01);
        }
        adc_ok = adc_read(0, 3, &sample);
        saturation_flex = saturation(sample , 0, 2000);
        filtered_flex = moving_average(saturation_flex);
        if (adc_ok == 0) {
            printf("ADC1 channel 3 value = %d\n", filtered_flex);
        } else {
            printf("ADC read error: %d\n", adc_ok);
        }
        /* Yield 10 ms to avoid spinning the CPU at 100% */
        delay(10000); /* 10 000 µs = 10 ms */
    }

    return 0;
}
