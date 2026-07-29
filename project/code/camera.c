#include "camera.h"
#include <math.h>

/*==================== Camera 图像显示 ====================*/
//uint8 binarization_threshold = 64;            /* 二值化阈值（大津法动态更新） */
//volatile uint8 otsu_update_flag = 0;          /* 大津法更新请求标志（PIT中断置1） */
//uint8 otsu_enable = 0;                        /* 大津法使能（进入摄像头显示时置1） */

// 相关变量定义
float line_err;                             // 中线误差
uint8 search_stop_line;                     // 搜索截至行

int16 left_line[MT9V03X_H];                 // 左边界数组
int16 right_line[MT9V03X_H];                // 右边界数组
uint8 mid_line[MT9V03X_H];                  // 中线数组

uint8 boundary_start_left;                   // 左边界起点
uint8 left_up_point;                        // 左上拐点
uint8 left_down_point;                      // 左下拐点
uint8 left_lost_count;                      // 左丢线计数
uint8 left_start;

uint8 boundary_start_right;                  // 右边界起点
uint8 right_up_point;                       // 右上拐点
uint8 right_down_point;                     // 右下拐点
uint8 right_lost_count;                     // 右丢线计数
uint8 right_start;

uint8 left_right_lost_count;                // 左右同时丢线计数

uint8 real_road_wide[DEAL_IMAGE_H];

// 最长白列相关变量申明
uint8 white_count[MT9V03X_W];               // 白列数组，来储存每一列白列的长度     
int16 longest_white_left[2];                // 左边的最长白列，0为长度，1为位置
int16 longest_white_right[2];               // 右边的最长白列，0为长度，1为位置
int16 left_lost_flag[MT9V03X_H];            // 左丢线数组
int16 right_lost_flag[MT9V03X_H];           // 右丢线数组

uint8 binary_image[DEAL_IMAGE_H][DEAL_IMAGE_W];  // 二值化图像缓冲（最长白列算法用）

// 默认视野范围
uint8 err_start_point = 25; //误差起始点
uint8 err_end_point = 100;   //误差终止点

//  斑马线相关变量判定
uint8 zebra_count_total = 0;      // 斑马线总计数
uint8 zebra_detect_state = 0;     // 斑马线检测状态 0:未检测 1:检测中 2:已通过
uint16 zebra_clear_timer = 0;     // 斑马线清除计时器
uint8 zebra_last_flag = 0;        // 上次斑马线标志
uint8 zebra_flag = 0;             // 斑马线标志位

// 外切补线赛道宽度数组
const uint8 road_wide[DEAL_IMAGE_H]=
{
41,42,43,45,46,47,49,49,51,53,
53,55,55,57,58,59,61,62,63,64,
65,67,68,69,70,72,73,74,76,76,
78,79,80,82,82,84,86,86,88,88,
90,91,92,94,95,96,97,98,100,100,
102,103,105,105,107,108,109,111,112,113,
114,116,117,118,119,120,122,123,124,126,
126,128,129,130,132,132,134,134,136,138,
138,140,140,142,144,144,146,146,148,149,
150,151,152,154,155,156,157,158,159,161,
162,163,164,165,166,167,169,170,171,172,
173,175,175,177,177,179,180,181,184,184
};

// 内切补线赛道宽度数组
const uint8 road_wide_inner[DEAL_IMAGE_H]=
{
41,42,43,45,46,47,49,49,51,53,
53,55,55,57,58,59,61,62,63,64,
65,67,68,69,70,72,73,74,76,76,
78,79,80,82,82,84,86,86,88,88,
90,91,92,94,95,96,97,98,100,100,
102,103,105,105,107,108,109,111,112,113,
114,116,117,118,119,120,122,123,124,126,
126,128,129,130,132,132,134,134,136,138,
138,140,140,142,144,144,146,146,148,149,
150,151,152,154,155,156,157,158,159,161,
162,163,164,165,166,167,169,170,171,172,
173,175,175,177,177,179,180,181,184,184
};

//元素标志位
uint8 cross_flag;//十字标志位
uint8 straight_flag=0; //直线标志位
uint8 circle_flag=0; //环岛标志位

/*
 *  灰度显示：持续刷新摄像头灰度图像，按下 button1(返回) 退出
 */
