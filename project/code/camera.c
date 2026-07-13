#include "camera.h"

/*==================== Camera 图像显示 ====================*/
#define BINARIZATION_THRESHOLD      64      /* 二值化阈值 */

/*
 *  灰度显示：持续刷新摄像头灰度图像，按下 button1(返回) 退出
 */
void show_gary(void)
{
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

    while (1) {
        if (mt9v03x_finish_flag) {
            ips200_displayimage03x((const uint8 *)mt9v03x_image, 240, 180);
            mt9v03x_finish_flag = 0;
        }
        key_scanner();
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }
}

/*
 *  二值化显示：持续刷新摄像头二值化图像，按下 button1(返回) 退出
 */
void show_binarize(void)
{
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

    while (1) {
        if (mt9v03x_finish_flag) {
            ips200_show_gray_image(0, 0, (const uint8 *)mt9v03x_image,
                                   MT9V03X_W, MT9V03X_H, 240, 180,
                                   BINARIZATION_THRESHOLD);
            mt9v03x_finish_flag = 0;
        }
        key_scanner();
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            break;
        }
    }
}
