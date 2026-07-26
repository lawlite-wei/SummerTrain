#include "encoder.h"

int16_t speed_L = 0;
int16_t speed_R = 0;

int32_t distance_L = 0;
int32_t distance_R = 0;

/*
 *  编码器数据更新（在 pit_handler 中周期调用）
 *  使用 encoder_get_count 获取带方向的脉冲计数
 *  每次调用后清零计数器，speed = 本次周期内的脉冲增量
 */
void encoder_update(void)
{
    int16_t cnt;

    cnt = encoder_get_count(ENCODER_1);
    encoder_clear_count(ENCODER_1);
    speed_L    = cnt;
    distance_L += cnt;

    cnt = -encoder_get_count(ENCODER_2);
    encoder_clear_count(ENCODER_2);
    speed_R    = cnt;
    distance_R += cnt;
}
