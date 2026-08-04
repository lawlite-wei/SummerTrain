#include "zf_common_headfile.h"

/*==================== 按键状态 ====================*/
/*
 * button1 : 返回 (E3)
 * button2 : 确认 (E4)
 * button3 : 下翻 (E2)
 * button4 : 上翻 (E5)
 */
static uint8 button1 = 0, button2 = 0, button3 = 0, button4 = 0;
static uint8 first_in_page_flag = 0;
static uint8 need_full_redraw   = 0;

/*==================== 链表指针 ====================*/
static menu_unit *p_unit      = NULL;   /* 当前选中单元 */
static menu_unit *p_unit_last = NULL;   /* 上一次选中的单元 */

/*==================== 功能函数指针 ====================*/
static void (*current_operation_menu)(void);

/*==================== 静态内存池 ====================*/
#ifdef USE_STATIC_MENU
static menu_unit    my_menu_unit[MEM_SIZE];
static param_set    my_param_set[MEM_SIZE];
static uint8        static_cnt = 0;
#endif

/*==================== 背景色 ====================*/
uint16 IPS200_BGCOLOR = RGB565_WHITE;

/*==================== 主页面指针 ====================*/
static menu_unit *main_page = NULL;

/*==================== Flash 编辑状态 ====================*/
static uint8  flash_status = 0;   /* 0=无, 1=显示edit, 2=显示save */

/*==================== 内存分配 ====================*/
static menu_unit *alloc_unit(void)
{
#ifdef USE_STATIC_MENU
    menu_unit *p = &my_menu_unit[static_cnt];
    static_cnt++;
    memset(p, 0, sizeof(menu_unit));
    return p;
#else
    menu_unit *p = malloc(sizeof(menu_unit));
    memset(p, 0, sizeof(menu_unit));
    return p;
#endif
}

static param_set *alloc_param(void)
{
#ifdef USE_STATIC_MENU
    param_set *p = &my_param_set[static_cnt];
    static_cnt++;
    memset(p, 0, sizeof(param_set));
    return p;
#else
    param_set *p = malloc(sizeof(param_set));
    memset(p, 0, sizeof(param_set));
    return p;
#endif
}

/*==================== 链表工具函数 ====================*/

static void ring_link(menu_unit *p1, menu_unit *p2)
{
    p1->up   = p2;
    p2->down = p1;
}

/* 遍历环，返回元素个数，同时找到 p_unit 在环中的索引 */
static uint8 ring_count(menu_unit *head, uint8 *cursor_idx)
{
    menu_unit *p = head;
    uint8 count  = 0;
    *cursor_idx  = 0;

    if (head == NULL) return 0;

    do {
        if (p == p_unit) *cursor_idx = count;
        count++;
        p = p->up;
    } while (p != head && count < 32);

    return count;
}

/* 获取当前环的头部 */
static menu_unit *get_ring_head(void)
{
    if (p_unit == NULL) return NULL;
    if (p_unit->is_title) return p_unit->enter;
    if (p_unit->back == NULL) return p_unit;
    if (p_unit->back->is_title) return p_unit->back->enter;
    return p_unit->back->enter;
}

/* 获取当前页面标题 */
static const char *get_page_title(void)
{
    if (p_unit == NULL) return "";
    if (p_unit->is_title) return p_unit->name;
    if (p_unit->back == NULL) return "";
    if (p_unit->back->is_title) return p_unit->back->name;
    return "";
}

/*==================== 菜单构建 API ====================*/

menu_unit *menu_create_page(const char *title)
{
    menu_unit *page = alloc_unit();
    strcpy(page->name, title);
    page->is_title = 1;
    page->type_t   = USE_FUN;
    page->back     = page;
    page->enter    = NULL;
    page->up       = page;
    page->down     = page;
    return page;
}

void menu_add_function(menu_unit *page, const char *name, void (*func)(void))
{
    menu_unit *item = alloc_unit();
    strcpy(item->name, name);
    item->type_t            = USE_FUN;
    item->current_operation = func;
    item->par_set           = NULL;
    item->back              = page;
    item->enter             = item;       /* 叶子节点 */
    item->is_title          = 0;

    if (page->enter == NULL) {
        page->enter  = item;
        item->up     = item;
        item->down   = item;
    } else {
        menu_unit *first = page->enter;
        menu_unit *last  = first->down;
        item->up         = first;
        item->down       = last;
        first->down      = item;
        last->up         = item;
    }
}

void menu_add_submenu(menu_unit *page, const char *name, menu_unit *sub_page)
{
    menu_unit *item = alloc_unit();
    strcpy(item->name, name);
    item->type_t            = USE_FUN;
    item->current_operation = NULL;
    item->par_set           = NULL;
    item->back              = page;
    item->is_title          = 0;
    item->enter             = sub_page->enter;  /* 跳转到子页第一个项目 */

    /* 子页标题的 back 指向此父项目，用于快速返回 */
    sub_page->back = item;

    if (page->enter == NULL) {
        page->enter  = item;
        item->up     = item;
        item->down   = item;
    } else {
        menu_unit *first = page->enter;
        menu_unit *last  = first->down;
        item->up         = first;
        item->down       = last;
        first->down      = item;
        last->up         = item;
    }
}

void menu_add_param(menu_unit *page, const char *name, void *p_param,
                    type_value t, float delta, uint8 num, uint8 point_num,
                    unit_type ut)
{
    menu_unit *plus  = alloc_unit();
    menu_unit *minus = alloc_unit();
    param_set *ps_p  = alloc_param();
    param_set *ps_m  = alloc_param();

    /* + 项 */
    strcpy(plus->name, name);
    plus->name[strlen(name)] = '+';
    plus->par_set = ps_p;
    ps_p->p_par     = p_param;
    ps_p->par_type  = t;
    ps_p->delta     = delta;
    ps_p->num       = num;
    ps_p->point_num = point_num;
    plus->type_t    = ut;
    plus->back      = page;
    plus->enter     = plus;
    plus->is_title  = 0;

    /* - 项 */
    strcpy(minus->name, name);
    minus->name[strlen(name)] = '-';
    minus->par_set = ps_m;
    ps_m->p_par     = p_param;
    ps_m->par_type  = t;
    ps_m->delta     = -delta;
    ps_m->num       = num;
    ps_m->point_num = point_num;
    minus->type_t   = ut;
    minus->back     = page;
    minus->enter    = minus;
    minus->is_title = 0;

    /* +/- 互连 */
    plus->up    = minus;
    plus->down  = minus;
    minus->up   = plus;
    minus->down = plus;

    /* 插入页面环 */
    if (page->enter == NULL) {
        page->enter  = plus;
        plus->up     = plus;
        plus->down   = plus;
    } else {
        menu_unit *first = page->enter;
        menu_unit *last  = first->down;
        plus->up         = first;
        plus->down       = last;
        first->down      = plus;
        last->up         = plus;
    }
}