void show_gary(void)
{
    int i, j;
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

//    otsu_enable = 1;  /* 启动大津法定时更新 */

    while (1) {
        if (mt9v03x_finish_flag) {
//            /* PIT中断每5ms置位，帧完整时更新阈值 */
//            if (otsu_update_flag) {
//                otsu_update_flag = 0;
//                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
//            }

            /* 二值化：灰度转0/1，供最长白列算法使用 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization) ? 1 : 0;
                }
            }

            /* 显示灰度图像（y=30 与 show_boundary_line 对齐） */
            ips200_show_gray_image(0, 30, (const uint8 *)mt9v03x_image,
                                   MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H, 0);

            /* 最长白列巡线 + 边界叠加显示 */
            boundary_line_init();
            longest_white_sweepline(binary_image);
//            road_wide_fill_lost_line();
			inner_draw_line();
            show_boundary_line();
            show_saidao_flag(); /* 右下角元素类型 */

            /* 实时显示 line_err */
            {
                static float last_err = 1000;
                float e = err_sum_average(err_start_point, err_end_point);
                if (e != last_err) {
                    ips200_show_string(0, 160, "err:        ");
                    ips200_show_float(0, 160, e, 4, 1);
                    last_err = e;
                }
            }

            mt9v03x_finish_flag = 0;
        }
        key_scanner();
        system_delay_ms(10);
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            menu_request_redraw();
            break;
        }
    }

//    otsu_enable = 0;
}

/*
 *  二值化显示：持续刷新摄像头二值化图像，按下 button1(返回) 退出
 */
void show_binarize(void)
{
    int i, j;
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

//    otsu_enable = 1;  /* 启动大津法定时更新 */

    while (1) {
        if (mt9v03x_finish_flag) {
//            /* PIT中断每5ms置位，帧完整时更新阈值 */
//            if (otsu_update_flag) {
//                otsu_update_flag = 0;
//                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
//            }

            /* 二值化：灰度转0/1，供最长白列算法使用 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization) ? 1 : 0;
                }
            }

            /* 显示二值化图像（y=30 与 show_boundary_line 对齐） */
            ips200_show_gray_image(0, 30, (const uint8 *)mt9v03x_image,
                                   MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H,
                                   binarization);

            /* 最长白列巡线 + 边界叠加显示 */
            boundary_line_init();
            longest_white_sweepline(binary_image);
//            road_wide_fill_lost_line();
			inner_draw_line();
            show_boundary_line();
            show_saidao_flag(); /* 右下角元素类型 */

            /* 实时显示 line_err */
            {
                static float last_err = 1000;
                float e = err_sum_average(err_start_point, err_end_point);
                if (e != last_err) {
                    ips200_show_string(0, 160, "err:        ");
                    ips200_show_float(0, 160, e, 4, 1);
                    last_err = e;
                }
            }

            mt9v03x_finish_flag = 0;
        }
        key_scanner();
        system_delay_ms(10);
        if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
            key_clear_state(KEY_1);
            menu_request_redraw();
            break;
        }
    }

//    otsu_enable = 0;
}

/*
*   数据初始化
*/
void boundary_line_init(void)
{
    for(int i=0;i<MT9V03X_H;i++)
    {
        left_line[i]=0;
        right_line[i]=MT9V03X_W-1;
        mid_line[i]=(left_line[i]+right_line[i])/2;
    }
    boundary_start_left=0;
    boundary_start_right=MT9V03X_W-1;
    left_down_point=0;
    left_up_point=0;
    right_down_point=0;
    right_up_point=0;
}

/*
 *  边界显示函数（可以在ips200里显示左边界，右边界和中线）
 */
void show_boundary_line(void)
{
    for(int i=30;i<30+MT9V03X_H;i++)
    {
        if((i-30)!=left_down_point&&(i-30)!=left_up_point)
        {
            ips200_draw_point(left_line[i-30],i,RGB565_RED);
            ips200_draw_point(left_line[i-30]+1,i,RGB565_RED);
            ips200_draw_point(left_line[i-30]+2,i,RGB565_RED);  
        }
        else
        {
            if((i-30)==left_down_point)
            {
                ips200_draw_point(left_line[i-30],i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30],i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30],i+1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i+1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i+1,RGB565_BLUE);
            }
            else
            {
                ips200_draw_point(left_line[i-30],i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i-1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30],i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i,RGB565_BLUE);
                ips200_draw_point(left_line[i-30],i+1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+1,i+1,RGB565_BLUE);
                ips200_draw_point(left_line[i-30]+2,i+1,RGB565_BLUE);
            }
        }

        if((i-30)!=right_down_point&&(i-30)!=right_up_point)
        { 
            ips200_draw_point(right_line[i-30],i,RGB565_RED);
            ips200_draw_point(right_line[i-30]-1,i,RGB565_RED);
            ips200_draw_point(right_line[i-30]-2,i,RGB565_RED);
        }
        else
        {
            if((i-30)==right_down_point)
            {
                ips200_draw_point(right_line[i-30],i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30],i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30],i+1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i+1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i+1,RGB565_BLUE);
            }
            else
            {
                ips200_draw_point(right_line[i-30],i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i-1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30],i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i,RGB565_BLUE);
                ips200_draw_point(right_line[i-30],i+1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-1,i+1,RGB565_BLUE);
                ips200_draw_point(right_line[i-30]-2,i+1,RGB565_BLUE);
            }
        }

        ips200_draw_point(mid_line[i-30],i,RGB565_GREEN);
        ips200_draw_point(mid_line[i-30]-1,i,RGB565_GREEN);
        ips200_draw_point(mid_line[i-30]+1,i,RGB565_GREEN);
    }
}

