#include "debug_test.h"
#include "motor.h"
#include "pid.h"

int8_t imu_init = 0;

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
    encoder_dir_init(ENCODER_1, ENCODER_1_LSB, ENCODER_1_DIR);
    encoder_dir_init(ENCODER_2, ENCODER_2_LSB, ENCODER_2_DIR);
	
	/* imu963ra初始化 */
	if(imu963ra_init()){imu_init = 0;}
	else{imu_init = 1;}

    /* TIM6中断初始化 */
    pit_ms_init(TIM6_PIT, 1);
	interrupt_set_priority(TIM6_IRQn,0);
	
	/* TIM2中断初始化（专门来读取imu）*/
	pit_ms_init(TIM2_PIT, 5);
	interrupt_set_priority(TIM2_IRQn,0);
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
        system_delay_ms(10);

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
    ips200_show_string(0,  32, "L_speed:");
    ips200_show_string(0,  64, "L_dist:");
    ips200_show_string(0,  96, "R_speed:");
    ips200_show_string(0, 128, "R_dist:");
    ips200_show_string(0, 160, "KEY1: back");

    while (1) {
        if (last_sL != speed_L) {
            ips200_show_string(80, 32, "     ");
            ips200_show_int(80, 32, speed_L, 5);
            last_sL = speed_L;
        }
        if (last_dL != distance_L) {
            ips200_show_string(80, 64, "        ");
            ips200_show_int(80, 64, distance_L, 8);
            last_dL = distance_L;
        }
        if (last_sR != speed_R) {
            ips200_show_string(80, 96, "     ");
            ips200_show_int(80, 96, speed_R, 5);
            last_sR = speed_R;
        }
        if (last_dR != distance_R) {
            ips200_show_string(80, 128, "        ");
            ips200_show_int(80, 128, distance_R, 8);
            last_dR = distance_R;
        }

        key_scanner();
        system_delay_ms(10);
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }

    menu_request_redraw();
}

/*
 *  速度环位置保持测试
 *  车记住当前位置，手动推车后自动回到原位
 *  用来在静止状态下调速度环 PID，KEY_1 退出
 */
void speed_hold_test(void)
{
    int32_t start_pos = (distance_L + distance_R) / 2;

    PID_t pos_pid = {
        .Kp = 3,  .Ki = 0,  .Kd = 3,
        .OutMax = 400,  .OutMin = -400,
    };

    speed_pid.ErrorInt = 0;
    speed_pid.Error0   = 0;
    speed_pid.Error1   = 0;

    ips200_clear();
    ips200_show_string(0,  0, "Speed Hold Test");
    ips200_show_string(0, 16, "err:       ");
    ips200_show_string(0, 32, "spd:       ");
    ips200_show_string(0, 48, "PWM:       ");
    ips200_show_string(0, 80, "Push car -> returns");
    ips200_show_string(0, 96, "KEY1: exit");

    int32_t last_err = 1, last_spd = 1, last_pwm = 1;

    while (1) {
        int32_t cur_pos = (distance_L + distance_R) / 2;
        int16_t cur_spd = (speed_L + speed_R) / 2;

        /* 位置环: 距离误差 → 目标速度 */
        pos_pid.Target = start_pos;
        pos_pid.Actual = cur_pos;
        PID_Update(&pos_pid);
        int16_t target_speed = (int16_t)pos_pid.Out;

        /* 速度环: 速度误差 → PWM (不用 base_speed, 靠积分自举) */
        speed_pid.Target = target_speed;
        speed_pid.Actual = cur_spd;
        PID_Update(&speed_pid);
        int32_t pwm_out = (int32_t)speed_pid.Out;

        motor_set_pwm(DIR_L, PWM_L, pwm_out);
        motor_set_pwm(DIR_R, PWM_R, pwm_out);

        int16_t err = (int16_t)(cur_pos - start_pos);
        if (err != last_err) {
            ips200_show_string(48, 16, "          ");
            ips200_show_int(48, 16, err, 8);
            last_err = err;
        }
        if (cur_spd != last_spd) {
            ips200_show_string(48, 32, "          ");
            ips200_show_int(48, 32, cur_spd, 8);
            last_spd = cur_spd;
        }
        if (pwm_out != last_pwm) {
            ips200_show_string(48, 48, "          ");
            ips200_show_int(48, 48, pwm_out, 8);
            last_pwm = pwm_out;
        }

        key_scanner();
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
        system_delay_ms(10);
    }

    motor_set_pwm(DIR_L, PWM_L, 0);
    motor_set_pwm(DIR_R, PWM_R, 0);
    menu_request_redraw();
}