/*----------------------------------------------------------------------------
 *  @brief      添加原地编辑项（确认进入编辑，上下调值，返回退出）
 *  @param      page        目标页面
 *  @param      name        显示名称（自动追加 ": val"）
 *  @param      p_val       值指针
 *  @param      step        调整步长
 *  @param      min_val     最小值
 *  @param      max_val     最大值
 *  @param      on_change   值变更回调（如写入硬件PWM），可为NULL
 *----------------------------------------------------------------------------*/
void menu_add_inline_edit(menu_unit *page, const char *name,
                          int16 *p_val, int16 step, int16 min_val, int16 max_val,
                          void (*on_change)(int16 val))
{
    char display[STR_LEN_MAX];
    sprintf(display, "%s: %d", name, *p_val);
    menu_add_function(page, display, NULL);

    /* menu_add_function 将新项插为 page->enter->down */
    menu_unit *item = page->enter->down;

    item->par_set = alloc_param();
    item->par_set->p_par      = p_val;
    item->par_set->delta      = step;
    item->par_set->par_type   = TYPE_INT;
    item->par_set->num        = 5;
    item->par_set->point_num  = 0;
    item->par_set->min_val    = min_val;
    item->par_set->max_val    = max_val;
    item->par_set->on_change  = on_change;
    item->current_operation   = NULL;    /* 确保确认键进编辑模式 */
}

/*----------------------------------------------------------------------------
 *  @brief      添加 Flash 可存储编辑项
 *              确认→编辑(右下edit)，上下调值，确认→保存(save)+退出，返回→放弃+退出
 *----------------------------------------------------------------------------*/
void menu_add_flash_edit(menu_unit *page, const char *name,
                         int16 *p_val, int16 step, int16 min_val, int16 max_val,
                         uint16 flash_buf_index,
                         void (*on_change)(int16 val))
{
    menu_add_inline_edit(page, name, p_val, step, min_val, max_val, on_change);
    menu_unit *item = page->enter->down;
    item->par_set->flash_enable    = 1;
    item->par_set->flash_buf_index = flash_buf_index;
}

/*----------------------------------------------------------------------------
 *  @brief      float 版原地编辑项（显示小数，步长可为浮点）
 *----------------------------------------------------------------------------*/
void menu_add_inline_edit_float(menu_unit *page, const char *name,
    float *p_val, float step, float min_val, float max_val,
    uint8 point_num, void (*on_change)(float val))
{
    char display[STR_LEN_MAX];
    if (point_num == 1)
        sprintf(display, "%s: %.1f", name, *p_val);
    else if (point_num == 2)
        sprintf(display, "%s: %.2f", name, *p_val);
    else
        sprintf(display, "%s: %.0f", name, *p_val);

    menu_add_function(page, display, NULL);
    menu_unit *item = page->enter->down;

    item->par_set = alloc_param();
    item->par_set->p_par            = p_val;
    item->par_set->delta            = step;
    item->par_set->par_type         = TYPE_FLOAT;
    item->par_set->num              = 5;
    item->par_set->point_num        = point_num;
    item->par_set->min_val_f        = min_val;
    item->par_set->max_val_f        = max_val;
    item->par_set->on_change_float  = on_change;
    item->current_operation         = NULL;
}

/*----------------------------------------------------------------------------
 *  @brief      float 版 Flash 可存储编辑项
 *              flash_scale: 10=0.1精度, 100=0.01精度（Flash存为int16）
 *----------------------------------------------------------------------------*/
void menu_add_flash_edit_float(menu_unit *page, const char *name,
    float *p_val, float step, float min_val, float max_val,
    uint8 point_num, uint16 flash_buf_index, int16 flash_scale,
    void (*on_change)(float val))
{
    menu_add_inline_edit_float(page, name, p_val, step, min_val, max_val,
                               point_num, on_change);
    menu_unit *item = page->enter->down;
    item->par_set->flash_enable    = 1;
    item->par_set->flash_buf_index = flash_buf_index;
    item->par_set->flash_scale     = flash_scale;
}

/*==================== 显示函数 ====================*/

static void show_current_page(void)
{
    menu_unit  *head;
    uint8       count, cursor_idx, i;
    menu_unit  *p;
    const char *title;

    head   = get_ring_head();
    title  = get_page_title();
    count  = ring_count(head, &cursor_idx);

    /* 全页重绘 */
    if (need_full_redraw) {
        ips200_clear();
        /* 标题 */
        ips200_show_string(0, 0, title);
        /* 项目列表 */
        p = head;
        for (i = 0; i < count; i++) {
            if (i == cursor_idx)
                ips200_show_string(0, DIS_Y * (i + 1), MOUSE_LOOK);
            else
                ips200_show_string(0, DIS_Y * (i + 1), " ");
            ips200_show_string(MOUSE_DIS, DIS_Y * (i + 1), p->name);
            p = p->up;
        }
        /* 清除多余行 */
        for (i = count; i < 10; i++) {
            ips200_show_string(0, DIS_Y * (i + 1), "                ");
        }
        need_full_redraw = 0;
        return;
    }

    /* 仅光标移动 */
    if (button3 || button4) {
        if (count <= 1) return;
        uint8 prev_idx;
        if (button3) {
            prev_idx = (cursor_idx == 0) ? (count - 1) : (cursor_idx - 1);
        } else {
            prev_idx = (cursor_idx == count - 1) ? 0 : (cursor_idx + 1);
        }
        ips200_show_string(0, DIS_Y * (prev_idx + 1), " ");
        ips200_show_string(0, DIS_Y * (cursor_idx + 1), MOUSE_LOOK);
    }

    /* Flash 状态提示 */
    if (flash_status == 1)
        ips200_show_string(180, 288, "edit");
    else if (flash_status == 2) {
        ips200_show_string(180, 288, "save");
        flash_status = 0;   /* save 只显示一帧 */
    } else {
        ips200_show_string(180, 288, "    ");
    }
}

