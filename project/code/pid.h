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

extern PID_t speed_pid;
extern PID_t gyro_pid;
extern PID_t image_pid;
extern Turn_PPDD_LocTypeDef track_pid;

void PID_Update(PID_t *p);
float PPDD_location(float setvalue, float actualvalue, float GZ, Turn_PPDD_LocTypeDef *PPDD);

#endif
