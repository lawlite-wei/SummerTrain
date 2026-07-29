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

/*
** 电机堵转保护函数
*/
void motor_protect(void)
{
	static int16_t block_time = 0;
	
	if(speed_pid.Out > 2500 && speed_L < 20 && speed_R < 20)
	{
		block_time ++;
	}
	else{block_time = 0;}
	
	if(block_time >= 100)
	{
		motor_set_pwm(DIR_L,PWM_L,0);
		motor_set_pwm(DIR_R,PWM_R,0);
	}
}
