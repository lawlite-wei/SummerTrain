#include "motor.h"

void motor_set_pwm(gpio_pin_enum dir,pwm_channel_enum pwm,int32 duty)
{
	// 极性反转
	duty = -duty;
	
	// 限幅
	if(duty >= 8000){duty = 8000;}
	if(duty <= -8000){duty = -8000;}
	
	if(duty >= 0)
	{
		gpio_set_level(dir, GPIO_HIGH);                                   // DIR输出高电平
		pwm_set_duty(pwm, duty);                                          // 输出pwm
	}
	else
	{
		gpio_set_level(dir, GPIO_LOW);                                    // DIR输出高电平
		pwm_set_duty(pwm, (-duty));                                       // 输出pwm
	}
}
