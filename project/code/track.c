#include "track.h"

void track_line   (void)
{
	motor_set_pwm(DIR_L,PWM_L,5000);
	motor_set_pwm(DIR_R,PWM_R,5000);
}
