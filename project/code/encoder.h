#ifndef _encoder_h
#define _encoder_h

#include "zf_common_headfile.h"

#define ENCODER_1                   (TIM3_ENCODER)
#define ENCODER_1_LSB               (TIM3_ENCODER_CH1_B4)
#define ENCODER_1_DIR               (TIM3_ENCODER_CH2_B5)

#define ENCODER_2                   (TIM4_ENCODER)
#define ENCODER_2_LSB               (TIM4_ENCODER_CH1_B6)
#define ENCODER_2_DIR               (TIM4_ENCODER_CH2_B7)

extern int16_t speed_L;
extern int16_t speed_R;
extern int32_t distance_L;
extern int32_t distance_R;

void encoder_update(void);

#endif
