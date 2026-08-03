#include "pid.h"
#include <stdlib.h>
#include <math.h>

#define LIMIT(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

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

/*******************************************图像专用PID*************************************************/
float Image_PID_Calculate(Direction_PID *pid, float expect, float feedback)
{
    pid->Error = expect;

    pid->OutPut = pid->Kp * pid->Error
                + pid->Kp2 * pid->Error * fabsf(pid->Error)
                + pid->Kd * (pid->Error - pid->Last_Error)
                + pid->Kd_feedback * feedback;

    pid->OutPut = LIMIT(pid->OutPut, -pid->MAX_OutPut, pid->MAX_OutPut);

    pid->Last_Error = pid->Error;

    return pid->OutPut;
}

/*******************************************图像环PID初始化********************************************/
void Direction_PID_Init(Direction_PID *pid, float kp, float ki, float kd,
                         float kd_feedback, float kp2, float max_out)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->Kd_feedback = kd_feedback;
    pid->Kp2 = kp2;
    pid->MAX_OutPut = max_out;

    pid->Error = 0;
    pid->Last_Error = 0;
    pid->Last_Last_Error = 0;
    pid->OutPut = 0;
    pid->Integral = 0;
    pid->MAX_Integral = max_out;
}

/*******************************************图像环Kp动态更新********************************************/
/**
 * @brief  根据当前速度和视野距离动态调整图像环Kp
 * @param  pid               图像环PID结构体指针
 * @param  avg_speed         当前平均速度（编码器脉冲/周期，范围约230~380）
 * @param  search_stop_line  搜索截至行（最长白列长度，越大=视野越远=越直，范围约30~120）
 *
 * 基准: 速度300、中弯(search_stop_line≈90)时 Kp=18.0（你调好的值）
 *
 * 公式: Kp = kp_ref × (speed / speed_ref) × dist_factor
 *       kp_ref = 18.0   —— 你的基准Kp
 *       speed_ref = 300 —— 你的正常巡航速度
 *
 * 两层自适应:
 *   ┌─ 速度因子: speed/speed_ref
 *   │   速度230 → ×0.77  (低速弯不需要太强转向)
 *   │   速度300 → ×1.00  (基准)
 *   │   速度380 → ×1.27  (高速需要更强转向力)
 *   │
 *   └─ 距离因子 dist_factor (视野越远=越直 → 因子越小 → Kp越低):
 *       search_stop_line > 110 → ×0.50  超长直道
 *       search_stop_line > 100 → ×0.65  长直道
 *       search_stop_line > 90  → ×0.80  中直道
 *       search_stop_line > 80  → ×0.90  微弯
 *       search_stop_line ≤ 80  → ×1.00  弯道(不打折)
 *
 * 典型工况Kp值:
 *   低速弯(230,≤80): 18×0.77×1.00 = 13.8
 *   中速直(300,>100): 18×1.00×0.65 = 11.7
 *   高速弯(380,≤80): 18×1.27×1.00 = 22.8
 *   高速直(380,>110): 18×1.27×0.50 = 11.4
 *
 * 硬限幅: Kp ∈ [8.0, 28.0]
 *
 * 调试: 在 track.c 的 printf 中打印 image_pid_struct.Kp 可实时观察Kp值
 */
void Image_Kp_Update(Direction_PID *pid, float avg_speed, uint8 search_stop_line)
{
    float kp_ref     = 18.0f;   // 你调好的基准Kp（原 image_pid.Kp）
    float speed_ref  = 320.0f;  // 你的正常巡航速度
    float speed_ratio = avg_speed / speed_ref;
    if (speed_ratio < 1.0f) speed_ratio = 1.0f;  // 低速不降Kp，高速才升Kp

    /* 距离因子: 视野越远=赛道越直 → Kp越低，防摆头 */
    /* search_stop_line 80→120 线性映射到 dist_factor 1.0→0.5 */
    float dist_factor;
    float t = ((float)search_stop_line - 80.0f) / 40.0f;
    if      (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    dist_factor = 1.0f - 0.5f * t;

    // [注释] 旧版阶梯分档：
    // if      (search_stop_line > 110)  dist_factor = 0.50f;  // 超长直道
    // else if (search_stop_line > 100)  dist_factor = 0.65f;  // 长直道
    // else if (search_stop_line > 90)   dist_factor = 0.80f;  // 中直道
    // else if (search_stop_line > 80)   dist_factor = 0.90f;  // 微弯
    // else                              dist_factor = 1.00f;  // 弯道

    pid->Kp = kp_ref * speed_ratio * dist_factor;

    /* 硬限幅 */
    if (pid->Kp < 8.0f)   pid->Kp = 8.0f;
    if (pid->Kp > 35.0f)  pid->Kp = 35.0f;
}

/*
*	角速度环
*   主要调节k,d  
*/ 
PID_t gyro_pid={
	.Kp = 18,
	.Ki = 0.0,
	.Kd = 13,
	
	.OutMax = 5000,
	.OutMin = -5000,
};

/*
*	图像环
*   主要调节k   （跟转弯有关）
*/ 
PID_t image_pid={
	.Kp = 18.0,
	.Ki = 0.0,
	.Kd = 1.5,
	
	.OutMax = 600,
	.OutMin = -600,
};

/*
*	速度环（保留作为参数参考，不再直接使用）
*/
PID_t speed_pid={
	.Kp = 7.1,
	.Ki = 1.0,
	.Kd = 0.2,

	.OutMax = 5000,
	.OutMin = -5000,
};

/*
*	左轮速度环（位置式）
*   参数继承原 speed_pid，后续独立整定
*/
PID_t speed_pid_L={
	.Kp = 7.1,
	.Ki = 1.0,
	.Kd = 0.2,

	.OutMax = 10000,
	.OutMin = -10000,
};

/*
*	右轮速度环（位置式）
*   参数继承原 speed_pid，后续独立整定
*/
PID_t speed_pid_R={
	.Kp = 7.1,
	.Ki = 1.0,
	.Kd = 0.2,

	.OutMax = 10000,
	.OutMin = -10000,
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

Direction_PID image_pid_struct = {
	.Kp = 18.0,             // 初始值，运行中由 Image_Kp_Update 动态更新
	.Kp2 = 0.0,
	.Ki = 0.0,
	.Kd = 1.5,              // 微分项（阻尼，固定不变）
	.Kd_feedback = 0.0,

	.Max_Error = 10000,
	.MAX_Integral = 10000,
	.MAX_OutPut = 700,

	.OutPut = 0.0,
};

