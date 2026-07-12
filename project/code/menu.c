#include "zf_common_headfile.h"
#include "menu.h"

/*==================== 按键状态 ====================*/
/*
 * button1 : 返回
 * button2 : 确认
 * button3 : 下翻
 * button4 : 上翻
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

/*==================== 主循环 ====================*/

void show_process(void *parameter)
{
    /* 1. 读取按键 */
    key_read();

    if (!(button1 || button2 || button3 || button4)) {
        /* 无按键时仍处理参数显示（首次显示值） */
        if (p_unit != NULL &&
            (p_unit->type_t == NORMAL_PAR || p_unit->type_t == PID_PAR))
            change_value(p_unit->par_set);
        return;
    }

    /* 2. 判断首页标记 */
    first_in_page_flag = (p_unit_last != p_unit) && (button1 || button2);

    /* 3. 导航 */
    if (button1 == 1) {
        /* 返回 */
        if (!p_unit->is_title) {
            p_unit = p_unit->back;
            /* 如果退到了子页标题（非主页），继续退到父项目 */
            if (p_unit->is_title && p_unit != main_page)
                p_unit = p_unit->back;
            need_full_redraw = 1;
        }
    } else if (button2 == 1) {
        if (p_unit->is_title) {
            /* 标题页按确认 → 进入第一个项目 */
            p_unit           = p_unit->enter;
            need_full_redraw = 1;
        } else if (p_unit->enter == p_unit) {
            /* 叶子节点 → 执行功能 */
            if (p_unit->current_operation != NULL)
                p_unit->current_operation();
        } else {
            /* 子菜单 → 进入 */
            p_unit           = p_unit->enter;
            need_full_redraw = 1;
        }
    } else if (button3 == 1) {
        /* 下翻 */
        if (!p_unit->is_title)
            p_unit = p_unit->up;
    } else if (button4 == 1) {
        /* 上翻 */
        if (!p_unit->is_title)
            p_unit = p_unit->down;
    }

    /* 4. 显示 */
    show_current_page();

    /* 5. 参数调节处理 */
    if (p_unit != NULL &&
        (p_unit->type_t == NORMAL_PAR || p_unit->type_t == PID_PAR))
        change_value(p_unit->par_set);

    /* 6. 记录状态，复位 */
    p_unit_last = p_unit;
    button1 = 0;
    button2 = 0;
    button3 = 0;
    button4 = 0;
}

/*==================== 构建菜单树 ====================*/
/*
 *  主菜单: ======MAIN======
 *          > Debug       →  Debug子页 (motor, encoder, imu)
 *            PID         →  PID子页 (speed_pid, track_pid)
 *            Start       →  执行 start_car()
 *            Camera      →  Camera子页 (gary, binarize)
 */
static menu_unit *debug_page;
static menu_unit *pid_page;
static menu_unit *camera_page;

static void build_menu_tree(void)
{
    /* 创建页面 */
    main_page   = menu_create_page("======MAIN======");
    debug_page  = menu_create_page("--Debug--");
    pid_page    = menu_create_page("--PID--");
    camera_page = menu_create_page("--Camera--");

    /* Debug 子页 */
    menu_add_function(debug_page, "motor",   NULL_FUN);
    menu_add_function(debug_page, "encoder", NULL_FUN);
    menu_add_function(debug_page, "imu",     NULL_FUN);

    /* PID 子页 */
    menu_add_function(pid_page, "speed_pid",  NULL_FUN);
    menu_add_function(pid_page, "track_pid",  NULL_FUN);

    /* Camera 子页 */
    menu_add_function(camera_page, "gary",     NULL_FUN);
    menu_add_function(camera_page, "binarize", NULL_FUN);

    /* 主页项目 */
    menu_add_submenu(main_page, "Debug",  debug_page);
    menu_add_submenu(main_page, "PID",    pid_page);
    menu_add_function(main_page, "Start", start_car);
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

    /* 构建菜单树 */
    build_menu_tree();

    /* 起始位置：主页第一个项目 */
    p_unit      = main_page->enter;
    p_unit_last = NULL;

    /* 首屏绘制 */
    need_full_redraw = 1;
    show_current_page();
}

/*==================== 空函数 ====================*/
void NULL_FUN(void)
{
}

/*==================== Start 功能 ====================*/
void start_car(void)
{
    /* TODO: 发车逻辑 */
}
