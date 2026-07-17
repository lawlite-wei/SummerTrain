#include "camera.h"
/*==================== Camera 图像显示 ====================*/
uint8 binarization_threshold = 64;            /* 二值化阈值（大津法动态更新） */
volatile uint8 otsu_update_flag = 0;          /* 大津法更新请求标志（PIT中断置1） */
uint8 otsu_enable = 0;                        /* 大津法使能（进入摄像头显示时置1） */

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
uint8 err_start_point = 20; //误差起始点
uint8 err_end_point = 90;   //误差终止点


/*
 *  灰度显示：持续刷新摄像头灰度图像，按下 button1(返回) 退出
 */
void show_gary(void)
{
    int i, j;
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

    otsu_enable = 1;  /* 启动大津法定时更新 */

    while (1) {
        if (mt9v03x_finish_flag) {
            /* PIT中断每5ms置位，帧完整时更新阈值 */
            if (otsu_update_flag) {
                otsu_update_flag = 0;
                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
            }

            /* 二值化：灰度转0/1，供最长白列算法使用 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization_threshold) ? 1 : 0;
                }
            }

            /* 显示灰度图像（y=30 与 show_boundary_line 对齐） */
            ips200_show_gray_image(0, 30, (const uint8 *)mt9v03x_image,
                                   MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H, 0);

            /* 最长白列巡线 + 边界叠加显示 */
            boundary_line_init();
            longest_white_sweepline(binary_image);
            show_boundary_line();

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

    otsu_enable = 0;  /* 退出摄像头显示，停止大津法更新 */
}

/*
 *  二值化显示：持续刷新摄像头二值化图像，按下 button1(返回) 退出
 */
void show_binarize(void)
{
    int i, j;
    ips200_clear();
    ips200_show_string(0, 300, "KEY1: back");

    otsu_enable = 1;  /* 启动大津法定时更新 */

    while (1) {
        if (mt9v03x_finish_flag) {
            /* PIT中断每5ms置位，帧完整时更新阈值 */
            if (otsu_update_flag) {
                otsu_update_flag = 0;
                binarization_threshold = otsuThreshold((uint8 *)mt9v03x_image);
            }

            /* 二值化：灰度转0/1，供最长白列算法使用 */
            for (i = 0; i < DEAL_IMAGE_H; i++) {
                for (j = 0; j < DEAL_IMAGE_W; j++) {
                    binary_image[i][j] = (mt9v03x_image[i][j] >= binarization_threshold) ? 1 : 0;
                }
            }

            /* 显示二值化图像（y=30 与 show_boundary_line 对齐） */
            ips200_show_gray_image(0, 30, (const uint8 *)mt9v03x_image,
                                   MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H,
                                   binarization_threshold);

            /* 最长白列巡线 + 边界叠加显示 */
            boundary_line_init();
            longest_white_sweepline(binary_image);
            show_boundary_line();

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

    otsu_enable = 0;  /* 退出摄像头显示，停止大津法更新 */
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
        for(int j=0;j<3;j++)
        {
            sum+=in_image[DEAL_IMAGE_H-1-j][DEAL_IMAGE_W/2-5+i];
        }
    }
    int average = sum / 30;    // 计算平均值
    if(average < 64){return 1;}
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

    for(i = DEAL_IMAGE_H-1;i>=DEAL_IMAGE_H-search_stop_line&&i>=0;i--)
    {
        mid_line[i]=(left_line[i]+right_line[i])/2;//存储中线
    }
}

/*
 *  大津法自适应阈值
 *  通过遍历灰度图自动计算二值化阈值，用于解决光线变化
 *  隔点采样（每2个像素取1个），计算量降至 1/4
 */
uint8 otsuThreshold(uint8 *image)
{
    #define GRAY_SCALE 256
    static uint8 last_threshold = 64;
    int pixel_count[GRAY_SCALE];
    float pixel_pro[GRAY_SCALE];
    int i, j;
    int pixel_max = 0, pixel_min = 255;
    uint16 width  = MT9V03X_W;
    uint16 height = MT9V03X_H;
    int pixel_sum  = width * height / 4;
    uint8 threshold = 0;
    uint8 *data = image;
    uint32 gray_sum = 0;

    for (i = 0; i < GRAY_SCALE; i++)
    {
        pixel_count[i] = 0;
        pixel_pro[i]   = 0;
    }

    /* 隔点采样统计直方图 */
    for (i = 0; i < height; i += 2)
    {
        for (j = 0; j < width; j += 2)
        {
            uint8 val = data[i * width + j];
            pixel_count[val]++;
            gray_sum += val;
            if (val > pixel_max) pixel_max = val;
            if (val < pixel_min) pixel_min = val;
        }
    }

    /* 计算每个灰度级的比例 */
    for (i = pixel_min; i < pixel_max; i++)
    {
        pixel_pro[i] = (float)pixel_count[i] / pixel_sum;
    }

    /* OTSU 类间方差最大化 */
    float w0, w1, u0tmp, u1tmp, u0, u1, delta_tmp, delta_max = 0;
    w0 = w1 = u0tmp = u1tmp = u0 = u1 = delta_tmp = 0;

    for (j = pixel_min; j < pixel_max; j++)
    {
        w0    += pixel_pro[j];
        u0tmp += j * pixel_pro[j];

        w1    = 1 - w0;
        u1tmp = (float)gray_sum / pixel_sum - u0tmp;

        u0        = u0tmp / w0;
        u1        = u1tmp / w1;
        delta_tmp = w0 * w1 * (u0 - u1) * (u0 - u1);

        if (delta_tmp > delta_max)
        {
            delta_max = delta_tmp;
            threshold = (uint8)j;
        }
        if (delta_tmp < delta_max)
        {
            break;
        }
    }

    /* 阈值合理性检查：出现异常值时沿用上一次有效阈值 */
    if (threshold > 90 && threshold < 130)
        last_threshold = threshold;
    else
        threshold = last_threshold;

    return threshold;
}

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
    //防止参数输入错误
    if(end_point<start_point)
    {
        uint8 t=end_point;
        end_point=start_point;
        start_point=t;
    }

    if(start_point<DEAL_IMAGE_H-search_stop_line)start_point=DEAL_IMAGE_H-search_stop_line-1;//防止起点越界
    if(end_point<DEAL_IMAGE_H-search_stop_line)end_point=DEAL_IMAGE_H-search_stop_line-2;//防止终点越界

    float err=0;
    for(int i=start_point;i<end_point;i++)
    {
        err+=(DEAL_IMAGE_W/2-((left_line[i]+right_line[i])>>1));//位操作等效除以2
    }
    err=err/(end_point-start_point);
    return err;
}