/*==================== 值修改 ====================*/

static void change_value(param_set *param)
{
    uint8  type      = param->par_type;
    float  delta_x   = param->delta;
    void  *value     = param->p_par;
    uint8  num       = param->num;
    uint8  point_num = param->point_num;
    uint16 val_y     = DIS_Y * 9;

    if (value == NULL) return;

    uint8 is_show = (p_unit_last == NULL || p_unit_last->par_set == NULL)
                    ? 1
                    : (p_unit_last->par_set->p_par != param->p_par);

    if (type == TYPE_FLOAT) {
        float *pv = (float *)value;
        if (IS_OK) *pv += delta_x;
        if (is_show) {
            ips200_show_string(0, val_y, "                ");
            ips200_show_float(0, val_y, *pv, num, point_num);
        }
    } else if (type == TYPE_DOUBLE) {
        double *pv = (double *)value;
        if (IS_OK) *pv += (double)delta_x;
        if (is_show) {
            ips200_show_string(0, val_y, "                ");
            ips200_show_float(0, val_y, (float)*pv, num, point_num);
        }
    } else if (type == TYPE_INT) {
        int *pv = (int *)value;
        if (IS_OK) *pv += (int)delta_x;
        if (is_show) {
            ips200_show_string(0, val_y, "                ");
            ips200_show_int(0, val_y, *pv, num);
        }
    } else if (type == TYPE_UINT16) {
        uint16 *pv = (uint16 *)value;
        if (IS_OK) *pv += (int)delta_x;
        if (is_show) {
            ips200_show_string(0, val_y, "                ");
            ips200_show_uint(0, val_y, *pv, num);
        }
    } else if (type == TYPE_UINT32) {
        uint32 *pv = (uint32 *)value;
        if (IS_OK) *pv += (int)delta_x;
        if (is_show) {
            ips200_show_string(0, val_y, "                ");
            ips200_show_uint(0, val_y, *pv, num);
        }
    }
}

/*==================== 按键读取 ====================*/

static void key_read(void)
{
    if (key_get_state(KEY_1) == KEY_SHORT_PRESS) {
        button1 = 1;
        key_clear_state(KEY_1);
    }
    if (key_get_state(KEY_2) == KEY_SHORT_PRESS) {
        button2 = 1;
        key_clear_state(KEY_2);
    }
    if (key_get_state(KEY_3) == KEY_SHORT_PRESS) {
        button3 = 1;
        key_clear_state(KEY_3);
    }
    if (key_get_state(KEY_4) == KEY_SHORT_PRESS) {
        button4 = 1;
        key_clear_state(KEY_4);
    }
}

/*==================== Motor 子页相关（show_process 需要引用） ====================*/
static menu_unit *pwm_L_item;
static menu_unit *pwm_R_item;
static menu_unit *motor_page;
static int16     pwm_L_val = 0;
static int16     pwm_R_val = 0;

/*==================== Flash 常量 ====================*/
#define PID_FLASH_SECTOR    127
#define PID_FLASH_PAGE      3

static void flash_save_value(uint16 buf_idx, int16 val)
{
    flash_read_page_to_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
    flash_union_buffer[buf_idx].int16_type = val;
    flash_write_page_from_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
}

static void flash_load_params(int16 *vals, uint8 count)
{
    flash_read_page_to_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
    for (uint8 i = 0; i < count; i++) {
        int16 v = flash_union_buffer[i].int16_type;
        /* 未初始化 Flash 值为 0xFFFF → int16 为 -1，默认用 0 */
        vals[i] = (v == -1) ? 0 : v;
    }
}

/*==================== 主循环 ====================*/

