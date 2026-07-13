#include "motor.h"

void motor_set_pwm(gpio_pin_enum dir,pwm_channel_enum pwm, const uint32 duty)
{
	gpio_set_level(dir, GPIO_HIGH);                                   // DIR输出高电平
    pwm_set_duty(pwm, duty);                                          // 输出pwm
}