/*
 *  简单的出界判断
 *  通过扫描正下方的一小部分区域来判定（10*3）   
 *  0：正常，1：出界
 */
uint8 image_out_of_bounds(unsigned char in_image[DEAL_IMAGE_H][DEAL_IMAGE_W])
{
    int sum = 0;
    for(int i=0;i<10; i++)
    {
        for(int j=0;j<5;j++)
        {
            sum+=in_image[DEAL_IMAGE_H-1-j][DEAL_IMAGE_W/2-5+i];
        }
    }
    int average = sum / 30;    // 计算平均值
    if(average < 210){return 1;}
    else{return 0;}
}

/*
 *  最长白列巡线扫线（双白列）
 */
void longest_white_sweepline(uint8 image[DEAL_IMAGE_H][DEAL_IMAGE_W])
{
    int i,j;
    int start_point = 20;                          // 开始扫线的下标
    int end_point = DEAL_IMAGE_W - 20;              // 结束扫线的下标 
    int left_border = 0, right_border = 0;         // 储存赛道临时位置

    // 变量清零初始化
    longest_white_left[0] = 0;
    longest_white_left[1] = 0;
    longest_white_right[0] = 0;
    longest_white_right[1] = 0;
    left_start = 0;
    right_start = 0;
    for(i=0;i<=DEAL_IMAGE_H-1;i++) 
    {
        left_line[i] = 0;
        right_line[i] = DEAL_IMAGE_W-1;
        left_lost_flag[i]=0;
        right_lost_flag[i]=0;
        real_road_wide[i]=0;
    }
    for(i=0;i<=DEAL_IMAGE_W-1;i++)
    {
        white_count[i]=0;
    }
    //记录每列白点数量
    for(j=start_point;j<end_point;j++)
    {
        for(i=DEAL_IMAGE_H-1;i>=0;i--)
        {
            if(image[i][j] == 0)break;//黑点跳出
            else white_count[j]++;//白点计数
        }
    }
    //寻找左最长白列
    for(i=start_point;i<end_point;i+=1)
    {
        if( longest_white_left[0] < white_count[i])//找最长的那一列，寻到右边界
        {
            longest_white_left[0] = white_count[i];
            longest_white_left[1] = i;
        }
    }
    //寻找右最长白列
    for(i=end_point;i>=longest_white_left[1];i-=1)//从右往左，找到右最长白列，寻到左边最长白列位置
    {
        if( longest_white_right[0] < white_count[i])//找最长的那一列
        {
            longest_white_right[0] = white_count[i];
            longest_white_right[1] = i;
        }
    }

    // 自适应的扫线截止线，防止过度扫线
    search_stop_line = (longest_white_left[0]>longest_white_right[0])?longest_white_left[0]:longest_white_right[0];

    //巡线
    for(i = DEAL_IMAGE_H-1;i>=DEAL_IMAGE_H-search_stop_line&&i>=0;i--)
    {
        for(j = longest_white_right[1]; j<=DEAL_IMAGE_W-1-2;j++)//从右最长白列找右边
        {
            if(image[i][j] == 1 && image[i][j+1] == 0 && image[i][j+2] == 0)//白黑黑，找到右边界
            {
                if(right_start==0)//右边界起点
                {
                    right_start = j;
                }
                right_border = j;
                right_lost_flag[i]=0;
                break;
            }
            else if(j>=DEAL_IMAGE_W-1-2)//右边界丢失
            {
                if(right_start==0)//左边界起点
                {
                    right_start = j;
                }
                right_border = DEAL_IMAGE_W-1;
                right_lost_flag[i]=1;//右边界丢线记录
                break;
            }
        }
        for(j = longest_white_left[1]; j>=2;j--)//从左最长白列找左边
        {
            if(image[i][j] == 1 && image[i][j-1] == 0 && image[i][j-2] == 0)//白黑黑，找到右边界
            {
                if(left_start==0)//左边界起点
                {
                    left_start = j;
                }
                left_border = j;
                left_lost_flag[i]=0;
                break;
            }
            else if(j<=2)//左边界丢失
            {
                if(left_start==0)//左边界起点
                {
                    left_start = j;
                }
                left_border = 0;
                left_lost_flag[i]=1;//左边界丢线记录
                break;
            }
        }
        left_line[i]=left_border;//存储左边线
        right_line[i]=right_border;//存储右边线
        real_road_wide[i]=right_border-left_border;//存储赛道宽度  
    }

    //边界丢线清零
    left_lost_count=0;
    right_lost_count=0;
    left_right_lost_count=0;
    //记录边界起点
    boundary_start_left=0;
    boundary_start_right=0;

    //记录丢边情况
    for(i=DEAL_IMAGE_H-1;i>=DEAL_IMAGE_H-search_stop_line;i--)
    {
        if(boundary_start_left==0&&left_lost_flag[i]==0)
        {
            boundary_start_left=i;//记录左边界起点
        }

        if(boundary_start_right==0&&right_lost_flag[i]==0)
        {
            boundary_start_right=i;//记录右边界起点
        }

        if(left_lost_flag[i]==1&&right_lost_flag[i]==0)left_lost_count++;//左丢
        if(left_lost_flag[i]==0&&right_lost_flag[i]==1)right_lost_count++;//右边丢
        if(left_lost_flag[i]==1&&right_lost_flag[i]==1)left_right_lost_count++;//丢双边
    }

    cross_judge();//判断十字
	
	zebra_judge_multi();   //  判断斑马线

    if(straight_judge())//判断直线
    {
        straight_flag=1;//直线标志位
    }
    else
    {
        straight_flag=0;//不是直线
    }

    for(i = DEAL_IMAGE_H-1;i>=DEAL_IMAGE_H-search_stop_line&&i>=0;i--)
    {
        mid_line[i]=(left_line[i]+right_line[i])/2;//存储中线
    }
}

