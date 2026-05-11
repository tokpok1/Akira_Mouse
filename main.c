
#include "akira_api.h"
#include <stdbool.h>
#include "utils.h"
#include "input.h"
#include "gestures.h"

#define EVT_BUF_LEN 68

#define FLEX1_CH 3 //pin14 делитель напряжения на gnd
#define FLEX2_CH 1//pin12
#define R_BUTTON 17// подключаем к 3,3 вольта, так что HIGH будет 1, а LOW - 0  
#define L_BUTTON 15
#define SDA_PIN 1
#define SCL_PIN 2
//--------------------------------------
#define SIZE 8

#define RIGHT 0
#define LEFT  1

#define LOOP_US 10000// частота 100 Гц, 10000 мкс
#define SAMPLES 80

const int MOUSE_PIXEL_MAX = 127;
const int MOUSE_PIXEL_MIN = -127;

int main(void)
{
    int32_t flex_pin1;
    int32_t flex_pin2;
    int adc_ch3_ok;
    int adc_ch1_ok;
    int Rbutton_ok;
    int Lbutton_ok;
    int saturation_flex1 = 0;
    int saturation_flex2 = 0;

    static float filtered_flex1 = 0.0f;
    static float filtered_flex2 = 0.0f;
    static int cooldown = 0;

    bool filter_initialized = false;
    bool isConnectedL = false;
    bool isConnectedR = false;
   
    int bias_gx = 0;
    int bias_gy = 0;
    int bias_ax = 0; 
    int bias_ay = 0;

    ble_set_local_name("AkiraOS_Mouse");

    int hid_ok = gestures_init();
    if (hid_ok < 0) {
        return hid_ok;
    }
    
    /* ---- GPIO пины ---- */
    Rbutton_ok = gpio_configure(R_BUTTON, GPIO_INPUT | GPIO_PULL_DOWN);
    if (Rbutton_ok < 0) 
    {
        printf("Failed to configure GPIO input pin!");
        return Rbutton_ok;
    }
    printf("GPIO %d configured successfully", R_BUTTON);

    Lbutton_ok = gpio_configure(L_BUTTON, GPIO_INPUT | GPIO_PULL_DOWN);
    if (Lbutton_ok < 0) 
    {
        printf("Failed to configure GPIO input pin!");
        return Lbutton_ok;
    }
    printf("GPIO %d configured successfully", L_BUTTON);
   
    /* Калибровка датчиков */
    for (int i = 0; i < SAMPLES; i++) 
    {
        int gx_calibration = sensor_read(SENSOR_CHAN_GYRO_X);
        int gy_calibration = sensor_read(SENSOR_CHAN_GYRO_Y);
        int ax_calibration = sensor_read(SENSOR_CHAN_ACCEL_X);
        int ay_calibration = sensor_read(SENSOR_CHAN_ACCEL_Y);
        if (gx_calibration == AKIRA_SENSOR_ERROR ||
            gy_calibration == AKIRA_SENSOR_ERROR ||
            ax_calibration == AKIRA_SENSOR_ERROR ||
            ay_calibration == AKIRA_SENSOR_ERROR)
        {
            printf("sensor not ready");
            return -1;
        }
        bias_gx += gx_calibration;
        bias_gy += gy_calibration;
        bias_ax += ax_calibration; 
        bias_ay += ay_calibration;
        delay(LOOP_US);
    }
    bias_gx /= SAMPLES;
    bias_gy /= SAMPLES;
    bias_ax /= SAMPLES; 
    bias_ay /= SAMPLES;

    while (1) 
    {

        if (hid_is_connected()) 
        {
            //читаем состояние кнопок и каналов adc для флексов
            int r_state = gpio_read(R_BUTTON);
            int l_state = gpio_read(L_BUTTON);

            adc_ch3_ok = adc_read(1, 3, &flex_pin1);
            if (adc_ch3_ok < 0)
            {
                printf("ADC read error ch3: %d\n", adc_ch3_ok);
                /* fallback: reuse previous filtered value to avoid jumps */
                saturation_flex1 = (int)filtered_flex1;
            }
            else
            {
                saturation_flex1 = saturation(flex_pin1, 0, 4000);
            }

            adc_ch1_ok = adc_read(1, 1, &flex_pin2);
            if (adc_ch1_ok < 0)
            {
                printf("ADC read error ch1: %d\n", adc_ch1_ok);
                /* fallback: reuse previous filtered value to avoid jumps */
                saturation_flex2 = (int)filtered_flex2;
            }
            else
            {
                saturation_flex2 = saturation(flex_pin2, 0, 4000);
            }
            if (!filter_initialized) 
            {
                filtered_flex1 = saturation_flex1; 
                filtered_flex2 = saturation_flex2;
                filter_initialized = true;
            } else 
            {
                //float для более плавного фильтра, можно юзать int, но тогда нужно будет подбирать коэффициент alpha
                filtered_flex1 = lowPass(filtered_flex1, (float)saturation_flex1, 0.2f);
                filtered_flex2 = lowPass(filtered_flex2, (float)saturation_flex2, 0.2f);
            }
            int output1 = (int)filtered_flex1;//меняем тип для удобства работы с порогами и суммой
            int output2 = (int)filtered_flex2;
            
            //подключены ли флексы, если подключены adc>1000
            isConnectedL = flexConnected(output1);
            isConnectedR = flexConnected(output2);
            handle_flex_or_button(output1, l_state, LEFT, 0x01);
            handle_flex_or_button(output2, r_state, RIGHT, 0x02);

            //берем значения гироскопа и преобразуем в пиксели для перемещения курсора
            int gx = sensor_read(SENSOR_CHAN_GYRO_X);
            int gy = sensor_read(SENSOR_CHAN_GYRO_Y);
            
            if (gx == AKIRA_SENSOR_ERROR || gy == AKIRA_SENSOR_ERROR)
            {
                printf("gyro not ready");
                gx = 0;
                gy = 0;
            }  
    
            gx -= bias_gx;
            gy -= bias_gy;

        
            int dx = 0;
            int dy = 0;
            process_motion(gx, gy, &dx, &dy);
            
            //берем акселерацию для жестов
            int ax = sensor_read(SENSOR_CHAN_ACCEL_X);
            int ay = sensor_read(SENSOR_CHAN_ACCEL_Y);
            if (ax == AKIRA_SENSOR_ERROR || ay == AKIRA_SENSOR_ERROR)
            {
                printf("accel not ready");
                ax = 0;
                ay = 0;
            } 

            ax -= bias_ax;
            ay -= bias_ay;
            int sum = output1 + output2;
            /* Обработка жестов и скролла */
            gestures_handle_accel(ax, ay, gx, gy, &cooldown);
            bool scrollMode = gestures_handle_scroll(sum, dy, r_state, l_state, isConnectedL, isConnectedR);

            if (!scrollMode)
            {
                hid_mouse_move(saturation(dx, MOUSE_PIXEL_MIN, MOUSE_PIXEL_MAX), 
                              saturation(dy, MOUSE_PIXEL_MIN, MOUSE_PIXEL_MAX));
            }
        }
        
        delay(LOOP_US);//частота 100 Гц
    }
    
    return 0;
}
