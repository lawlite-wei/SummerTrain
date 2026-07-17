#ifndef _camera_h
#define _camera_h

#include "zf_common_headfile.h"

#define DEAL_IMAGE_H 120
#define DEAL_IMAGE_W 188

extern uint8 binarization_threshold;
extern volatile uint8 otsu_update_flag;
extern uint8 otsu_enable;
extern float line_err;
extern uint8 err_start_point;
extern uint8 err_end_point;
extern uint8 binary_image[DEAL_IMAGE_H][DEAL_IMAGE_W];

void show_gary(void);
void show_binarize(void);
void boundary_line_init(void);
void show_boundary_line(void);
void longest_white_sweepline(uint8 image[DEAL_IMAGE_H][DEAL_IMAGE_W]);
uint8 image_out_of_bounds(unsigned char in_image[DEAL_IMAGE_H][DEAL_IMAGE_W]);
uint8 otsuThreshold(uint8 *image);
float err_sum_average(uint8 start_point,uint8 end_point);

#endif
