#include "track.h"
#include <math.h>

#define base_speed  1000    // 基础pwm

// 巡线模式（按下Start后进入，KEY1退出）
void track_line(void)
{
    int i, j;

//    otsu_enable = 1;

	static int32_t turn_control = 0;
	// 每次速度环积分清0
	speed_pid.ErrorInt = 0; 
	speed_pid.Error0 = 0;
	speed_pid.Error1 = 0;

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
            road_wide_fill_lost_line();

            /* 计算中线误差 */
            line_err = err_sum_average(err_start_point, err_end_point);
			
			/* 速度环 */
			speed_pid.Target = 210;
			speed_pid.Actual = (speed_L + speed_R) / 2;
			PID_Update(&speed_pid);
			float dif_speed = speed_pid.Out;
			
			//if(fabsf(line_err) < 20){image_pid.ErrorInt = 0;}
			
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

            /* 入弯动态降速 v1：偏差越大、速度越低 */
//			float speed_scale = 1.0f - fabsf(line_err) * 0.045f;
//			if (speed_scale < 0.35f) speed_scale = 0.35f;
//			int32_t dyn_speed = (int32_t)(base_speed * speed_scale);
//			
//			/* 限幅 */
//			if(dyn_speed < 800){dyn_speed = 800;}

            /* 差速输出 */
//			motor_set_pwm(DIR_L, PWM_L, base_speed + turn_control);
//            motor_set_pwm(DIR_R, PWM_R, base_speed - turn_control);
			
			motor_set_pwm(DIR_L, PWM_L, speed_pid.Out + turn_control);
            motor_set_pwm(DIR_R, PWM_R, speed_pid.Out - turn_control);

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
