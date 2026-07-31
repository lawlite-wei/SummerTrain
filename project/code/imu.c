#include "imu.h"

int16_t gz;

float real_gz;

// imu读取函数
void imu_get(void)
{
    imu963ra_get_acc();
    imu963ra_get_gyro();
}

// gz去零漂
void gz_filter(void)
{
	gz = imu963ra_gyro_z + 5;   // 5为零漂
}

// 实际角速度获取
void get_real_gz(void)
{
	gz = imu963ra_gyro_z + 5;   // 5为零漂
	real_gz = imu963ra_gyro_transition(imu963ra_gyro_z/10*10);
}


/*
 *  IMU 6轴数据显示（Debug → imu → 进入）
 *  实时显示 acc x/y/z 和 gyro x/y/z，KEY_1 退出
 */
void imu_test(void)
{
    int16 last_ax = 1, last_ay = 1, last_az = 1;
    int16 last_gx = 1, last_gy = 1, last_gz = 1;

    ips200_clear();
    ips200_show_string(0,   0, "--IMU--");
    ips200_show_string(0,  16, "acc_x:");
    ips200_show_string(0,  32, "acc_y:");
    ips200_show_string(0,  48, "acc_z:");
    ips200_show_string(0,  64, "gyro_x:");
    ips200_show_string(0,  80, "gyro_y:");
    ips200_show_string(0,  96, "gyro_z:");
    ips200_show_string(0, 128, "KEY1: back");

    while (1) {
        if (last_ax != imu963ra_acc_x) {
            ips200_show_string(72, 16, "     ");
            ips200_show_int(72, 16, imu963ra_acc_x, 5);
            last_ax = imu963ra_acc_x;
        }
        if (last_ay != imu963ra_acc_y) {
            ips200_show_string(72, 32, "     ");
            ips200_show_int(72, 32, imu963ra_acc_y, 5);
            last_ay = imu963ra_acc_y;
        }
        if (last_az != imu963ra_acc_z) {
            ips200_show_string(72, 48, "     ");
            ips200_show_int(72, 48, imu963ra_acc_z, 5);
            last_az = imu963ra_acc_z;
        }
        if (last_gx != imu963ra_gyro_x) {
            ips200_show_string(72, 64, "     ");
            ips200_show_int(72, 64, imu963ra_gyro_x, 5);
            last_gx = imu963ra_gyro_x;
        }
        if (last_gy != imu963ra_gyro_y) {
            ips200_show_string(72, 80, "     ");
            ips200_show_int(72, 80, imu963ra_gyro_y, 5);
            last_gy = imu963ra_gyro_y;
        }
        if (last_gz != imu963ra_gyro_z) {
            ips200_show_string(72, 96, "     ");
            ips200_show_int(72, 96, imu963ra_gyro_z, 5);
            last_gz = imu963ra_gyro_z;
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
