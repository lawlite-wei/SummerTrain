#include "debug_test.h"
#include "motor.h"

void HARDWARE_INIT(void)
{
    /* 初始化屏幕+按键+菜单链表+摄像头初始化 */
    menu_init();

    /* 电机PWM初始化（左右） */
    gpio_init(DIR_L, GPO, GPIO_HIGH, GPO_PUSH_PULL);
    pwm_init(PWM_L, 17000, 0);

    gpio_init(DIR_R, GPO, GPIO_HIGH, GPO_PUSH_PULL);
    pwm_init(PWM_R, 17000, 0);

    /* 编码器初始化 */
    encoder_quad_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B);
    encoder_quad_init(ENCODER_2, ENCODER_2_A, ENCODER_2_B);

    /* 中断初始化 */
    pit_ms_init(TIM6_PIT, 1);
}

/*
 *  电机PWM测试：按键实时调节左右电机PWM
 *  KEY_4: 左电机 +2    KEY_3: 左电机 -2
 *  KEY_2: 右电机 +2    KEY_1: 右电机 -2 / 长按退出
 */
void motor_test(void)
{
    int16 left_val  = 0;
    int16 right_val = 0;

    ips200_clear();
    ips200_show_string(0,  0, "Motor Test");
    ips200_show_string(0, 32, "L:     ");
    ips200_show_string(0, 48, "R:     ");
    ips200_show_string(0, 80, "K4:+L K3:-L");
    ips200_show_string(0, 96, "K2:+R K1:-R");
    ips200_show_string(0, 112, "Hold K1: exit");

    ips200_show_int(40, 32, left_val, 5);
    ips200_show_int(40, 48, right_val, 5);

    while (1) {
        key_scanner();

        if (key_get_state(KEY_4) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_4);
            left_val += 2;
            if (left_val > 10000) left_val = 10000;
            motor_set_pwm(DIR_L, PWM_L, left_val);
            ips200_show_string(40, 32, "     ");
            ips200_show_int(40, 32, left_val, 5);
        }
        if (key_get_state(KEY_3) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_3);
            left_val -= 2;
            if (left_val < 0) left_val = 0;
            motor_set_pwm(DIR_L, PWM_L, left_val);
            ips200_show_string(40, 32, "     ");
            ips200_show_int(40, 32, left_val, 5);
        }
        if (key_get_state(KEY_2) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_2);
            right_val += 2;
            if (right_val > 10000) right_val = 10000;
            motor_set_pwm(DIR_R, PWM_R, right_val);
            ips200_show_string(40, 48, "     ");
            ips200_show_int(40, 48, right_val, 5);
        }
        if (key_get_state(KEY_1) == KEY_LONG_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }

    /* 停止电机 */
    motor_set_pwm(DIR_L, PWM_L, 0);
    motor_set_pwm(DIR_R, PWM_R, 0);
    ips200_show_string(0, 144, "Motor stop, exit.");
}

void encoder_test(void)
{
    int16_t last_sL = 1, last_sR = 1;
    int32_t last_dL = 1, last_dR = 1;

    ips200_clear();
    ips200_show_string(0,   0, "--Encoder--");
    ips200_show_string(0,  32, "speed_L:");
    ips200_show_string(0,  48, "distance_L:");
    ips200_show_string(0,  64, "speed_R:");
    ips200_show_string(0,  80, "distance_R:");
    ips200_show_string(0, 112, "KEY1: back");

    while (1) {
        if (last_sL != speed_L) {
            ips200_show_string(104, 32, "     ");
            ips200_show_int(104, 32, speed_L, 5);
            last_sL = speed_L;
        }
        if (last_dL != distance_L) {
            ips200_show_string(104, 48, "        ");
            ips200_show_int(104, 48, distance_L, 8);
            last_dL = distance_L;
        }
        if (last_sR != speed_R) {
            ips200_show_string(104, 64, "     ");
            ips200_show_int(104, 64, speed_R, 5);
            last_sR = speed_R;
        }
        if (last_dR != distance_R) {
            ips200_show_string(104, 80, "        ");
            ips200_show_int(104, 80, distance_R, 8);
            last_dR = distance_R;
        }

        key_scanner();
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }

    menu_request_redraw();
}
