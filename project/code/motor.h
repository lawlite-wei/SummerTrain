#ifndef _motor_h
#define _motor_h

#include "zf_common_headfile.h"

#define DIR_L               (A0 )
#define PWM_L               (TIM5_PWM_CH2_A1)

#define DIR_R               (A2 )
#define PWM_R               (TIM5_PWM_CH4_A3)

void motor_set_pwm(gpio_pin_enum dir,pwm_channel_enum pwm,int32 duty);
void motor_protect(void);

#endif