void show_process(void *parameter)
{
    static uint8      editing    = 0;
    static int16     *edit_val   = NULL;
    static int16      edit_step;
    static float     *edit_val_f = NULL;
    static float      edit_step_f;
    static uint8      edit_is_float = 0;
    static menu_unit *edit_item  = NULL;
    static char       edit_base[STR_LEN_MAX];
    static uint8      flash_edit = 0;
    static int16      flash_orig_val_i;
    static float      flash_orig_val_f;

    key_read();

    /* ---- 编辑模式 ---- */
    if (editing) {
        uint8  pn = edit_item->par_set->point_num;

        if (edit_is_float) {
            /* === Float 编辑 === */
            float  edit_max_f = edit_item->par_set->max_val_f;
            float  edit_min_f = edit_item->par_set->min_val_f;
            void (*on_chg_f)(float) = edit_item->par_set->on_change_float;

            if (button4 == 1) {
                *edit_val_f += edit_step_f;
                if (*edit_val_f > edit_max_f) *edit_val_f = edit_max_f;
                if (on_chg_f) on_chg_f(*edit_val_f);
                if (pn == 1) sprintf(edit_item->name, "%s: %.1f", edit_base, *edit_val_f);
                else         sprintf(edit_item->name, "%s: %.2f", edit_base, *edit_val_f);
                need_full_redraw = 1;
            } else if (button3 == 1) {
                *edit_val_f -= edit_step_f;
                if (*edit_val_f < edit_min_f) *edit_val_f = edit_min_f;
                if (on_chg_f) on_chg_f(*edit_val_f);
                if (pn == 1) sprintf(edit_item->name, "%s: %.1f", edit_base, *edit_val_f);
                else         sprintf(edit_item->name, "%s: %.2f", edit_base, *edit_val_f);
                need_full_redraw = 1;
            } else if (button1 == 1) {
                if (flash_edit) {
                    *edit_val_f = flash_orig_val_f;
                    if (pn == 1) sprintf(edit_item->name, "%s: %.1f", edit_base, *edit_val_f);
                    else         sprintf(edit_item->name, "%s: %.2f", edit_base, *edit_val_f);
                }
                editing       = 0;
                flash_edit    = 0;
                flash_status  = 0;
                edit_is_float = 0;
                edit_val_f    = NULL;
                edit_item     = NULL;
                need_full_redraw = 1;
            } else if (button2 == 1 && flash_edit) {
                int16 scaled = (int16)(*edit_val_f * edit_item->par_set->flash_scale);
                flash_save_value(edit_item->par_set->flash_buf_index, scaled);
                flash_orig_val_f = *edit_val_f;
                flash_status = 2;
                editing       = 0;
                flash_edit    = 0;
                edit_is_float = 0;
                need_full_redraw = 1;
            }
        } else {
            /* === Int16 编辑（原有逻辑） === */
            int16  edit_max = edit_item->par_set->max_val;
            int16  edit_min = edit_item->par_set->min_val;
            void (*on_chg)(int16) = edit_item->par_set->on_change;

            if (button4 == 1) {
                *edit_val += edit_step;
                if (*edit_val > edit_max) *edit_val = edit_max;
                if (on_chg) on_chg(*edit_val);
                sprintf(edit_item->name, "%s: %d", edit_base, *edit_val);
                need_full_redraw = 1;
            } else if (button3 == 1) {
                *edit_val -= edit_step;
                if (*edit_val < edit_min) *edit_val = edit_min;
                if (on_chg) on_chg(*edit_val);
                sprintf(edit_item->name, "%s: %d", edit_base, *edit_val);
                need_full_redraw = 1;
            } else if (button1 == 1) {
                if (flash_edit) {
                    *edit_val = flash_orig_val_i;
                    sprintf(edit_item->name, "%s: %d", edit_base, *edit_val);
                }
                editing     = 0;
                flash_edit  = 0;
                flash_status = 0;
                edit_val    = NULL;
                edit_item   = NULL;
                need_full_redraw = 1;
            } else if (button2 == 1 && flash_edit) {
                flash_save_value(edit_item->par_set->flash_buf_index, *edit_val);
                flash_orig_val_i = *edit_val;
                flash_status = 2;
                editing      = 0;
                flash_edit   = 0;
                need_full_redraw = 1;
            }
        }

        show_current_page();
        p_unit_last = p_unit;
        button1 = button2 = button3 = button4 = 0;
        return;
    }

    /* ---- 普通模式 ---- */
    if (!(button1 || button2 || button3 || button4)) {
        /* 响应异步重绘请求（如 imu_menu_update 更新了数值） */
        if (need_full_redraw)
            show_current_page();
        return;
    }

    first_in_page_flag = (p_unit_last != p_unit) && (button1 || button2);

    /* 导航 */
    if (button1 == 1) {
        /* 主页面不响应返回键：p_unit 在主页面上则忽略 */
        if (!(p_unit->back == main_page || p_unit == main_page)) {
            if (!p_unit->is_title) {
                /* 退出 Motor 子页时停转电机 */
                if (p_unit->back == motor_page) {
                    motor_set_pwm(DIR_L, PWM_L, 0);
                    motor_set_pwm(DIR_R, PWM_R, 0);
                    pwm_L_val = 0;
                    pwm_R_val = 0;
                }
                p_unit = p_unit->back;
                if (p_unit->is_title && p_unit != main_page)
                    p_unit = p_unit->back;
                need_full_redraw = 1;
            }
        }
    } else if (button2 == 1) {
        if (p_unit->is_title) {
            p_unit = p_unit->enter;
            need_full_redraw = 1;
        } else if (p_unit->enter == p_unit) {
            /* 叶子节点 */
            if (p_unit->current_operation != NULL) {
                p_unit->current_operation();
            } else if (p_unit->par_set != NULL && p_unit->par_set->p_par != NULL) {
                /* 进入原地编辑模式 */
                editing = 1;
                edit_item = p_unit;
                strcpy(edit_base, p_unit->name);
                char *c = strchr(edit_base, ':');
                if (c) *c = '\0';

                if (p_unit->par_set->par_type == TYPE_FLOAT) {
                    edit_is_float = 1;
                    edit_val_f   = (float *)p_unit->par_set->p_par;
                    edit_step_f  = p_unit->par_set->delta;
                    if (p_unit->par_set->flash_enable) {
                        flash_edit = 1;
                        flash_orig_val_f = *edit_val_f;
                        flash_status = 1;
                    }
                } else {
                    edit_is_float = 0;
                    edit_val   = (int16 *)p_unit->par_set->p_par;
                    edit_step  = (int16)p_unit->par_set->delta;
                    if (p_unit->par_set->flash_enable) {
                        flash_edit = 1;
                        flash_orig_val_i = *edit_val;
                        flash_status = 1;
                    }
                }
            }
        } else {
            p_unit = p_unit->enter;
            need_full_redraw = 1;
        }
    } else if (button3 == 1) {
        if (!p_unit->is_title)
            p_unit = p_unit->up;
    } else if (button4 == 1) {
        if (!p_unit->is_title)
            p_unit = p_unit->down;
    }

    show_current_page();

    p_unit_last = p_unit;
    button1 = button2 = button3 = button4 = 0;
}

/*==================== 构建菜单树 ====================*/
static menu_unit *debug_page;
static menu_unit *pid_page;
static menu_unit *camera_page;
static menu_unit *speed_pid_page;
static menu_unit *speed_pid_L_page;
static menu_unit *speed_pid_R_page;
static menu_unit *track_pid_page;
static menu_unit *gyro_pid_page;
static menu_unit *image_pid_page;

/* PID 存储变量（Flash ↔ 菜单双向同步，on_change 写入实际 PID 结构体） */
static float speed_Kp, speed_Ki, speed_Kd;
static float speed_L_Kp, speed_L_Ki, speed_L_Kd;
static float speed_R_Kp, speed_R_Ki, speed_R_Kd;
static int16 track_kp, track_kd, track_kp2, track_kd2;
static float gyro_Kp, gyro_Ki, gyro_Kd;
static float image_Kp, image_Kd, image_Kp2;