///*
// *  大津法自适应阈值
// *  通过遍历灰度图自动计算二值化阈值，用于解决光线变化
// *  隔点采样（每2个像素取1个），计算量降至 1/4
// */
//uint8 otsuThreshold(uint8 *image)
//{
//    #define GRAY_SCALE 256
//    static uint8 last_threshold = 64;
//    int pixel_count[GRAY_SCALE];
//    float pixel_pro[GRAY_SCALE];
//    int i, j;
//    int pixel_max = 0, pixel_min = 255;
//    uint16 width  = MT9V03X_W;
//    uint16 height = MT9V03X_H;
//    int pixel_sum  = width * height / 4;
//    uint8 threshold = 0;
//    uint8 *data = image;
//    uint32 gray_sum = 0;
//
//    for (i = 0; i < GRAY_SCALE; i++)
//    {
//        pixel_count[i] = 0;
//        pixel_pro[i]   = 0;
//    }
//
//    /* 隔点采样统计直方图 */
//    for (i = 0; i < height; i += 2)
//    {
//        for (j = 0; j < width; j += 2)
//        {
//            uint8 val = data[i * width + j];
//            pixel_count[val]++;
//            gray_sum += val;
//            if (val > pixel_max) pixel_max = val;
//            if (val < pixel_min) pixel_min = val;
//        }
//    }
//
//    /* 计算每个灰度级的比例 */
//    for (i = pixel_min; i < pixel_max; i++)
//    {
//        pixel_pro[i] = (float)pixel_count[i] / pixel_sum;
//    }
//
//    /* OTSU 类间方差最大化 */
//    float w0, w1, u0tmp, u1tmp, u0, u1, delta_tmp, delta_max = 0;
//    w0 = w1 = u0tmp = u1tmp = u0 = u1 = delta_tmp = 0;
//
//    for (j = pixel_min; j < pixel_max; j++)
//    {
//        w0    += pixel_pro[j];
//        u0tmp += j * pixel_pro[j];
//
//        w1    = 1 - w0;
//        u1tmp = (float)gray_sum / pixel_sum - u0tmp;
//
//        u0        = u0tmp / w0;
//        u1        = u1tmp / w1;
//        delta_tmp = w0 * w1 * (u0 - u1) * (u0 - u1);
//
//        if (delta_tmp > delta_max)
//        {
//            delta_max = delta_tmp;
//            threshold = (uint8)j;
//        }
//        if (delta_tmp < delta_max)
//        {
//            break;
//        }
//    }
//
//    /* 阈值合理性检查：出现异常值时沿用上一次有效阈值 */
//    if (threshold > 90 && threshold < 130)
//        last_threshold = threshold;
//    else
//        threshold = last_threshold;
//
//    return threshold;
//}

