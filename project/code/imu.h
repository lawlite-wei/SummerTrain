#ifndef _imu_h
#define _imu_h

#include "zf_common_headfile.h"

extern int16_t gz;

void imu_get(void);
void gz_filter(void);
void imu_test(void);

#endif