/* Flash 缓冲区索引 */
#define FIDX_SPEED_KP   0
#define FIDX_SPEED_KI   1
#define FIDX_SPEED_KD   2
#define FIDX_TRACK_KP   3
#define FIDX_TRACK_KD   4
#define FIDX_TRACK_KP2  5
#define FIDX_TRACK_KD2  6
#define FIDX_GYRO_KP    7
#define FIDX_GYRO_KD    8
#define FIDX_GYRO_KI    9
#define FIDX_IMAGE_KP   10
#define FIDX_SPEED_L_KP 11
#define FIDX_SPEED_L_KI 12
#define FIDX_SPEED_L_KD 13
#define FIDX_SPEED_R_KP 14
#define FIDX_SPEED_R_KI 15
#define FIDX_SPEED_R_KD 16
#define FIDX_IMAGE_KD   17
#define FIDX_IMAGE_KP2  18

static void pwm_L_on_change(int16 val) { motor_set_pwm(DIR_L, PWM_L, (uint32)val); }
static void pwm_R_on_change(int16 val) { motor_set_pwm(DIR_R, PWM_R, (uint32)val); }

/* PID 值变更时同步到实际结构体 */
static void sp_Kp_cb(float v) { speed_pid.Kp = v; }
static void sp_Ki_cb(float v) { speed_pid.Ki = v; }
static void sp_Kd_cb(float v) { speed_pid.Kd = v; }
static void tp_kp_cb(int16 v) { track_pid.kp  = (float)v; }
static void tp_kd_cb(int16 v) { track_pid.kd  = (float)v; }
static void tp_kp2_cb(int16 v) { track_pid.kp2 = v / 100.0f; }
static void tp_kd2_cb(int16 v) { track_pid.kd2 = v / 100.0f; }
static void gp_Kp_cb(float v) { gyro_pid.Kp  = v; }
static void gp_Ki_cb(float v) { gyro_pid.Ki  = v; }
static void gp_Kd_cb(float v) { gyro_pid.Kd  = v; }
static void ip_Kp_cb(float v)  { image_kp_ref = v; }           // 写入图像环基准Kp
static void ip_Kd_cb(float v)  { image_pid_struct.Kd = v; }    // 写入图像环Kd
static void ip_Kp2_cb(float v) { image_pid_struct.Kp2 = v; }   // 写入图像环二次项Kp2
static void spL_Kp_cb(float v) { speed_pid_L.Kp = v; }
static void spL_Ki_cb(float v) { speed_pid_L.Ki = v; }
static void spL_Kd_cb(float v) { speed_pid_L.Kd = v; }
static void spR_Kp_cb(float v) { speed_pid_R.Kp = v; }
static void spR_Ki_cb(float v) { speed_pid_R.Ki = v; }
static void spR_Kd_cb(float v) { speed_pid_R.Kd = v; }