/**
*
*  计算某几行的平均误差，可利用菜单调节
*  start_point = 20
*  end_point = 90
*  err 误差值
*  调节start_point和end_point来决定视野范围（50~119）
**/
float err_sum_average(uint8 start_point,uint8 end_point)
{
	
//	static float err_kp1 = 7.0f;
//	static float err_kp2 = 0.7f;
    //防止参数输入错误
    if(end_point<start_point)
    {
        uint8 t=end_point;
        end_point=start_point;
        start_point=t;
    }

    if(start_point<DEAL_IMAGE_H-search_stop_line)start_point=DEAL_IMAGE_H-search_stop_line-1;//防止起点越界
    if(end_point<DEAL_IMAGE_H-search_stop_line)end_point=DEAL_IMAGE_H-search_stop_line-2;//防止终点越界

//#if 0  /* ---- 旧版：按误差大小非线性加权 ---- */
//    float start_err = DEAL_IMAGE_W/2 - ((left_line[start_point]+right_line[start_point])>>1);
//    float end_err   = DEAL_IMAGE_W/2 - ((left_line[end_point-1]+right_line[end_point-1])>>1);
//    float mid_err = (start_err + end_err) / 2.0f;
//
//    float err=0;
//    for(int i=start_point;i<end_point;i++)
//    {
//        float single_err = DEAL_IMAGE_W/2 - ((left_line[i]+right_line[i])>>1);
//        if(single_err < mid_err)
//            err += single_err * err_kp1;
//        else
//            err += single_err * err_kp2;
//    }
//    err=err/(end_point-start_point);
//    return err;
//#endif

    /* ---- 新版：近处加权 → 急弯走内圈 ---- */
    float err = 0, total_w = 0;
    for(int i = start_point; i < end_point; i++) {
        float e = (float)DEAL_IMAGE_W/2
                - (float)((left_line[i] + right_line[i]) >> 1);

        /* 权重: 远处→近处 线性过渡, 改下面两个数即可 */
        float near_w = 6.0f;   /* 近处(车头)权重 */
        float far_w  = 12.0f;   /* 远处(前方)权重 */
        float t = (float)(i - start_point) / (float)(end_point - start_point);
        float w = far_w + (near_w - far_w) * t;

        err += w * e;
        total_w += w;
    }
    return err / total_w;
}

/*
 * 左边补线
 * x1,y1 起点坐标
 * x2,y2 终点坐标
 */
void left_draw_line(uint8 x1,uint8 y1,uint8 x2,uint8 y2)
{
    uint8 hx;
    uint8 a1=y1;
    uint8 a2=y2;
    //防止越界以及参数输入错误
    if(y1>y2)
    {
        uint8 t=y1;
        y1=y2;
        y2=t;
    }

    if(x1>=DEAL_IMAGE_W-1)x1=DEAL_IMAGE_W-1;
    else if(x1<=0)x1=0;
    if(y1>=DEAL_IMAGE_H-1)y1=DEAL_IMAGE_H-1;
    else if(y1<=0)y1=0;

    if(x2>=DEAL_IMAGE_W-1)x2=DEAL_IMAGE_W-1;
    else if(x2<=0)x2=0;
    if(y2>=DEAL_IMAGE_H-1)y2=DEAL_IMAGE_H-1;
    else if(y2<=0)y2=0;

    for(uint8 i=a1;i<a2;i++)
    {
        hx=x1+(i-y1)*(x2-x1)/(y2-y1);//使用斜率补线
        //防止补线越界
        if(hx>=DEAL_IMAGE_W-1)hx=DEAL_IMAGE_W-1;
        else if(hx<=0)hx=0;
        left_line[i]=hx;
    }
}

/*
 * 右边补线
 * x1,y1 起点坐标
 * x2,y2 终点坐标
 */
void right_draw_line(uint8 x1,uint8 y1,uint8 x2,uint8 y2)
{
    uint8 hx;
    uint8 a1=y1;
    uint8 a2=y2;
    //防止越界以及参数输入错误
    if(y1>y2)
    {
        uint8 t=y1;
        y1=y2;
        y2=t;
    }

    if(x1>=DEAL_IMAGE_W-1)x1=DEAL_IMAGE_W-1;
    else if(x1<=0)x1=0;
    if(y1>=DEAL_IMAGE_H-1)y1=DEAL_IMAGE_H-1;
    else if(y1<=0)y1=0;

    if(x2>=DEAL_IMAGE_W-1)x2=DEAL_IMAGE_W-1;
    else if(x2<=0)x2=0;
    if(y2>=DEAL_IMAGE_H-1)y2=DEAL_IMAGE_H-1;
    else if(y2<=0)y2=0;

    for(uint8 i=a1;i<a2;i++)
    {
        hx=x1+(i-y1)*(x2-x1)/(y2-y1);//使用斜率补线
        //防止补线越界
        if(hx>=DEAL_IMAGE_W-1)hx=DEAL_IMAGE_W-1;
        else if(hx<=0)hx=0;
        right_line[i]=hx;
    }
}

/*
 * 左边界延长
 * start_point 延长起点
 * end_point   延长终点
 */
