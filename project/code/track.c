#include "track.h"
#include <math.h>

#define base_speed  1000    // 基础pwm

/* ─── 角速度环量纲转换系数 ─── */
#define GZ_TO_ENCODER       0.299f  // gyro_z(°/s) → 编码器差速(脉冲/20ms), 实测标定
#define IMAGE_TO_GYRO_SCALE 0.5f    // image_out(抽象) → 角速度环期望(编码器差速单位)
                                     // 推算: 弯道编码器差速≈100~150, image_out≈500~700
                                     // scale = 120/600 ≈ 0.2。0.5太大导致角速度环常饱和

// 巡线模式（按下Start后进入，KEY1退出）
void track_line(void)
{
    int i, j;

//    otsu_enable = 1;

	static int32_t turn_control = 0;
	static float base_speed_target = 0;
		static float gyro_fb_filt = 0.0f;     // gyro_fb 低通滤波
		static float err_focus_filt = 55.0f;  // 动态前瞻滤波

	// 积分/误差清0
	speed_pid_L.ErrorInt = 0;
	speed_pid_L.Error0 = 0;
	speed_pid_L.Error1 = 0;
	speed_pid_R.ErrorInt = 0;
	speed_pid_R.Error0 = 0;
	speed_pid_R.Error1 = 0;
	gyro_pid.ErrorInt  = 0;
    gyro_pid.Error0    = 0;
    gyro_pid.Error1    = 0;

    while (1) {
        if (mt9v03x_finish_flag) {
//            /* 大津法动态阈值更�?*/
//            if (otsu_update_flag) {
//                otsu_update_flag = 0;
//                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
//            }

            /* 出界判断，出界则停车 */
            if (image_out_of_bounds(mt9v03x_image)) {
                break;
            }

			/* 斑马线判断，两次则停车 */
			if (zebra_count_total >= 2) {
                break;
            }

            /* 二值化 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization) ? 1 : 0;
                }
            }

            /* 扫线提取左右边界 */
            boundary_line_init();
            longest_white_sweepline(binary_image);

            /* 道宽半边补线：单边丢线时用道宽从另一侧推算 */
			inner_draw_line();

            /* 计算中线误差（内部使用 err_focus_row 做高斯加权） */
            line_err = err_sum_average(err_start_point, err_end_point);

			/* ──────────── 速度决策：直线+弯道速度计算 ──────────── */
			/* 入弯动态降速：偏差越大、速度越低 */
            static float speed_err = 0;
            speed_err += (line_err - speed_err) * 0.5f;  // α=0.5，够平滑且不过度滞后
			float speed_scale = 1.0f - fabsf(line_err) * 0.045f;
			float new_target = 380 * speed_scale;

   		    if (new_target < 200) new_target = 200;
   		    if (new_target < base_speed_target) {
   			    base_speed_target += (new_target - base_speed_target) * 1.0f;  // 减速快跟
   		    } else {
   			    base_speed_target += (new_target - base_speed_target) * 0.4f;  // 加速缓升
   		    }

			float avg_actual_speed = (speed_L + speed_R) / 2.0f;

			/* ──────────── 图像环 · 动态前瞻 ──────────── */
			/*
			 * 弯道看远提前预判，直道看近防摆头
			 * 使用轻微滤波平滑弯直交界过渡（α=0.6，快速响应避免相位滞后）
			 */
			{
			    float target_focus;
			    if (search_stop_line < 80)
			        target_focus = 45.0f;
			    else
			        target_focus = 75.0f - avg_actual_speed / 15.0f;
			    if (target_focus < 30) target_focus = 30;
			    if (target_focus > 100) target_focus = 100;
			    err_focus_filt += 0.8f * (target_focus - err_focus_filt);
			    err_focus_row = (uint8)err_focus_filt;
			}

			/* ──────────── 图像环 · 动态Kp ──────────── */
			/*
			 * 两层自适应（详见 pid.c Image_Kp_Update）:
			 *   1) 速度越快 → Kp越大（同等偏差需更强转向）
			 *   2) 视野越远(=越直) → Kp越低（防直道摆头）
			 * 基准: 速度300/中弯 → Kp=18.0（你调好的默认值）
			 */
			Image_Kp_Update(&image_pid_struct, avg_actual_speed, search_stop_line);
			// printf("Kp:%.1f focus:%d top:%d\r\n", image_pid_struct.Kp, err_focus_row, search_stop_line);

			/* ──────────── 图像环 · PID计算 ──────────── */
			/*
			 * Out = Kp×line_err + Kd×(line_err - last_err)
			 * 正=左转, 负=右转（与旧版 image_pid 符号一致）
			 */
			float image_out = Image_PID_Calculate(&image_pid_struct, line_err, 0);

			/* ──────────── 角速度环 ──────────── */
			/*
			 * 反馈: gyro_z 换算为编码器差速量纲 → 输入角速度环
			 * α=1.0 表示不过滤（直道需低延迟）。如果陀螺仪噪声大，降到 0.6~0.7
			 */
			float gyro_target = image_out * IMAGE_TO_GYRO_SCALE;
			float gyro_fb_raw = (float)real_gz * GZ_TO_ENCODER;
			gyro_fb_filt += 1.0f * (gyro_fb_raw - gyro_fb_filt);  // α=1.0：不过滤，无相位滞后

			gyro_pid.Target = gyro_target;
			gyro_pid.Actual = gyro_fb_filt;
			PID_Update(&gyro_pid);
			turn_control = -(int32_t)gyro_pid.Out;

			/* 防侧翻：动态差速限制，直道紧弯道松 */
			/* 偏差大=急弯→允许更大差速；偏差小=直道→收紧防翻 */
			{
			    float err_abs = fabsf(line_err);
			    float min_forward = 110.0f - err_abs * 2.5f;  // 直道≈110, 急弯≈10
			    if (min_forward < 10.0f) min_forward = 10.0f;
			    if (min_forward > 110.0f) min_forward = 110.0f;
			    float max_diff = base_speed_target - min_forward;
			    if (max_diff < 30.0f) max_diff = 30.0f;
			    if (turn_control >  (int32_t)max_diff) turn_control =  (int32_t)max_diff;
			    if (turn_control < -(int32_t)max_diff) turn_control = -(int32_t)max_diff;
			}

			/* ──────────── 左右速度环（最终输出级）──────────── */
			/* turn_control 已在编码器差速量纲，直接加减 */
			speed_pid_L.Target = base_speed_target + turn_control;
			speed_pid_L.Actual = speed_L;
			PID_Update(&speed_pid_L);

			speed_pid_R.Target = base_speed_target - turn_control;
			speed_pid_R.Actual = speed_R;
			PID_Update(&speed_pid_R);

            /* 差速输出：速度环直接输出 PWM */
			motor_set_pwm(DIR_L, PWM_L, (int32_t)speed_pid_L.Out);
            motor_set_pwm(DIR_R, PWM_R, (int32_t)speed_pid_R.Out);

            motor_protect();

            mt9v03x_finish_flag = 0;
        }

        key_scanner();
        system_delay_ms(1);
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }

    /* 停车 & 重绘菜单 */
    motor_set_pwm(DIR_L, PWM_L, 0);
    motor_set_pwm(DIR_R, PWM_R, 0);
//    otsu_enable = 0;
    menu_request_redraw();
}
