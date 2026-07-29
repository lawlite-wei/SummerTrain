#include "pid.h"
#include <stdlib.h>
#include <math.h>

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

/**
* @brief  PPDD位置式PID
* @param   setvalue 设定值
* @param   actualvalue 实际值
* @param   GZ  角速度
* @param   PPDD PPDD参数结构体
*/
float PPDD_location(float setvalue, float actualvalue, float GZ, Turn_PPDD_LocTypeDef *PPDD)
{
    PPDD->ek = setvalue - actualvalue;
    PPDD->location_sum += PPDD->ek;

    // 使用 fabsf 代替 abs
    PPDD->out = PPDD->kp * PPDD->ek 
                + PPDD->kp2 * fabsf(PPDD->ek) * PPDD->ek 
                + (PPDD->ek - PPDD->ek1) * PPDD->kd 
                + GZ * PPDD->kd2;

    PPDD->ek1 = PPDD->ek;

    // 限幅
    if (PPDD->out < -PPDD->PID_OUT_LIMIT_MAX) PPDD->out = -PPDD->PID_OUT_LIMIT_MAX;
    if (PPDD->out >  PPDD->PID_OUT_LIMIT_MAX) PPDD->out =  PPDD->PID_OUT_LIMIT_MAX;

    return PPDD->out;
}

/*
*	角速度环
*   主要调节k,d  
*/ 
PID_t gyro_pid={
	.Kp = 18,
	.Ki = 0.0,
	.Kd = 13,
	
	.OutMax = 5350,
	.OutMin = -5350,
};

/*
*	图像环
*   主要调节k   （跟转弯有关）
*/ 
PID_t image_pid={
	.Kp = 18.0,
	.Ki = 0.0,
	.Kd = 2.5,
	
	.OutMax = 550,
	.OutMin = -550,
};

/*
*	速度环  (暂时不用)
*   主要调节k,d
*/ 
PID_t speed_pid={
	.Kp = 7.1,
	.Ki = 1.0,
	.Kd = 0.2,
	
	.OutMax = 3000,
	.OutMin = -3000,
};

/*
*	寻迹pid控制
*   主要调节k,d
*/
Turn_PPDD_LocTypeDef track_pid = {
	.kp = 120,
	.kd = 60,
	.kp2 = 0.5,
	.kd2 = 0.6,
	
	.PID_OUT_LIMIT_MAX = 7800,
};