static void build_menu_tree(void)
{
    /* 从 Flash 加载 PID 参数（首次上电用 pid.c 结构体默认值） */
    #define PID_VAL_COUNT 19
    int16 pid_vals[PID_VAL_COUNT];
    flash_load_params(pid_vals, PID_VAL_COUNT);

    /* 辅助宏：从flash读float (×10缩放存储, -1表示未初始化) */
    #define LOAD_FL(v, idx, def) \
        v = (pid_vals[idx] != -1) ? ((float)pid_vals[idx] / 10.0f) : def

    LOAD_FL(speed_Kp, FIDX_SPEED_KP, speed_pid.Kp);
    LOAD_FL(speed_Ki, FIDX_SPEED_KI, speed_pid.Ki);
    LOAD_FL(speed_Kd, FIDX_SPEED_KD, speed_pid.Kd);
    track_kp  = (pid_vals[FIDX_TRACK_KP]  != -1) ? pid_vals[FIDX_TRACK_KP]  : (int16)track_pid.kp;
    track_kd  = (pid_vals[FIDX_TRACK_KD]  != -1) ? pid_vals[FIDX_TRACK_KD]  : (int16)track_pid.kd;
    track_kp2 = (pid_vals[FIDX_TRACK_KP2] != -1) ? pid_vals[FIDX_TRACK_KP2] : (int16)(track_pid.kp2 * 100);
    track_kd2 = (pid_vals[FIDX_TRACK_KD2] != -1) ? pid_vals[FIDX_TRACK_KD2] : (int16)(track_pid.kd2 * 100);
    LOAD_FL(gyro_Kp,  FIDX_GYRO_KP,  gyro_pid.Kp);
    LOAD_FL(gyro_Kd,  FIDX_GYRO_KD,  gyro_pid.Kd);
    gyro_Ki  = (pid_vals[FIDX_GYRO_KI]  != -1) ? ((float)pid_vals[FIDX_GYRO_KI]  / 100.0f) : gyro_pid.Ki;
    LOAD_FL(image_Kp, FIDX_IMAGE_KP, image_kp_ref);
    LOAD_FL(image_Kd, FIDX_IMAGE_KD, image_pid_struct.Kd);
    image_Kp2 = (pid_vals[FIDX_IMAGE_KP2] != -1) ? ((float)pid_vals[FIDX_IMAGE_KP2] / 100.0f) : image_pid_struct.Kp2;
    LOAD_FL(speed_L_Kp, FIDX_SPEED_L_KP, speed_pid_L.Kp);
    LOAD_FL(speed_L_Ki, FIDX_SPEED_L_KI, speed_pid_L.Ki);
    LOAD_FL(speed_L_Kd, FIDX_SPEED_L_KD, speed_pid_L.Kd);
    LOAD_FL(speed_R_Kp, FIDX_SPEED_R_KP, speed_pid_R.Kp);
    LOAD_FL(speed_R_Ki, FIDX_SPEED_R_KI, speed_pid_R.Ki);
    LOAD_FL(speed_R_Kd, FIDX_SPEED_R_KD, speed_pid_R.Kd);

    #undef LOAD_FL

    /* 首次上电时把默认值写入 Flash */
    if (pid_vals[0] == -1) {
        pid_vals[FIDX_SPEED_KP] = (int16)(speed_Kp * 10);
        pid_vals[FIDX_SPEED_KI] = (int16)(speed_Ki * 10);
        pid_vals[FIDX_SPEED_KD] = (int16)(speed_Kd * 10);
        pid_vals[FIDX_TRACK_KP] = track_kp;  pid_vals[FIDX_TRACK_KD] = track_kd;
        pid_vals[FIDX_TRACK_KP2] = track_kp2; pid_vals[FIDX_TRACK_KD2] = track_kd2;
        pid_vals[FIDX_GYRO_KP]  = (int16)(gyro_Kp * 10);
        pid_vals[FIDX_GYRO_KD]  = (int16)(gyro_Kd * 10);
        pid_vals[FIDX_GYRO_KI]  = (int16)(gyro_Ki * 100);
        pid_vals[FIDX_IMAGE_KP] = (int16)(image_Kp * 10);
        pid_vals[FIDX_IMAGE_KD] = (int16)(image_Kd * 10);
        pid_vals[FIDX_IMAGE_KP2] = (int16)(image_Kp2 * 100);
        pid_vals[FIDX_SPEED_L_KP] = (int16)(speed_L_Kp * 10);
        pid_vals[FIDX_SPEED_L_KI] = (int16)(speed_L_Ki * 10);
        pid_vals[FIDX_SPEED_L_KD] = (int16)(speed_L_Kd * 10);
        pid_vals[FIDX_SPEED_R_KP] = (int16)(speed_R_Kp * 10);
        pid_vals[FIDX_SPEED_R_KI] = (int16)(speed_R_Ki * 10);
        pid_vals[FIDX_SPEED_R_KD] = (int16)(speed_R_Kd * 10);
        flash_read_page_to_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
        for (uint8 i = 0; i < PID_VAL_COUNT; i++) flash_union_buffer[i].int16_type = pid_vals[i];
        flash_write_page_from_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
    }
    /* 同步到实际 PID 结构体 */
    sp_Kp_cb(speed_Kp); sp_Ki_cb(speed_Ki); sp_Kd_cb(speed_Kd);
    tp_kp_cb(track_kp); tp_kd_cb(track_kd); tp_kp2_cb(track_kp2); tp_kd2_cb(track_kd2);
    gp_Kp_cb(gyro_Kp); gp_Ki_cb(gyro_Ki); gp_Kd_cb(gyro_Kd);
    ip_Kp_cb(image_Kp); ip_Kd_cb(image_Kd); ip_Kp2_cb(image_Kp2);
    spL_Kp_cb(speed_L_Kp); spL_Ki_cb(speed_L_Ki); spL_Kd_cb(speed_L_Kd);
    spR_Kp_cb(speed_R_Kp); spR_Ki_cb(speed_R_Ki); spR_Kd_cb(speed_R_Kd);

    main_page   = menu_create_page("======MAIN======");
    debug_page  = menu_create_page("--Debug--");
    pid_page    = menu_create_page("--PID--");
    camera_page = menu_create_page("--Camera--");
    motor_page  = menu_create_page("--Motor--");
    speed_pid_page = menu_create_page("Speed PID");
    track_pid_page = menu_create_page("Track PID");
    gyro_pid_page  = menu_create_page("Gyro PID");
    image_pid_page = menu_create_page("Image PID");
    speed_pid_L_page = menu_create_page("Speed PID L");
    speed_pid_R_page = menu_create_page("Speed PID R");

    /* Motor 子页 */
    menu_add_inline_edit(motor_page, "pwm_L", &pwm_L_val, 50, -10000, 10000, pwm_L_on_change);
    menu_add_inline_edit(motor_page, "pwm_R", &pwm_R_val, 50, -10000, 10000, pwm_R_on_change);
    pwm_L_item = motor_page->enter;
    pwm_R_item = pwm_L_item->up;

    /* Speed PID : 变更实时写入 speed_pid 结构体 */
    menu_add_flash_edit_float(speed_pid_page, "Kp", &speed_Kp, 0.5f, -50, 100, 1, FIDX_SPEED_KP, 10, sp_Kp_cb);
    menu_add_flash_edit_float(speed_pid_page, "Ki", &speed_Ki, 0.1f, -10,  50, 1, FIDX_SPEED_KI, 10, sp_Ki_cb);
    menu_add_flash_edit_float(speed_pid_page, "Kd", &speed_Kd, 0.1f, -10,  50, 1, FIDX_SPEED_KD, 10, sp_Kd_cb);

    /* Speed PID L : 左轮速度环 */
    menu_add_flash_edit_float(speed_pid_L_page, "Kp", &speed_L_Kp, 0.1f, -50, 100, 1, FIDX_SPEED_L_KP, 10, spL_Kp_cb);
    menu_add_flash_edit_float(speed_pid_L_page, "Ki", &speed_L_Ki, 0.1f, -10,  50, 1, FIDX_SPEED_L_KI, 10, spL_Ki_cb);
    menu_add_flash_edit_float(speed_pid_L_page, "Kd", &speed_L_Kd, 0.1f, -10,  50, 1, FIDX_SPEED_L_KD, 10, spL_Kd_cb);

    /* Speed PID R : 右轮速度环 */
    menu_add_flash_edit_float(speed_pid_R_page, "Kp", &speed_R_Kp, 0.1f, -50, 100, 1, FIDX_SPEED_R_KP, 10, spR_Kp_cb);
    menu_add_flash_edit_float(speed_pid_R_page, "Ki", &speed_R_Ki, 0.1f, -10,  50, 1, FIDX_SPEED_R_KI, 10, spR_Ki_cb);
    menu_add_flash_edit_float(speed_pid_R_page, "Kd", &speed_R_Kd, 0.1f, -10,  50, 1, FIDX_SPEED_R_KD, 10, spR_Kd_cb);

    /* Track PID : kp(±5), kd(±2), kp2(×100, ±5), kd2(×100, ±5) */
    menu_add_flash_edit(track_pid_page, "kp",  &track_kp,  10, -1000, 1000,  FIDX_TRACK_KP,  tp_kp_cb);
    menu_add_flash_edit(track_pid_page, "kd",  &track_kd,  5, -1000, 1000,  FIDX_TRACK_KD,  tp_kd_cb);
    menu_add_flash_edit(track_pid_page, "kp2", &track_kp2, 10, -1000, 1000, FIDX_TRACK_KP2, tp_kp2_cb);
    menu_add_flash_edit(track_pid_page, "kd2", &track_kd2, 10, -1000, 1000, FIDX_TRACK_KD2, tp_kd2_cb);

    /* Gyro PID : */
    menu_add_flash_edit_float(gyro_pid_page, "Kp", &gyro_Kp, 0.2f, -50, 100, 1, FIDX_GYRO_KP, 10, gp_Kp_cb);
    menu_add_flash_edit_float(gyro_pid_page, "Ki", &gyro_Ki, 0.02f, -5, 5, 2, FIDX_GYRO_KI, 100, gp_Ki_cb);
    menu_add_flash_edit_float(gyro_pid_page, "Kd", &gyro_Kd, 0.1f, -50, 100, 1, FIDX_GYRO_KD, 10, gp_Kd_cb);

    /* Image PID : Kp=基准Kp(动态调整时的参考值), Kd=微分项, Kp2=二次项(弯道加力) */
    menu_add_flash_edit_float(image_pid_page, "Kp", &image_Kp, 0.5f, 0, 100, 1, FIDX_IMAGE_KP, 10, ip_Kp_cb);
    menu_add_flash_edit_float(image_pid_page, "Kd", &image_Kd, 0.1f, 0, 50, 1, FIDX_IMAGE_KD, 10, ip_Kd_cb);
    menu_add_flash_edit_float(image_pid_page, "Kp2",&image_Kp2,0.05f,0,5.0,2,FIDX_IMAGE_KP2,100,ip_Kp2_cb);

    /* Debug 子页 */
    menu_add_submenu(debug_page, "motor",   motor_page);
    menu_add_function(debug_page, "encoder", encoder_test);
    menu_add_function(debug_page, "IMU",        imu_test);
    menu_add_function(debug_page, "speed hold",    speed_hold_test);
    menu_add_function(debug_page, "speed hold LR", speed_hold_LR_test);
    menu_add_function(debug_page, "gyro hold",     gyro_hold_test);
    menu_add_function(debug_page, "ratio calc",   ratio_calc_test);

    /* PID 子页 */
    menu_add_submenu(pid_page, "speed_pid",   speed_pid_page);
    menu_add_submenu(pid_page, "speed_pid_L", speed_pid_L_page);
    menu_add_submenu(pid_page, "speed_pid_R", speed_pid_R_page);
    menu_add_submenu(pid_page, "track_pid",   track_pid_page);
    menu_add_submenu(pid_page, "gyro_pid",  gyro_pid_page);
    menu_add_submenu(pid_page, "image_pid", image_pid_page);
    menu_add_function(pid_page, "Reset PID", reset_pid);

    /* Camera 子页 */
    menu_add_function(camera_page, "gary",     show_gary);
    menu_add_function(camera_page, "binarize", show_binarize);
	
    /* 主页 */
    menu_add_submenu(main_page, "Debug",  debug_page);
    menu_add_submenu(main_page, "PID",    pid_page);
    menu_add_function(main_page, "Start", track_line);
    menu_add_submenu(main_page, "Camera", camera_page);
}