void extend_left_line(uint8 start_point, uint8 end_point)
{
    float k;
    // 防止越界
    if (start_point >= DEAL_IMAGE_H - 1) start_point = DEAL_IMAGE_H - 1;
    if (start_point < 0) start_point = 0;
    if (end_point >= DEAL_IMAGE_H - 1) end_point = DEAL_IMAGE_H - 1;
    if (end_point < 0) end_point = 0;

    // 确保起点大于终点
    if (start_point < end_point) {
        uint8 t = start_point;
        start_point = end_point;
        end_point = t;
    }

    // 如果起点过于靠下，直接连线
    if (start_point >= DEAL_IMAGE_H - 6) {
        left_draw_line(left_line[start_point], start_point, left_line[end_point], end_point);
    } else {
        // 计算斜率
        k = (float)(left_line[start_point] - left_line[start_point + 4]) / 5.0;
        
        // 从起点向上延长
        for (int16_t i = start_point; i >= end_point; i--) {
            left_line[i] = left_line[start_point] + (int)((i - start_point) * (-k));// 使用斜率延长(负斜率)
            
            // 防止越界
            if (left_line[i] < 1) {
                left_line[i] = 1;
            }
            if (left_line[i] >= DEAL_IMAGE_W - 2) {
                left_line[i] = DEAL_IMAGE_W - 2;
            }
        }
    }
}

/*
 * 右边界延长
 * start_point 延长起点
 * end_point   延长终点
 */
void extend_right_line(uint8 start_point, uint8 end_point)
{
    float k;
    //防止越界
    if(start_point>=DEAL_IMAGE_H-1)start_point=DEAL_IMAGE_H-1;
    if(start_point<0)start_point=0;
    if(end_point>=DEAL_IMAGE_H-1)end_point=DEAL_IMAGE_H-1;
    if(end_point<0)end_point=0;
    
    if(end_point<start_point)
    {
        uint8 t=start_point;
        start_point=end_point;
        end_point=t;
    }

    if(start_point<=5)//起点过于靠上，直接连线
    {
        right_draw_line(right_line[start_point],start_point,right_line[end_point],end_point);
    }
    else
    {
        k=(float)(right_line[start_point]-right_line[start_point-4])/5.0;//斜率
        for(uint8 i=start_point;i<=end_point;i++)
        {
            right_line[i]=right_line[start_point]+(int)(i-start_point)*k;//使用斜率延长

            if(right_line[i]<1)//防止越界
            {
                right_line[i]=1;
            }
            
            if(right_line[i]>=DEAL_IMAGE_W-2)//防止越界
            {
                right_line[i]=DEAL_IMAGE_W-2;
            }
        }
    }
}

/**
*
* @brief  道宽半边补左线
**/
void road_wide_draw_left_line(void)
{
    for(int i=0;i<DEAL_IMAGE_H-1;i++)
    {
        left_line[i]=right_line[i]-road_wide[i];
        if(left_line[i]<1)//防止越界
        {
            left_line[i]=1;
        }
    }
}

/**
*
* @brief  道宽半边补右线
**/
void road_wide_draw_right_line(void)
{
    for(int i=0;i<DEAL_IMAGE_H-1;i++)
    {
        right_line[i]=left_line[i]+road_wide[i];
        if(right_line[i]>=DEAL_IMAGE_W-2)//防止越界
        {
            right_line[i]=DEAL_IMAGE_W-2;
        }
    }
}

/**
*
* @brief  外切补线
**/
void road_wide_fill_lost_line(void)
{
    if (cross_flag || circle_flag)
        return;

    if (left_lost_count > 20 && right_lost_count > 0 &&  right_lost_count < 5)
    {
        road_wide_draw_left_line();
    }
    else if (right_lost_count > 20 && left_lost_count > 0 && left_lost_count < 5)
    {
        road_wide_draw_right_line();
    }
    else
    {
        return;
    }

    /* 补线后重算中线 */
    for (int i = DEAL_IMAGE_H - 1; i >= DEAL_IMAGE_H - search_stop_line && i >= 0; i--)
    {
        mid_line[i] = (left_line[i] + right_line[i]) / 2;
    }
}

/**
*
* @brief  右边丢线补左边
**/
void right_lose_draw_left_line(void)
{
    for(int i=0;i<DEAL_IMAGE_H-1;i++)
    {
        left_line[i]=right_line[i]-road_wide[i];
        if(left_line[i]<1)//防止越界
        {
            left_line[i]=1;
        }
    }
}

/**
*
* @brief  左边丢线补右边
**/
void left_lose_draw_right_line(void)
{
    for(int i=0;i<DEAL_IMAGE_H-1;i++)
    {
        right_line[i]=left_line[i]+road_wide[i];
        if(right_line[i]>=DEAL_IMAGE_W-2)//防止越界
        {
            right_line[i]=DEAL_IMAGE_W-2;
        }
    }
}

