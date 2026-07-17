#include "track.h"

#define base_speed  2000    // 基础pwm

// 巡线模式（按下Start后进入，KEY1退出）
void track_line(void)
{
    int i, j;

    otsu_enable = 1;

    while (1) {
        if (mt9v03x_finish_flag) {
            /* 大津法动态阈值更新 */
            if (otsu_update_flag) {
                otsu_update_flag = 0;
                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
            }

            /* 二值化 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization_threshold) ? 1 : 0;
                }
            }

            /* 扫线提取左右边界 */
            boundary_line_init();
            longest_white_sweepline(binary_image);

            /* 计算中线误差 */
            line_err = err_sum_average(err_start_point, err_end_point);

            /* PID 更新 */
            track_pid.Actual = line_err;
            PID_Update(&track_pid);

            /* 差速输出 */
            motor_set_pwm(DIR_L, PWM_L, base_speed + track_pid.Out);
            motor_set_pwm(DIR_R, PWM_R, base_speed - track_pid.Out);

            mt9v03x_finish_flag = 0;
        }

        key_scanner();
        system_delay_ms(10);
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }

    /* 停车 & 重绘菜单 */
    motor_set_pwm(DIR_L, PWM_L, 0);
    motor_set_pwm(DIR_R, PWM_R, 0);
    otsu_enable = 0;
    menu_request_redraw();
}