/*==================== 初始化 ====================*/

void menu_init(void)
{
    /* 屏幕初始化 */
    ips200_init(IPS200_TYPE_SPI);
    ips200_set_font(IPS200_8X16_FONT);
    ips200_set_color(RGB565_BLACK, RGB565_WHITE);
    ips200_clear();

    /* 按键初始化（10ms 扫描周期，需在中断/PIT中调用 key_scanner） */
    key_init(10);

    /* 摄像头初始化（失败不阻塞菜单） */
    ips200_show_string(0, 0, "mt9v03x init.");
    if (mt9v03x_init()) {
        ips200_show_string(0, 16, "camera fail, skip.");
        system_delay_ms(500);
    } else {
        ips200_show_string(0, 16, "init success.");
    }

    /* 构建菜单树 */
    build_menu_tree();

    /* 起始位置：主页第一个项目 */
    p_unit      = main_page->enter;
    p_unit_last = NULL;

    /* 首屏绘制 */
    need_full_redraw = 1;
    show_current_page();
}

/*==================== 重绘请求 ====================*/
void menu_request_redraw(void)
{
    need_full_redraw = 1;
}

/*==================== 赛道元素显示 ====================*/
/*
 *  根据判别的元素标志位，在右下角显示类型。仅变化时更新，不刷屏。
 */
void show_saidao_flag(void)
{
    static const char *last_flag = NULL;
    const char *flag;

    /* 出界判断用灰度图像（mt9v03x_image），二值图 average 恒 < 240 */
    if (image_out_of_bounds(mt9v03x_image))
        flag = "out";
    else if (cross_flag)
        flag = "cross";
    else if (zebra_flag)
        flag = "zebra";
    else if (straight_flag)
        flag = "straight";
    else
        flag = "curve";

    if (last_flag != flag) {
        /* 清旧文字再写新文字，不刷全屏 */
        if (last_flag)
            ips200_show_string(150, 300, "        ");
        ips200_show_string(150, 300, flag);
        last_flag = flag;
    }
}

/*==================== 更新页面条目名（值变化后同步显示） ====================*/
static void update_page_item_names(menu_unit *page)
{
    menu_unit *item = page->enter;
    if (!item) return;
    do {
        if (item->par_set && item->par_set->p_par) {
            char base[STR_LEN_MAX];
            strcpy(base, item->name);
            char *c = strchr(base, ':');
            if (c) *c = '\0';

            if (item->par_set->par_type == TYPE_FLOAT) {
                float val = *(float *)item->par_set->p_par;
                uint8 pn = item->par_set->point_num;
                if (pn == 1)
                    sprintf(item->name, "%s: %.1f", base, val);
                else if (pn == 2)
                    sprintf(item->name, "%s: %.2f", base, val);
                else
                    sprintf(item->name, "%s: %.0f", base, val);
            } else {
                int16 val = *(int16 *)item->par_set->p_par;
                sprintf(item->name, "%s: %d", base, val);
            }
        }
        item = item->up;
    } while (item != page->enter);
}