/**
*
* @brief  内切补线
**/
void inner_draw_line(void)
{
    if (cross_flag || circle_flag)
        return;

    if (left_lost_count > 20 && right_lost_count > 5 &&  right_lost_count < 30)
    {
        left_lose_draw_right_line();
    }
    else if (right_lost_count > 20 && left_lost_count > 5 && left_lost_count < 30)
    {
        right_lose_draw_left_line();
    }
    else
    {
        return;
    }

    /* 补线后重算中线 */
    for (int i = DEAL_IMAGE_H - 1; i >= DEAL_IMAGE_H - search_stop_line && i >= 0; i--)
    {
        mid_line[i] = (left_line[i] + right_line[i]) / 2;
    }
}

/*
 * 找下拐点
 * start_point 搜索起点
 * end_point 搜索终点
 */
void find_down_point(uint8 start_point,uint8 end_point)
{
    //参数清零
    left_down_point=0;
    right_down_point=0;
    if(start_point<end_point)
    {
        uint8 t=start_point;
        start_point=end_point;
        end_point=t;
    }
    if(start_point>DEAL_IMAGE_H-5-1)
    {
        start_point=DEAL_IMAGE_H-5-1;
    }
    if(end_point<DEAL_IMAGE_H-search_stop_line)
    {
        end_point=DEAL_IMAGE_H-search_stop_line;
    }
    if(end_point<5)
    {
        end_point=5;
    }
    for(int i=start_point;i>=end_point;i--)
    {
        //点i下面2个连续相差不大并且点i与上面边3个点分别相差很大，认为有下左拐点
        if(left_down_point==0&&
            abs(left_line[i]-left_line[i+1])<=7&&
            abs(left_line[i+1]-left_line[i+2])<=7&&
            abs(left_line[i+2]-left_line[i+3])<=7&&
            (left_line[i]-left_line[i-2])>=8&&
            (left_line[i]-left_line[i-3])>=8&&
            (left_line[i]-left_line[i-4])>=8)
            {
                left_down_point=i+3;
            }
        if(right_down_point==0&&
            abs(right_line[i]-right_line[i+1])<=6&&
            abs(right_line[i+1]-right_line[i+2])<=6&&
            abs(right_line[i+2]-right_line[i+3])<=6&&
            (right_line[i]-right_line[i-2])<=-8&&
            (right_line[i]-right_line[i-3])<=-8&&
            (right_line[i]-right_line[i-4])<=-8)
            {
                right_down_point=i+3;
            }
        if(left_down_point!=0&&right_down_point!=0)
        {
            break;
        }       
    }
}

/*
 * 找上拐点
 * start_point 搜索起点
 * end_point 搜索终点
 */
void find_up_point(uint8 start_point,uint8 end_point)
{
    left_up_point=0;
    right_up_point=0;
    if(start_point<end_point)
    {
        uint8 t=start_point;
        start_point=end_point;
        end_point=t;
    }

    if(start_point>DEAL_IMAGE_H-5-1)
    {
        start_point=DEAL_IMAGE_H-5-1;
    }

    if(end_point<DEAL_IMAGE_H-search_stop_line)
    {
        end_point=DEAL_IMAGE_H-search_stop_line;
    }

    if(end_point<5)
    {
        end_point=5;
    }

    for(int i=start_point;i>=end_point;i--)
    {
        //点i下面2个连续相差不大并且点i与上面边3个点分别相差很大，认为有上左拐点
        if(left_up_point==0&&
            abs(left_line[i]-left_line[i-1])<=5&&
            abs(left_line[i-1]-left_line[i-2])<=5&&
            abs(left_line[i-2]-left_line[i-3])<=5&&
            (left_line[i]-left_line[i+2])>=8&&
            (left_line[i]-left_line[i+3])>=15&&
            (left_line[i]-left_line[i+4])>=15)
            {
                left_up_point=i-3;
            }
        if(right_up_point==0&&
            abs(right_line[i]-right_line[i-1])<=3&&
            abs(right_line[i-1]-right_line[i-2])<=3&&
            abs(right_line[i-2]-right_line[i-3])<=3&&
            (right_line[i]-right_line[i+2])<=-8&&
            (right_line[i]-right_line[i+3])<=-15&&
            (right_line[i]-right_line[i+4])<=-15)
            {
                right_up_point=i-3;
            }
        if(left_up_point!=0&&right_up_point!=0)
        {
            break;
        }       
    }
}

