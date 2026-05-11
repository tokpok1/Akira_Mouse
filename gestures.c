#include "gestures.h"
#include "utils.h"
#include "akira_api_wrapper.h"

#define COOLDOWN_TICKS 50
#define X_ACCEL_THRESHOLD 7000
#define Y_ACCEL_THRESHOLD 10000
#define FLEX_SUM_MIN 6400
#define FLEX_SUM_MAX 7548
#define HID_KEY_LEFT_ARROW  0x50
#define HID_KEY_RIGHT_ARROW 0x4F
#define HID_KEY_D           0x07
#define HID_KEY_SPACE       0x2C
#define HID_MOD_LEFT_GUI    0x08

#define DEADZONE_X 25
#define DEADZONE_Y 30
#define ALPHA 5
#define SENSITIVITY_NUM 2200// логика целых чисел можно подобрать
#define SENSITIVITY_DEN 100000

static int filt_x = 0;
static int filt_y = 0;
static int acc_x = 0;
static int acc_y = 0;

//фильтрация гироскопа и преобразование в пиксели
void process_motion(int gx, int gy, int *out_dx, int *out_dy)
{
    if (myAbs(gx) < DEADZONE_X) gx = 0;
    if (myAbs(gy) < DEADZONE_Y) gy = 0;

    filt_x += (gx - filt_x) / ALPHA; // low pass
    filt_y += (gy - filt_y) / ALPHA;

    acc_x += (-filt_y) * SENSITIVITY_NUM;
    acc_y += (-filt_x) * SENSITIVITY_NUM;

    int dx = acc_x / SENSITIVITY_DEN;
    int dy = acc_y / SENSITIVITY_DEN;
    //здесь именно int логика потому что пиксели целые 
    acc_x -= dx * SENSITIVITY_DEN;// накапливаем остаток для точности 
    acc_y -= dy * SENSITIVITY_DEN;

    if (out_dx) *out_dx = dx;
    if (out_dy) *out_dy = dy;
}

int gestures_init()
{
    //инициализация HID
    int hid_ok = hid_init(HID_TRANSPORT_BLE, HID_DEVICE_COMBO);
    if (hid_ok < 0) {
        printf("hid init failed");
        return hid_ok;
    }
    //жесты
    hid_action_register("minimize", HID_MOD_LEFT_GUI, HID_KEY_D);
    hid_action_register("win_left",  HID_MOD_LEFT_GUI, HID_KEY_LEFT_ARROW);
    hid_action_register("win_right", HID_MOD_LEFT_GUI, HID_KEY_RIGHT_ARROW);
    hid_action_register("change_language", HID_MOD_LEFT_GUI, HID_KEY_SPACE);
    
    return 0;
}

void gestures_handle_accel(int ax, int ay, int gx, int gy, int *cooldown)
{
    static float filterd_ax = 0.0f;
    static float filterd_ay = 0.0f;

    filterd_ax = lowPass(filterd_ax, (float)ax, 0.2f);
    filterd_ay = lowPass(filterd_ay, (float)ay, 0.2f);

    if (*cooldown > 0)
    {
        (*cooldown)--;
    }

    if (myAbs(filterd_ax) > X_ACCEL_THRESHOLD && *cooldown == 0)
    {
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
        *cooldown = COOLDOWN_TICKS;
    }
    
    if(myAbs(filterd_ay) > Y_ACCEL_THRESHOLD && *cooldown == 0)
    {
        if(gy > 0)
        {
            printf("вниз\n");
            hid_action_trigger("change_language");
        }
        else if(gy < 0)
        {
            printf("вверх\n");
            hid_action_trigger("minimize");
        }
        *cooldown = COOLDOWN_TICKS;
    }
}


bool gestures_handle_scroll(int flexSum, int dy, int r_state, int l_state,
                           int isConnectedL, int isConnectedR)
{
    int scrollMode = 0;
    int scroll_value = 0;

    if (isConnectedL && isConnectedR)
    {
        if (r_state)
        {
            scrollMode = 1;
            scroll_value = (FLEX_SUM_MAX - flexSum) * 254 / (FLEX_SUM_MAX - FLEX_SUM_MIN) - 127;
        }
    }
    else
    {
        if (r_state && l_state)
        {
            scrollMode = 1;
            scroll_value = dy;
        }
    }

    if (scrollMode)
    {
        hid_mouse_scroll(scroll_value);
        return true;
    }
    
    return false;
}
