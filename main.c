
#include "akira_api.h"
#include <stdbool.h>

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
#define COOLDOWN_TICKS 50 // количество тиков для кулдауна жестов, при 100 Гц это 0.5 секунды
#define DEADZONE_X 25
#define DEADZONE_Y 30
#define X_ACCEL_THRESHOLD 7000
#define Y_ACCEL_THRESHOLD 10000
#define ALPHA 5
// *0.022
#define SENSITIVITY_NUM 2200 //  1200..4000 
#define SENSITIVITY_DEN  100000     /* for 10ms loop and raw=rad/s*1000 */

#define HID_KEY_LEFT_ARROW  0x50
#define HID_KEY_RIGHT_ARROW 0x4F
#define HID_KEY_D           0x07
#define HID_KEY_SPACE       0x2C
#define HID_MOD_LEFT_GUI    0x08   // Windows

const int CLICK_ON  = 3450;  // согнулся → клик
const int CLICK_OFF = 3600;  // выпрямился → отпускание
const int FLEX_SUM_MIN = 6400;
const int FLEX_SUM_MAX = 7548;
const int MOUSE_PIXEL_MAX = 127;
const int MOUSE_PIXEL_MIN = -127;

int last_state[2] = {-1, -1};

static int myAbs(int value) {
    return value < 0 ? -value : value; 
}

static int saturation(int value, int min, int max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

int flexClick(int value, int id, uint8_t btn)
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
    return prev[id];
}

bool flexConnected(int adc_value) {
  return adc_value > 1000;
}

float lowPass(float prev, float current, float alpha) {
    return alpha * current + (1 - alpha) * prev;
}