/**
*
* @brief  判断直道
* @retval 直道返回1，非直道返回0
**/
uint8 straight_judge(void)
{
    if(search_stop_line>=110)
    {
        if(boundary_start_left>=115&&boundary_start_right>=115&&left_lost_count<10&&right_lost_count<10&&left_right_lost_count<10)
//		if(boundary_start_left>=60&&boundary_start_right>=60&&left_lost_count<70&&right_lost_count<70&&left_right_lost_count<70)
        {
            if(fabsf(line_err)<=7)
            {
                return 1;//直道
            }
            else
            {
                return 0;//非直道
            }
        }
		else
		{
			return 0;//非直道
		}
    }
    else
    {
        return 0;//非直道
    }
}

/**
* 判断十字路口并补线
**/
void cross_judge(void)
{

    if(!circle_flag)
    {
        if(left_right_lost_count>10)
        {
            find_up_point(MT9V03X_H-1,0);//寻找上拐点

            find_down_point(MT9V03X_H-1,(left_up_point+right_up_point)/2);//寻找下拐点

            if(right_up_point&&left_up_point)
            {
                cross_flag=1;//十字标志置1

                if(left_down_point&&right_down_point)//如果四个拐点都存在
                {
                    left_draw_line(left_line[left_up_point],left_up_point,left_line[left_down_point],left_down_point);//左边补线
                    right_draw_line(right_line[right_up_point],right_up_point,right_line[right_down_point],right_down_point);//右边补线
                }
                else if(left_down_point&&!right_down_point)//如果左边有下拐点，右边没有
                {
                    left_draw_line(left_line[left_up_point],left_up_point,left_line[left_down_point],left_down_point);//左边补线
                    extend_right_line(right_up_point-1,DEAL_IMAGE_H-1);//右边延长
                }
                else if(!left_down_point&&right_down_point)//如果右边有下拐点，左边没有
                {
                    right_draw_line(right_line[right_up_point],right_up_point,right_line[right_down_point],right_down_point);//右边补线
                    extend_left_line(left_up_point-1,DEAL_IMAGE_H-1);//左边延长
                }
                else if(!left_down_point&&!right_down_point)//如果四个拐点都不存在
                {
                    extend_left_line(left_up_point-1,DEAL_IMAGE_H-1);//左边延长
                    extend_right_line(right_up_point-1,DEAL_IMAGE_H-1);//右边延长
                }
            }
            else
            {
                cross_flag=0;//十字标志清零
            } 
        }
    }
}

/**
* 斑马线判定
**/
void zebra_judge_multi(void)
{
    uint8 zebra_count = 0;
    uint8 zebra_detected = 0;  // 当前帧是否检测到斑马线
    
    // 基本条件检查
    if(longest_white_left[1] > 20 && longest_white_right[1] < DEAL_IMAGE_W - 20 &&
       longest_white_right[1] > 20 && longest_white_left[1] < DEAL_IMAGE_W - 20 &&
       search_stop_line >= 110 &&
       boundary_start_left >= DEAL_IMAGE_H - 20 &&
       boundary_start_right >= DEAL_IMAGE_H - 20)
    {
        // 检测斑马线特征
        for(int i = DEAL_IMAGE_H - 1; i >= DEAL_IMAGE_H - 3; i--) 
        {
            zebra_count = 0;  // 每行重新计数
            for(int j = 0; j <= DEAL_IMAGE_W - 1 - 3; j++)
            {
                // 检测白黑黑模式
                if(binary_image[i][j] == 1 && binary_image[i][j+1] == 0 && binary_image[i][j+2] == 0)
                {
                    zebra_count++;
                }
            }
            
            // 如果某一行的跳变次数足够多，认为检测到斑马线
            if(zebra_count >= 4)  // 适当降低阈值提高检测率
            {
                zebra_detected = 1;
                break;
            }
        }
    }
    
    // 状态机处理斑马线识别
    switch(zebra_detect_state)
    {
        case 0:  // 未检测状态
            if(zebra_detected)
            {
                zebra_detect_state = 1;  // 进入检测中状态
                zebra_clear_timer = 0;
                zebra_flag = 1;
                zebra_count_total++;     // 斑马线计数加1
            }
            else
            {
                zebra_flag = 0;
            }
            break;
            
        case 1:  // 检测中状态
            if(zebra_detected)
            {
                zebra_flag = 1;
                zebra_clear_timer = 0;  // 重置计时器
            }
            else
            {
                zebra_clear_timer++;
                if(zebra_clear_timer >= 5)  // 连续5帧未检测到，进入已通过状态
                {
                    zebra_detect_state = 2;
                    zebra_flag = 0;
                    zebra_clear_timer = 0;
                }
            }
            break;
            
        case 2:  // 已通过状态
            zebra_flag = 0;
            zebra_clear_timer++;
            if(zebra_clear_timer >= 20)  // 等待20帧后恢复到未检测状态
            {
                zebra_detect_state = 0;
                zebra_clear_timer = 0;
            }
            break;
    }
     
    zebra_last_flag = zebra_flag;
}
