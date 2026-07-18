#include "encoder.h"

volatile int16_t speed_L = 0;
volatile int16_t speed_R = 0;

volatile int32_t distance_L = 0;
volatile int32_t distance_R = 0;

/*
 *  编码器数据更新（在 pit_handler 中周期调用）
 *  读取两个编码器计数 → 更新速度和距离全局变量
 */
void encoder_update(void)
{
    int16_t cnt;

    cnt = encoder_get_count(ENCODER_1);
    encoder_clear_count(ENCODER_1);
    speed_L    = cnt;
    distance_L += cnt;

    cnt = encoder_get_count(ENCODER_2);
    encoder_clear_count(ENCODER_2);
    speed_R    = cnt;
    distance_R += cnt;
}
