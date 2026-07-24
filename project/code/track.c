#include "track.h"
#include <math.h>

#define base_speed  2200    // 基础pwm

// 巡线模式（按下Start后进入，KEY1退出）
void track_line(void)
{
    int i, j;

//    otsu_enable = 1;

	static int32_t turn_control = 0;

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

            /* 计算中线误差 */
            line_err = err_sum_average(err_start_point, err_end_point);
			
			/* 图像环 */
			image_pid.Target = 0;
			image_pid.Actual = line_err;
			PID_Update(&image_pid);
			float target_gz = -image_pid.Out;
			
			/* 角速度环 */
			gyro_pid.Target = target_gz;
			gyro_pid.Actual = (float)real_gz;
			PID_Update(&gyro_pid);
			turn_control = -(int32_t)gyro_pid.Out;

//            /* PID 更新 */
//            turn_control  = PPDD_location(0, line_err, gz, &track_pid);

            /* 入弯动态降速 v1：偏差越大、速度越低 */
			float speed_scale = 1.0f - fabsf(line_err) * 0.078f;
//			if (speed_scale < 0.35f) speed_scale = 0.35f;
			int32_t dyn_speed = (int32_t)(base_speed * speed_scale);
			
			/* 限幅 */
			if(dyn_speed < 800){dyn_speed = 800;}

//            /* 动态基础速度 v2：偏差 + 偏差变化率 联合降速
//               入弯时 err_delta 大 → 提前刹车，解决长直道入弯漂移 */
//            static float last_line_err = 0;
//            float abs_err   = fabsf(line_err);
//            float err_delta = fabsf(line_err - last_line_err);
//            last_line_err   = line_err;

//            float reduction = abs_err * 22.0f + err_delta * 45.0f;
//            if (reduction > 1300.0f) reduction = 1300.0f;
//            int32_t dyn_speed = base_speed - (int32_t)reduction;

            /* 差速输出 */
			motor_set_pwm(DIR_L, PWM_L, dyn_speed + turn_control);
            motor_set_pwm(DIR_R, PWM_R, dyn_speed - turn_control);

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