/*==================== Reset PID：清空 Flash，恢复 pid.c 默认值 ====================*/
/* 默认值与 pid.c 同步 */
#define SPD_KP_DEF  7.1f
#define SPD_KI_DEF   1.0f
#define SPD_KD_DEF  0.2f
#define TRK_KP_DEF  120
#define TRK_KD_DEF  60
#define TRK_KP2_DEF  50   /* 0.5 × 100 */
#define TRK_KD2_DEF  60   /* 0.6 × 100 */
#define GYR_KP_DEF  6.0f   /* 角速度环Kp（编码器差速量纲，原地hold起点）*/
#define GYR_KI_DEF   0.14f
#define GYR_KD_DEF  0.0f   /* 角速度环Kd */
#define IMG_KP_DEF   18.0f  /* 图像环基准Kp */
#define IMG_KD_DEF   0.0f   /* 图像环Kd */
#define IMG_KP2_DEF  0.3f   /* 图像环二次项Kp2 */
#define SPD_LR_KP_DEF  7.1f   /* 左右速度环共用 */
#define SPD_LR_KI_DEF  0.8f
#define SPD_LR_KD_DEF  0.0f

void reset_pid(void)
{
    /* 1. 擦除 Flash */
    flash_erase_page(PID_FLASH_SECTOR, PID_FLASH_PAGE);

    /* 2. 恢复硬编码默认值 */
    speed_Kp = SPD_KP_DEF; speed_Ki = SPD_KI_DEF; speed_Kd = SPD_KD_DEF;
    speed_L_Kp = SPD_LR_KP_DEF; speed_L_Ki = SPD_LR_KI_DEF; speed_L_Kd = SPD_LR_KD_DEF;
    speed_R_Kp = SPD_LR_KP_DEF; speed_R_Ki = SPD_LR_KI_DEF; speed_R_Kd = SPD_LR_KD_DEF;
    track_kp = TRK_KP_DEF; track_kd = TRK_KD_DEF;
    track_kp2 = TRK_KP2_DEF; track_kd2 = TRK_KD2_DEF;
    gyro_Kp  = GYR_KP_DEF;  gyro_Ki  = GYR_KI_DEF;  gyro_Kd  = GYR_KD_DEF;
    image_Kp = IMG_KP_DEF;  image_Kd = IMG_KD_DEF;  image_Kp2 = IMG_KP2_DEF;

    /* 3. 写入 Flash（float ×10 存入 int16 槽位） */
    #define FLASH_F(idx, val) flash_union_buffer[idx].int16_type = (int16)((val) * 10.0f)
    flash_read_page_to_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);
    FLASH_F(FIDX_SPEED_KP, speed_Kp); FLASH_F(FIDX_SPEED_KI, speed_Ki); FLASH_F(FIDX_SPEED_KD, speed_Kd);
    flash_union_buffer[FIDX_TRACK_KP].int16_type  = track_kp;
    flash_union_buffer[FIDX_TRACK_KD].int16_type  = track_kd;
    flash_union_buffer[FIDX_TRACK_KP2].int16_type = track_kp2;
    flash_union_buffer[FIDX_TRACK_KD2].int16_type = track_kd2;
    FLASH_F(FIDX_GYRO_KP,  gyro_Kp);  FLASH_F(FIDX_GYRO_KD,  gyro_Kd);
    flash_union_buffer[FIDX_GYRO_KI].int16_type  = (int16)(gyro_Ki * 100);
    FLASH_F(FIDX_IMAGE_KP, image_Kp);  FLASH_F(FIDX_IMAGE_KD, image_Kd);
    flash_union_buffer[FIDX_IMAGE_KP2].int16_type = (int16)(image_Kp2 * 100);
    FLASH_F(FIDX_SPEED_L_KP, speed_L_Kp); FLASH_F(FIDX_SPEED_L_KI, speed_L_Ki); FLASH_F(FIDX_SPEED_L_KD, speed_L_Kd);
    FLASH_F(FIDX_SPEED_R_KP, speed_R_Kp); FLASH_F(FIDX_SPEED_R_KI, speed_R_Ki); FLASH_F(FIDX_SPEED_R_KD, speed_R_Kd);
    #undef FLASH_F
    flash_write_page_from_buffer(PID_FLASH_SECTOR, PID_FLASH_PAGE);

    /* 4. 同步到结构体 */
    sp_Kp_cb(speed_Kp); sp_Ki_cb(speed_Ki); sp_Kd_cb(speed_Kd);
    tp_kp_cb(track_kp); tp_kd_cb(track_kd); tp_kp2_cb(track_kp2); tp_kd2_cb(track_kd2);
    gp_Kp_cb(gyro_Kp); gp_Ki_cb(gyro_Ki); gp_Kd_cb(gyro_Kd);
    ip_Kp_cb(image_Kp); ip_Kd_cb(image_Kd); ip_Kp2_cb(image_Kp2);
    spL_Kp_cb(speed_L_Kp); spL_Ki_cb(speed_L_Ki); spL_Kd_cb(speed_L_Kd);
    spR_Kp_cb(speed_R_Kp); spR_Ki_cb(speed_R_Ki); spR_Kd_cb(speed_R_Kd);

    /* 5. 更新菜单条目显示 */
    update_page_item_names(speed_pid_page);
    update_page_item_names(speed_pid_L_page);
    update_page_item_names(speed_pid_R_page);
    update_page_item_names(track_pid_page);
    update_page_item_names(gyro_pid_page);
    update_page_item_names(image_pid_page);

    menu_request_redraw();
}

/*==================== 空函数 ====================*/
void NULL_FUN(void)
{

}
