#ifndef _camera_h
#define _camera_h

#include "zf_common_headfile.h"

#define DEAL_IMAGE_H 120
#define DEAL_IMAGE_W 188

void show_gary(void);
void show_binarize(void);
void boundary_line_init(void);
void show_boundary_line(void);
void longest_white_sweepline(uint8 image[DEAL_IMAGE_H][DEAL_IMAGE_W]);
uint8 image_out_of_bounds(unsigned char in_image[DEAL_IMAGE_H][DEAL_IMAGE_W]);

#endif
