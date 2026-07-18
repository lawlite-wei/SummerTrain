#ifndef _camera_h
#define _camera_h

#include "zf_common_headfile.h"

#define binarization   243  // 默认二值化阈值

#define DEAL_IMAGE_H 120
#define DEAL_IMAGE_W 188

//extern uint8 binarization_threshold;        /* 大津法动态阈值（已注释） */
//extern volatile uint8 otsu_update_flag;     /* 大津法更新请求标志（已注释） */
//extern uint8 otsu_enable;                   /* 大津法使能（已注释） */
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
//uint8 otsuThreshold(uint8 *image);         /* 大津法（已注释） */
float err_sum_average(uint8 start_point,uint8 end_point);
void left_draw_line(uint8 x1,uint8 y1,uint8 x2,uint8 y2);
void right_draw_line(uint8 x1,uint8 y1,uint8 x2,uint8 y2);
void extend_left_line(uint8 start_point, uint8 end_point);
void extend_right_line(uint8 start_point, uint8 end_point);
void road_wide_draw_left_line(void);
void road_wide_draw_right_line(void);
void find_down_point(uint8 start_point,uint8 end_point);
void find_up_point(uint8 start_point,uint8 end_point);
uint8 straight_judge(void);
void cross_judge(void);


#endif
