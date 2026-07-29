#ifndef _debug_test_h
#define _debug_test_h

#include "zf_common_headfile.h"
#include "encoder.h"

extern int8_t imu_init;

void HARDWARE_INIT      (void);
void motor_test         (void);
void encoder_test       (void);
void speed_hold_test    (void);
void gyro_hold_test     (void);

#endif
