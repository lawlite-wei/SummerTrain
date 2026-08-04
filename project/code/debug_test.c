#include "debug_test.h"
#include <math.h>

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
 *  KEY_4(E5): 左电机 +2    KEY_3(E2): 左电机 -2
 *  KEY_2(E4): 右电机 +2    KEY_1(E3): 右电机 -2 / 长按退出
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

/*
 *  左右速度环保持测试（独立轮速控制 + 位置环回位）
 *  左右轮各自用位置环（距离误差→目标速度）驱动速度环
 *  推车后位置误差立刻产生回位力，效果和原 speed_hold_test 一致
 *  用来在静止状态下分别调左右速度环 Kp/Ki/Kd，KEY_1 退出
 *  调好的参数可直接用于寻迹中的 speed_pid_L / speed_pid_R
 */
void speed_hold_LR_test(void)
{
    int32_t start_pos_L = distance_L;
    int32_t start_pos_R = distance_R;

    /* 位置环：距离误差 → 目标速度（参数同 speed_hold_test） */
    PID_t pos_pid_L = {
        .Kp = 3,  .Ki = 0,  .Kd = 3,
        .OutMax = 400,  .OutMin = -400,
    };
    PID_t pos_pid_R = {
        .Kp = 3,  .Ki = 0,  .Kd = 3,
        .OutMax = 400,  .OutMin = -400,
    };

    /* 清除左右速度环状态 */
    speed_pid_L.ErrorInt = 0;
    speed_pid_L.Error0   = 0;
    speed_pid_L.Error1   = 0;
    speed_pid_R.ErrorInt = 0;
    speed_pid_R.Error0   = 0;
    speed_pid_R.Error1   = 0;

    ips200_clear();
    ips200_show_string(0,  0, "Speed Hold LR Test");
    ips200_show_string(0, 16, "L_pos:       ");
    ips200_show_string(0, 32, "L_spd:       ");
    ips200_show_string(0, 48, "L_PWM:       ");
    ips200_show_string(0, 64, "R_pos:       ");
    ips200_show_string(0, 80, "R_spd:       ");
    ips200_show_string(0, 96, "R_PWM:       ");
    ips200_show_string(0, 128, "Push car -> returns");
    ips200_show_string(0, 144, "KEY1: exit");

    int32 last_Lpos = 1, last_Lpwm = 1;
    int16 last_Lspd = 1;
    int32 last_Rpos = 1, last_Rpwm = 1;
    int16 last_Rspd = 1;

    while (1) {
        int32_t cur_pos_L = distance_L;
        int32_t cur_pos_R = distance_R;

        /* 左轮：位置环 → 目标速度 → 速度环 → PWM */
        pos_pid_L.Target = start_pos_L;
        pos_pid_L.Actual = cur_pos_L;
        PID_Update(&pos_pid_L);

        speed_pid_L.Target = pos_pid_L.Out;
        speed_pid_L.Actual = speed_L;
        PID_Update(&speed_pid_L);

        /* 右轮：位置环 → 目标速度 → 速度环 → PWM */
        pos_pid_R.Target = start_pos_R;
        pos_pid_R.Actual = cur_pos_R;
        PID_Update(&pos_pid_R);

        speed_pid_R.Target = pos_pid_R.Out;
        speed_pid_R.Actual = speed_R;
        PID_Update(&speed_pid_R);

        motor_set_pwm(DIR_L, PWM_L, (int32_t)speed_pid_L.Out);
        motor_set_pwm(DIR_R, PWM_R, (int32_t)speed_pid_R.Out);

        /* 显示更新 */
        int32_t err_L = cur_pos_L - start_pos_L;
        int32_t err_R = cur_pos_R - start_pos_R;

        if (err_L != last_Lpos) {
            ips200_show_string(72, 16, "          ");
            ips200_show_int(72, 16, err_L, 8);
            last_Lpos = err_L;
        }
        if (speed_L != last_Lspd) {
            ips200_show_string(72, 32, "          ");
            ips200_show_int(72, 32, speed_L, 8);
            last_Lspd = speed_L;
        }
        if ((int32_t)speed_pid_L.Out != last_Lpwm) {
            ips200_show_string(72, 48, "          ");
            ips200_show_int(72, 48, (int32_t)speed_pid_L.Out, 8);
            last_Lpwm = (int32_t)speed_pid_L.Out;
        }
        if (err_R != last_Rpos) {
            ips200_show_string(72, 64, "          ");
            ips200_show_int(72, 64, err_R, 8);
            last_Rpos = err_R;
        }
        if (speed_R != last_Rspd) {
            ips200_show_string(72, 80, "          ");
            ips200_show_int(72, 80, speed_R, 8);
            last_Rspd = speed_R;
        }
        if ((int32_t)speed_pid_R.Out != last_Rpwm) {
            ips200_show_string(72, 96, "          ");
            ips200_show_int(72, 96, (int32_t)speed_pid_R.Out, 8);
            last_Rpwm = (int32_t)speed_pid_R.Out;
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

/*
 *  编码器差速/陀螺仪 换算系数计算
 *  电机原地差速旋转，打印 ratio = (speed_L - speed_R) / real_gz
 *  跑多组取平均值，作为 SPEED_DIFF_GAIN 的参考值
 *  K4:+L K3:-L  K2:+R K1:exit
 */
void ratio_calc_test(void)
{
    if (!imu_init) {
        ips200_clear();
        ips200_show_string(0, 0, "IMU not init!");
        system_delay_ms(1000);
        menu_request_redraw();
        return;
    }

    int16 L_pwm =  2000;
    int16 R_pwm = -2000;
    float ratio_sum  = 0.0f;
    uint16 ratio_cnt = 0;

    motor_set_pwm(DIR_L, PWM_L, L_pwm);
    motor_set_pwm(DIR_R, PWM_R, R_pwm);

    ips200_clear();
    ips200_show_string(0,   0, "Ratio Calc Test");
    ips200_show_string(0,  16, "L_PWM:       R_PWM:");
    ips200_show_string(0,  32, "L_spd:       R_spd:");
    ips200_show_string(0,  48, "Diff:        ");
    ips200_show_string(0,  64, "gyro_z:      ");
    ips200_show_string(0,  80, "ratio:       ");
    ips200_show_string(0,  96, "avg:         n:");
    ips200_show_string(0, 128, "K4:+L K3:-L|K2:+R K1:exit");

    int16 last_Lpwm = 1, last_Rpwm = 1;
    int16 last_Lspd = 1, last_Rspd = 1;
    int16 last_diff = 1;
    int16 last_gz   = 1;
    int16 last_ratio_i = 1;
    int16 last_avg_i   = 1;

    while (1) {
        get_real_gz();

        int16 diff = speed_L - speed_R;
        float ratio = 0.0f;

        /* 陀螺仪有明显转动时才计算，避免静止时除零噪声 */
        if (fabsf(real_gz) > 0.5f && diff != 0) {
            ratio = (float)diff / real_gz;
            ratio_sum += ratio;
            ratio_cnt++;
        }

        float avg = (ratio_cnt > 0) ? (ratio_sum / ratio_cnt) : 0.0f;

        /* 显示 PWM */
        if (L_pwm != last_Lpwm || R_pwm != last_Rpwm) {
            ips200_show_string(48,  16, "             ");
            ips200_show_int(48,  16, L_pwm, 5);
            ips200_show_string(120, 16, "     ");
            ips200_show_int(120, 16, R_pwm, 5);
            last_Lpwm = L_pwm;
            last_Rpwm = R_pwm;
        }

        /* 显示编码器速度 */
        if (speed_L != last_Lspd) {
            ips200_show_string(48, 32, "     ");
            ips200_show_int(48, 32, speed_L, 5);
            last_Lspd = speed_L;
        }
        if (speed_R != last_Rspd) {
            ips200_show_string(120, 32, "     ");
            ips200_show_int(120, 32, speed_R, 5);
            last_Rspd = speed_R;
        }

        /* 显示差速 */
        if (diff != last_diff) {
            ips200_show_string(48, 48, "     ");
            ips200_show_int(48, 48, diff, 5);
            last_diff = diff;
        }

        /* 显示陀螺仪 */
        {
            int16 gz_disp = (int16)(real_gz * 100.0f);
            if (gz_disp != last_gz) {
                ips200_show_string(72, 64, "        ");
                ips200_show_float(72, 64, real_gz, 5, 2);
                last_gz = gz_disp;
            }
        }

        /* 显示当前 ratio */
        {
            int16 r_disp = (int16)(ratio * 1000.0f);
            if (r_disp != last_ratio_i) {
                ips200_show_string(48, 80, "        ");
                ips200_show_float(48, 80, ratio, 5, 3);
                last_ratio_i = r_disp;
            }
        }

        /* 显示平均值和采样数 */
        {
            int16 a_disp = (int16)(avg * 1000.0f);
            if (a_disp != last_avg_i) {
                ips200_show_string(48, 96, "            ");
                ips200_show_float(48, 96, avg, 5, 3);
                ips200_show_string(130, 96, "   ");
                ips200_show_int(130, 96, ratio_cnt, 3);
                last_avg_i = a_disp;
            }
        }

        /* 按键调整 PWM */
        key_scanner();
        if (key_get_state(KEY_4) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_4);
            L_pwm += 100;
            if (L_pwm > 10000) L_pwm = 10000;
            motor_set_pwm(DIR_L, PWM_L, L_pwm);
            /* 改 PWM 后清空历史数据重新采样 */
            ratio_sum  = 0.0f;
            ratio_cnt  = 0;
        }
        if (key_get_state(KEY_3) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_3);
            L_pwm -= 100;
            if (L_pwm < 0) L_pwm = 0;
            motor_set_pwm(DIR_L, PWM_L, L_pwm);
            ratio_sum  = 0.0f;
            ratio_cnt  = 0;
        }
        if (key_get_state(KEY_2) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_2);
            R_pwm += 100;
            if (R_pwm > 10000) R_pwm = 10000;
            motor_set_pwm(DIR_R, PWM_R, R_pwm);
            ratio_sum  = 0.0f;
            ratio_cnt  = 0;
        }
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

/*
 *  角速度环保持测试（含回位功能）
 *  目标角速度=0，推车后靠角速度环+速度环抵抗并回到原位
 *  信号链与寻迹一致: gz→编码器差速量纲→gyro_pid→左右速度环→PWM
 *  角速度环积分项 Ki 提供回位力（角速度误差的积分=角度偏差）
 *  KEY_1 退出，调好的参数可直接用于寻迹
 */
void gyro_hold_test(void)
{
    if (!imu_init) {
        ips200_clear();
        ips200_show_string(0, 0, "IMU not init!");
        system_delay_ms(1000);
        menu_request_redraw();
        return;
    }

    /* gz → 编码器差速换算系数（与 track.c 保持一致，实测标定值） */
    const float gz_to_encoder = 0.299f;

    /* ─── 清除所有环的积分和误差状态 ─── */
    gyro_pid.ErrorInt  = 0;
    gyro_pid.Error0    = 0;
    gyro_pid.Error1    = 0;
    speed_pid_L.ErrorInt = 0;
    speed_pid_L.Error0   = 0;
    speed_pid_L.Error1   = 0;
    speed_pid_R.ErrorInt = 0;
    speed_pid_R.Error0   = 0;
    speed_pid_R.Error1   = 0;

    ips200_clear();
    ips200_show_string(0,   0, "Gyro Hold Test");
    ips200_show_string(0,  16, "gz:         G_dis:");
    ips200_show_string(0,  32, "err:        Int:   ");
    ips200_show_string(0,  48, "turn:       ");
    ips200_show_string(0,  64, "L_PWM:      R_PWM:");
    ips200_show_string(0,  96, "Push car -> return");
    ips200_show_string(0, 112, "Ki=0仅抵抗, Ki>0可回位");
    ips200_show_string(0, 128, "KEY1: exit");

    int16 last_gz = 1, last_gdis = 1;
    int16 last_err = 1, last_int = 1;
    int32 last_turn = 1, last_lpwm = 1, last_rpwm = 1;

    static float G_dis_filt = 0.0f;  // G_dis 低通滤波后的值
    const float lpf_alpha = 0.33f;   // 低通系数(fc≈8Hz@10ms)，越大响应越快但噪声越多

    while (1) {
        get_real_gz();

        /* ─── 角速度环 ─── */
        float G_dis = (float)real_gz * gz_to_encoder;        // 原始换算值
        G_dis_filt += lpf_alpha * (G_dis - G_dis_filt);      // 一阶低通滤波，抑制陀螺仪噪声
        gyro_pid.Target = 0.0f;                               // 目标：零角速度（不转）
        gyro_pid.Actual = G_dis_filt;                         // 用滤波后的反馈
        PID_Update(&gyro_pid);
        int32_t turn_control = -(int32_t)gyro_pid.Out;  // 编码器差速修正量

        /* ─── 左右速度环（原地差速旋转，与寻迹结构一致）─── */
        speed_pid_L.Target = (float)turn_control;
        speed_pid_L.Actual = speed_L;
        PID_Update(&speed_pid_L);

        speed_pid_R.Target = (float)(-turn_control);
        speed_pid_R.Actual = speed_R;
        PID_Update(&speed_pid_R);

        motor_set_pwm(DIR_L, PWM_L, (int32_t)speed_pid_L.Out);
        motor_set_pwm(DIR_R, PWM_R, (int32_t)speed_pid_R.Out);
        motor_protect();

        /* ─── 显示更新 ─── */
        {
            int16 gz_disp  = (int16)(real_gz * 100.0f);
            int16 gdis_disp = (int16)(G_dis * 10.0f);
            int16 err_disp  = (int16)(gyro_pid.Error0 * 10.0f);
            int16 int_disp  = (int16)(gyro_pid.ErrorInt * 10.0f);

            if (gz_disp != last_gz) {
                ips200_show_string(32,  16, "          ");
                ips200_show_float(32,  16, real_gz, 5, 2);
                last_gz = gz_disp;
            }
            if (gdis_disp != last_gdis) {
                ips200_show_string(120, 16, "          ");
                ips200_show_float(120, 16, G_dis, 5, 1);
                last_gdis = gdis_disp;
            }
            if (err_disp != last_err) {
                ips200_show_string(32,  32, "          ");
                ips200_show_float(32,  32, gyro_pid.Error0, 5, 1);
                last_err = err_disp;
            }
            if (int_disp != last_int) {
                ips200_show_string(104, 32, "          ");
                ips200_show_float(104, 32, gyro_pid.ErrorInt, 5, 1);
                last_int = int_disp;
            }
            if (turn_control != last_turn) {
                ips200_show_string(48,  48, "          ");
                ips200_show_int(48,  48, turn_control, 6);
                last_turn = turn_control;
            }
        }
        {
            int32 lpwm = (int32_t)speed_pid_L.Out;
            int32 rpwm = (int32_t)speed_pid_R.Out;
            if (lpwm != last_lpwm) {
                ips200_show_string(48,  64, "          ");
                ips200_show_int(48,  64, lpwm, 6);
                last_lpwm = lpwm;
            }
            if (rpwm != last_rpwm) {
                ips200_show_string(120, 64, "          ");
                ips200_show_int(120, 64, rpwm, 6);
                last_rpwm = rpwm;
            }
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
