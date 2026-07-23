#ifndef _menu_h
#define _menu_h

#include "zf_common_headfile.h"

/*==================== 菜单配置 ====================*/
#define SON_NUM             8           /* 每页最大子单元数 */
#define STR_LEN_MAX         16          /* 菜单项名称最大长度 */
#define MOUSE_DIS           10          /* 光标与文字的距离 */
#define MOUSE_LOOK          ">"         /* 光标符号 */

/*==================== 屏参 (IPS200 240x320) ====================*/
#define DIS_X               120
#define DIS_Y               16
#define SCREEN_W            240
#define SCREEN_H            320

/*==================== 参数类型枚举 ====================*/
typedef enum {
    TYPE_FLOAT = 1,
    TYPE_DOUBLE,
    TYPE_INT,
    TYPE_UINT16,
    TYPE_UINT32
} type_value;

typedef enum {
    USE_FUN = 1,        /* 执行功能函数 */
    NORMAL_PAR,         /* 普通参数调节 */
    PID_PAR             /* PID参数调节 */
} unit_type;

/*==================== 参数结构体 ====================*/
typedef struct {
    void        *p_par;         /* 指向要修改的参数 */
    float       delta;          /* 每次修改的步长 */
    type_value  par_type;       /* 参数类型 */
    uint8       num;            /* 显示整数位数 */
    uint8       point_num;      /* 显示小数位数 */
    int16       min_val;        /* 原地编辑最小值 */
    int16       max_val;        /* 原地编辑最大值 */
    void        (*on_change)(int16 val); /* 值变更时回调（如更新硬件PWM） */
    uint8       flash_enable;   /* Flash存储使能（1=需要断电保存） */
    uint16      flash_buf_index;/* 在flash_union_buffer中的索引 */
} param_set;

/*==================== 菜单单元（双向链表节点） ====================*/
typedef struct MENU_UNIT {
    param_set           *par_set;           /* 参数指针（功能型为NULL） */
    struct MENU_UNIT    *up;                /* 上一个兄弟节点 */
    struct MENU_UNIT    *down;              /* 下一个兄弟节点 */
    struct MENU_UNIT    *enter;             /* 进入子页面 / 自己=叶子节点 */
    struct MENU_UNIT    *back;              /* 返回父页面 */
    void                (*current_operation)(void); /* 功能函数指针 */
    char                name[STR_LEN_MAX];  /* 显示名称 */
    uint8               is_title;           /* 是否为标题行（不可选中） */
    unit_type           type_t;             /* 单元类型 */
} menu_unit;

/*==================== 静态内存池 ====================*/
#define USE_STATIC_MENU
#define MEM_SIZE        60

/*==================== 快捷键 ====================*/
#define IS_OK           ((button2 == 1) && (!first_in_page_flag))

/*==================== 全局变量声明 ====================*/
extern uint16 IPS200_BGCOLOR;

/*==================== 函数声明 ====================*/
void menu_init(void);
void show_process(void *parameter);

/* 菜单构建API */
menu_unit *menu_create_page(const char *title);
void menu_add_function(menu_unit *page, const char *name, void (*func)(void));
void menu_add_submenu(menu_unit *page, const char *name, menu_unit *sub_page);
void menu_add_param(menu_unit *page, const char *name, void *p_param,
                    type_value t, float delta, uint8 num, uint8 point_num,
                    unit_type ut);
void menu_add_inline_edit(menu_unit *page, const char *name,
                          int16 *p_val, int16 step, int16 min_val, int16 max_val,
                          void (*on_change)(int16 val));
void menu_add_flash_edit(menu_unit *page, const char *name,
                         int16 *p_val, int16 step, int16 min_val, int16 max_val,
                         uint16 flash_buf_index,
                         void (*on_change)(int16 val));

/* 菜单重绘请求（外部函数调用后标记需要刷新） */
void menu_request_redraw(void);
/* 赛道元素显示（摄像头图像右下角） */
void show_saidao_flag(void);
/* 重置 PID Flash → pid.c 默认值 */
void reset_pid(void);

/* 空函数 */
void NULL_FUN(void);

/* imu数据刷新 */
#endif