void handle_button_state(int state, int button_index, uint8_t button_id)
{
    if (state < 0) {
        printf("Failed to read GPIO input!\n");
        return;
    }

    //printf("GPIO %d state: %d\n", button_index, state);

    if (state != last_state[button_index]) {
        if (state == 1) {
            //printf("Input HIGH detected\n");
            hid_mouse_btn_press(button_id);
        } else {
            //printf("Input LOW detected\n");
            hid_mouse_btn_release(button_id);
        }

        last_state[button_index] = state;
    }
}

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
    int r_flex_state = 0;
    int l_flex_state = 0;
    int scroll_value = 0;

    static float filtered_flex1 = 0.0f;
    static float filtered_flex2 = 0.0f;
    static float filterd_ax = 0.0f;
    static float filterd_ay = 0.0f;
    static int cooldown = 0;

    bool filter_initialized = false;
    bool isConnectedL = false;
    bool isConnectedR = false;
    bool scrollMode = false;
   
    int bias_gx = 0;
    int bias_gy = 0;
    int bias_ax = 0; 
    int bias_ay = 0;
    int filt_x = 0;
    int filt_y = 0;
    int acc_x = 0; 
    int acc_y = 0;

    ble_set_local_name("AkiraOS_Mouse");

    int hid_ok = hid_init(HID_TRANSPORT_BLE, HID_DEVICE_COMBO);
    if (hid_ok < 0) {
        printf("hid init failed");
    }
    hid_action_register("minimize", HID_MOD_LEFT_GUI, HID_KEY_D);
    hid_action_register("win_left",  HID_MOD_LEFT_GUI, HID_KEY_LEFT_ARROW);
    hid_action_register("win_right", HID_MOD_LEFT_GUI, HID_KEY_RIGHT_ARROW);
    hid_action_register("change_language", HID_MOD_LEFT_GUI, HID_KEY_SPACE);
    /* ---- 6. Main event loop ---- */
    uint8_t evt_buf[EVT_BUF_LEN];
    // настройка пинов -----------------------------------
    Rbutton_ok = gpio_configure(R_BUTTON, GPIO_INPUT | GPIO_PULL_DOWN);
    if (Rbutton_ok < 0) 
    {
        printf( "Failed to configure GPIO input pin!");
        return Rbutton_ok;
    }
    printf( "GPIO 15 configured successfully");

    Lbutton_ok = gpio_configure(L_BUTTON, GPIO_INPUT | GPIO_PULL_DOWN);
    if (Lbutton_ok < 0) 
    {
        printf( "Failed to configure GPIO input pin!");
        return Lbutton_ok;
    }
    printf( "GPIO 17 configured successfully");
   
    
    //добываем смещение ------------------------------------
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
            if(adc_ch3_ok < 0)
            {
                printf("ADC read error: %d\n", adc_ch3_ok);
            }
            adc_ch1_ok = adc_read(1, 1, &flex_pin2);
            if(adc_ch1_ok < 0)
            {
                printf("ADC read error: %d\n", adc_ch1_ok);
            }
            //ограничиваем, фильтруем значения флексов
            saturation_flex1 = saturation(flex_pin1 , 0, 4000);
            saturation_flex2 = saturation(flex_pin2 , 0, 4000);
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
            int flexSum = output1 + output2;
            //подключены ли флексы, если подключены adc>1000
            isConnectedL = flexConnected(output1);
            isConnectedR = flexConnected(output2);
            //если флекс подключен клик можно делать только по нему, если нет - fallback на кнопки
            if (isConnectedL) 
            {
                l_flex_state = flexClick(output1, LEFT,  0x01);
            }
            else 
            {
                handle_button_state(l_state, LEFT, 0x01);
            }
            if (isConnectedR) 
            {
                r_flex_state = flexClick(output2, RIGHT, 0x02);
            }
            else 
            {
                handle_button_state(r_state, RIGHT, 0x02);
            }

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

            if (myAbs(gx) < DEADZONE_X) gx = 0;
            if (myAbs(gy) < DEADZONE_Y) gy = 0;

            filt_x += (gx - filt_x) / ALPHA; //low pass filter
            filt_y += (gy - filt_y) / ALPHA;
        
            //масштабирование x, меняем оси  и инвертируем y для удобства использования в качестве мыши
            acc_x += (-filt_y) * SENSITIVITY_NUM;
            acc_y += (-filt_x) * SENSITIVITY_NUM;

            int dx = acc_x / SENSITIVITY_DEN;
            int dy = acc_y / SENSITIVITY_DEN;

            acc_x -= dx * SENSITIVITY_DEN; //сохраняем остаток чтобы не терять точность при малых движениях
            acc_y -= dy * SENSITIVITY_DEN;//фильтр такой потому что пиксели int
            
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

            filterd_ax = lowPass(filterd_ax, (float)ax, 0.2f);
            filterd_ay = lowPass(filterd_ay, (float)ay, 0.2f);

            if (cooldown > 0)//чтобы не срабатывать на каждое чтение при превышении порога
            {
                cooldown--;
            }

            if (myAbs(filterd_ax) > X_ACCEL_THRESHOLD && cooldown == 0)
            {
                // направление от гироскопа
                if (gx > 0)
                {
                    printf("вправо\n");
                    hid_action_trigger("win_right");

                }
                else if (gx < 0)
                {
                    printf("влево\n");
                    hid_action_trigger("win_left");
                }
                cooldown = COOLDOWN_TICKS;//жест раз в 50 тиков, при частоте 100 Гц это 0.5 секунды
            }
            if(myAbs(filterd_ay) > Y_ACCEL_THRESHOLD && cooldown == 0)
            {
                if(gy > 0)// берем знак гироскопа он стабильнее
                {
                    printf("вниз\n");
                    hid_action_trigger("change_language");
                }
                else if(gy < 0)
                {
                    printf("вверх\n");
                    hid_action_trigger("minimize");
                }
                cooldown = COOLDOWN_TICKS;
            }

            scrollMode = false;
            scroll_value = 0;
            //если оба флекса подключены, то скроллим по сумме
            //если нет - скроллим по нажатию двух кнопкок и движению по y
            if (isConnectedL && isConnectedR)
            {
                if (r_state)
                {
                    scrollMode = true;
                    scroll_value = (FLEX_SUM_MAX - flexSum) * 254 / (FLEX_SUM_MAX - FLEX_SUM_MIN) - 127;
                }
            }
            else
            {
                if (r_state && l_state)
                {
                    scrollMode = true;
                    scroll_value = dy;
                }
            }
            if (scrollMode)
            {
                hid_mouse_scroll(scroll_value);
            }
            else
            {

                hid_mouse_move(saturation(dx, MOUSE_PIXEL_MIN, MOUSE_PIXEL_MAX), saturation(dy, MOUSE_PIXEL_MIN, MOUSE_PIXEL_MAX));
                
            }

        }
        
        delay(LOOP_US);//частота 100 Гц
    }
    
    return 0;
}
