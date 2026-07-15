#include "pid.h"

// pid计算函数
void PID_Update(PID_t *p)
{
	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;
	
	if (p->Ki != 0)
	{
		p->ErrorInt += p->Error0;
	}
	else
	{
		p->ErrorInt = 0;
	}
	
	p->Out = p->Kp * p->Error0
		   + p->Ki * p->ErrorInt
		   + p->Kd * (p->Error0 - p->Error1);

	// 抗积分饱和：Out被截断时，撤销本次ErrorInt的累加，防止积分无限增长
	if (p->Out > p->OutMax)
	{
		if (p->Error0 > 0 && p->Ki != 0) {p->ErrorInt -= p->Error0;}
		p->Out = p->OutMax;
	}
	if (p->Out < p->OutMin)
	{
		if (p->Error0 < 0 && p->Ki != 0) {p->ErrorInt -= p->Error0;}
		p->Out = p->OutMin;
	}
}

// 速度环
PID_t speed_pid={
	.Kp = 300,
	.Ki = 0,
	.Kd = 0,
	
	.OutMax = 7000,
	.OutMin = -7000,
};

// 寻迹pid控制
PID_t track_pid={
	.Kp = 300,
	.Ki = 0,
	.Kd = 0,
	
	.Target = 0,
	.OutMax = 7000,
	.OutMin = -7000,
};
