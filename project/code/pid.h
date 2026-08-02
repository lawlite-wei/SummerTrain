#ifndef _pid_h
#define _pid_h

#include "zf_common_headfile.h"

typedef struct 
{
	float Target;
	float Actual;
	float Out;
	
	float Kp;
	float Ki;
	float Kd;
	
	float Error0;
	float Error1;
	float ErrorInt;
	
	float OutMax;
	float OutMin;
} PID_t;

typedef struct
{
  float kp;
  float kd;
  float kp2;
  float kd2;
  float ek;                      
  float ek1;                      
  float location_sum;            
  float out;
  float PID_I_LIMIT_MAX;
  float PID_OUT_LIMIT_MAX;	
}Turn_PPDD_LocTypeDef;

typedef struct{
        float Kp;//一次项系数
        float Kp2;//二次项系数
        float Ki;
        float Kd;
        float Kd_feedback;      //反馈值微分系数

        float Error;            //误差
        float Integral;         //误差积分
        float Max_Error;            //误差
        float Last_Error;       //上次误差
        float Last_Last_Error;

        float OutPut;
        float MAX_Integral;
        float MAX_OutPut;
        float expect_last;

}Direction_PID;

extern PID_t speed_pid;
extern PID_t gyro_pid;
extern PID_t image_pid;
extern Turn_PPDD_LocTypeDef track_pid;
extern Direction_PID image_pid_struct;

void PID_Update(PID_t *p);
float PPDD_location(float setvalue, float actualvalue, float GZ, Turn_PPDD_LocTypeDef *PPDD);
void Direction_PID_Init(Direction_PID *pid, float kp, float ki, float kd,
                         float kd_feedback, float kp2, float max_out);
float Image_PID_Calculate(Direction_PID *pid, float expect, float feedback);
void Image_Kp_Update(Direction_PID *pid, float avg_speed, uint8 search_stop_line);

#endif
