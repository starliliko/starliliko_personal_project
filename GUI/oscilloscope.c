#include "lvgl/lvgl.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#define M_PI 3.14159265358979323846
/* 核心配置：适配320x240小屏幕 */
#define SCOPE_WIDTH     320     // 屏幕宽度
#define SCOPE_HEIGHT    240     // 屏幕高度
#define TABLE_HEIGHT    75      // 表格总高度（4行×16px + 边框）
#define WAVE_HEIGHT     (SCOPE_HEIGHT - TABLE_HEIGHT)  // 波形区高度

/* 其他参数 */
#define GRID_X_COUNT    10      
#define GRID_Y_COUNT    8       
#define BUFFER_SIZE     1024     
#define TIME_DIV        0.01     // 时基：0.01ms/格（总显示时间=0.1ms×8=0.8ms）
#define TRIGGER_LEVEL   0        // 触发电平：0V

//显示频率10-30K
/* 电压参数 - 明确每格0.5V */
#define V_PER_DIV       0.5f    // 每格电压：0.5V（核心参数，用于Y轴标识）
#define V_RANGE         4.0f    // 总电压范围：4V（-2V ~ +2V）
#define PIXELS_PER_VOLT (WAVE_HEIGHT / V_RANGE)  // 像素/伏换算

/* 全局变量 */
static lv_obj_t *scope_canvas;
static lv_obj_t *measurement_table;

// 通道参数（仅保留必要变量，新增【目标频率】变量用于调频率）
static int16_t wave1_buffer[BUFFER_SIZE], wave2_buffer[BUFFER_SIZE];
static float ch1_current_v, ch1_freq;  // 测量值（当前电压）、实际频率
static float ch2_current_v, ch2_freq;
static const char *ch1_wave_type = "Sine";   // 波形类型
static const char *ch2_wave_type = "Square";
// 新增：频率调节变量（外部可修改此变量来调整波形频率，范围建议1Hz~160kHz）
static float ch1_target_freq = 150*1000.0f;  // 通道1默认目标频率：1kHz
static float ch2_target_freq = 150*1000.0f;   // 通道2默认目标频率：800Hz

/* 频率计算：基于上升沿过零点检测 */
static float calculate_frequency(int16_t *wave_buf, uint16_t buf_len) {
    float freq = 0.0f;
    uint16_t zero_cross_cnt = 0;
    uint16_t zero_cross_pos[10] = {0};
    const int16_t trigger_level = 0;  // 0V过零点

    // 检测上升沿过零点（前一采样点<0，当前采样点>=0）
    for (uint16_t i = 1; i < buf_len; i++) {
        if (wave_buf[i-1] < trigger_level && wave_buf[i] >= trigger_level) {
            zero_cross_pos[zero_cross_cnt++] = i;
            if (zero_cross_cnt >= sizeof(zero_cross_pos)/sizeof(zero_cross_pos[0])) {
                break;  // 最多检测10个过零点，避免数组溢出
            }
        }
    }

    // 至少2个过零点才能计算周期
    if (zero_cross_cnt >= 2) {
        uint16_t cycle_samples = zero_cross_pos[1] - zero_cross_pos[0];  // 周期对应的采样数
        float sample_interval = (TIME_DIV * 1e-3f * GRID_X_COUNT) / buf_len;  // 采样间隔（秒）
        float cycle = sample_interval * cycle_samples;  // 周期（秒）
        freq = (cycle > 1e-6f) ? (1.0f / cycle) : 0.0f;  // 频率=1/周期，避免除以0
    }

    return freq;
}

/* 计算测量参数：仅保留当前电压和频率 */
static void calculate_all_measurements(void) {
    // 通道1：当前电压（最后一个采样点）+ 频率
    ch1_current_v = (float)wave1_buffer[BUFFER_SIZE-1] / PIXELS_PER_VOLT;
    ch1_freq = calculate_frequency(wave1_buffer, BUFFER_SIZE);

    // 通道2：当前电压 + 频率
    ch2_current_v = (float)wave2_buffer[BUFFER_SIZE-1] / PIXELS_PER_VOLT;
    ch2_freq = calculate_frequency(wave2_buffer, BUFFER_SIZE);
}

/* 生成测试波形：【频率可调版】通道1正弦波，通道2方波 */
static void generate_test_waves(void) {
    // 1. 计算核心参数：总显示时间、采样间隔（固定，由TIME_DIV和BUFFER_SIZE决定）
    const float total_display_time = TIME_DIV * 1e-3f * GRID_X_COUNT;  // 总显示时间：0.8ms = 0.0008s
    const float sample_interval = total_display_time / BUFFER_SIZE;    // 单个采样点间隔：0.0008s/256 ≈ 3.125μs

    // 2. 计算相位步进（根据目标频率动态调整，核心是“每个采样点的相位增量”）
    // 相位步进 = 2π * 目标频率 * 采样间隔（确保一个周期内的采样点数量正确）
    const float ch1_phase_step = 2 * M_PI * ch1_target_freq * sample_interval;
    const float ch2_phase_step = 2 * M_PI * ch2_target_freq * sample_interval;

    // 3. 静态相位变量（确保波形连续移动，不重置）
    static float ch1_phase = 0.0f;
    static float ch2_phase = 0.0f;

    // 4. 通道1：可调频率正弦波（幅度1.5V，避免超出-2V~+2V范围）
    const float ch1_amp = 1.5f;  // 正弦波幅度
    for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
        float ch1_v = ch1_amp * sin(ch1_phase);  // 基于动态相位生成正弦波
        wave1_buffer[i] = (int16_t)(ch1_v * PIXELS_PER_VOLT);  // 转换为像素偏移
        // 相位累加（超过2π后重置，避免数值溢出）
        ch1_phase += ch1_phase_step;
        if (ch1_phase >= 2 * M_PI) {
            ch1_phase -= 2 * M_PI;
        }
    }


    // 5. 通道2：可调频率方波（幅度1.2V，占空比50%）
    const float ch2_amp = 1.2f;  // 方波幅度
    for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
        // 相位在0~π时为高电平，π~2π时为低电平（占空比50%）
        float ch2_v = (ch2_phase < M_PI) ? ch2_amp : -ch2_amp;
        wave2_buffer[i] = (int16_t)(ch2_v * PIXELS_PER_VOLT);  // 转换为像素偏移
        // 相位累加（超过2π后重置）
        ch2_phase += ch2_phase_step;
        if (ch2_phase >= 2 * M_PI) {
            ch2_phase -= 2 * M_PI;
        }
    }
}

/* 绘制背景、网格、坐标轴 + Y轴0.5V/div标识 */
static void draw_scope_background(lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN) return;

    // 1. 绘制示波器背景（深灰色）
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_100;
    rect_dsc.bg_color = lv_color_hex(0x222222);
    lv_draw_rect(dsc->draw_ctx, &rect_dsc, dsc->draw_area);

    // 2. 绘制网格线（浅灰色，宽度1px）
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x333333);
    line_dsc.width = 1;

    // X轴竖线（10格，9条线）
    lv_point_t x_line[2] = {{0, dsc->draw_area->y1}, {0, dsc->draw_area->y2}};
    for (uint16_t i = 1; i < GRID_X_COUNT; i++) {
        x_line[0].x = x_line[1].x = dsc->draw_area->x1 + 
            (dsc->draw_area->x2 - dsc->draw_area->x1) * i / GRID_X_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);
    }

    // Y轴横线（8格，7条线）
    lv_point_t y_line[2] = {{dsc->draw_area->x1, 0}, {dsc->draw_area->x2, 0}};
    for (uint16_t i = 1; i < GRID_Y_COUNT; i++) {
        y_line[0].y = y_line[1].y = dsc->draw_area->y1 + 
            (dsc->draw_area->y2 - dsc->draw_area->y1) * i / GRID_Y_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);
    }

    // 3. 绘制坐标轴（绿色，宽度2px）
    line_dsc.color = lv_color_hex(0x00FF00);
    line_dsc.width = 2;

    // X轴（0V参考线，水平中线）
    y_line[0].y = y_line[1].y = dsc->draw_area->y1 + 
        (dsc->draw_area->y2 - dsc->draw_area->y1) / 2;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);

    // Y轴（左侧垂直中线，预留单位标识空间）
    x_line[0].x = x_line[1].x = dsc->draw_area->x1 + 30;  // 与波形绘制起始位置对齐
    lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);

    // 4. 新增：Y轴0.5V/div单位标识（左上角，白色文字）
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_hex(0xFFFFFF);  // 白色文字
    label_dsc.font = LV_FONT_DEFAULT;          // 默认字体
    label_dsc.align = LV_TEXT_ALIGN_LEFT;      // 左对齐

    // 标识位置：左上角（波形区左上角，x偏移10px，y偏移10px）
    lv_area_t y_unit_area = {
        .x1 = dsc->draw_area->x1 + 10,
        .y1 = dsc->draw_area->y1 + 10,
        .x2 = dsc->draw_area->x1 + 80,  // 足够显示"Y: 0.5V/div"
        .y2 = dsc->draw_area->y1 + 30
    };
    lv_draw_label(dsc->draw_ctx, &label_dsc, &y_unit_area, "0.5V", NULL);

    // 5. 绘制触发线（红色虚线，与X轴重合）
    line_dsc.color = lv_color_hex(0xFF0000);
    line_dsc.width = 1;
    line_dsc.dash_width = 2;  // 虚线宽度
    line_dsc.dash_gap = 2;    // 虚线间隙
    lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);
}

/* 绘制双通道波形：通道1青色，通道2黄色 */
static void draw_waveforms(lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN) return;

    // 波形绘制区域：避开Y轴和单位标识（x从30px开始）
    int start_x = dsc->draw_area->x1 + 30;
    int end_x = dsc->draw_area->x2;
    int center_y = dsc->draw_area->y1 + (dsc->draw_area->y2 - dsc->draw_area->y1) / 2;  // 0V线
    int wave_width = end_x - start_x;  // 波形水平绘制宽度

    // 1. 通道1波形（青色，线宽2px）
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x00FFFF);
    line_dsc.width = 2;

    for (uint16_t i = 1; i < BUFFER_SIZE; i++) {
        lv_point_t p1 = {
            .x = start_x + (wave_width * (i-1)) / (BUFFER_SIZE - 1),
            .y = center_y - wave1_buffer[i-1]  // 缓冲区值为像素偏移（向上为正）
        };
        lv_point_t p2 = {
            .x = start_x + (wave_width * i) / (BUFFER_SIZE - 1),
            .y = center_y - wave1_buffer[i]
        };
        lv_draw_line(dsc->draw_ctx, &line_dsc, &p1, &p2);
    }

    // 2. 通道2波形（黄色，线宽2px）
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0xFFFF00);
    line_dsc.width = 2;

    for (uint16_t i = 1; i < BUFFER_SIZE; i++) {
        lv_point_t p1 = {
            .x = start_x + (wave_width * (i-1)) / (BUFFER_SIZE - 1),
            .y = center_y - wave2_buffer[i-1]
        };
        lv_point_t p2 = {
            .x = start_x + (wave_width * i) / (BUFFER_SIZE - 1),
            .y = center_y - wave2_buffer[i]
        };
        lv_draw_line(dsc->draw_ctx, &line_dsc, &p1, &p2);
    }
}

/* 更新表格：仅保留测量值、频率、波形类型 */
static void update_measurement_table(void) {
    // 1. 设置表格行列数（4行：标题行+3个参数行；3列：参数名+CH1+CH2）
    lv_table_set_row_cnt(measurement_table, 4);
    lv_table_set_col_cnt(measurement_table, 3);

    // 2. 表格样式设置（高对比度，适配小屏幕）
    // 表格背景（黑色）+ 单元格背景（深灰色）
    lv_obj_set_style_bg_color(measurement_table, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(measurement_table, lv_color_hex(0x222222), LV_PART_ITEMS);
    // 文字（白色）+ 边框（灰色）
    lv_obj_set_style_text_color(measurement_table, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_border_color(measurement_table, lv_color_hex(0x666666), LV_PART_ITEMS);
    lv_obj_set_style_border_width(measurement_table, 1, LV_PART_ITEMS);
    // 行高（16px）+ 内边距（1px），确保文字不溢出
    lv_obj_set_style_height(measurement_table, 16, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(measurement_table, 1, LV_PART_ITEMS);
    // 字体（默认字体）
    lv_obj_set_style_text_font(measurement_table, LV_FONT_DEFAULT, LV_PART_ITEMS);

    // 3. 列宽适配（总宽度=320px，无溢出）
    lv_table_set_col_width(measurement_table, 0, 100);   // 参数名列（足够显示"Parameter"）
    lv_table_set_col_width(measurement_table, 1, 120);  // CH1列（足够显示"1.23V"）
    lv_table_set_col_width(measurement_table, 2, 120);  // CH2列

    // 4. 表格内容填充
    // 标题行
    lv_table_set_cell_value(measurement_table, 0, 0, "Parameter");
    lv_table_set_cell_value(measurement_table, 0, 1, "CH1 (Cyan)");
    lv_table_set_cell_value(measurement_table, 0, 2, "CH2 (Yellow)");

    // 测量值（当前电压）行
    char ch1_cur[15], ch2_cur[15];
    snprintf(ch1_cur, sizeof(ch1_cur), "%.2fV", ch1_current_v);
    snprintf(ch2_cur, sizeof(ch2_cur), "%.2fV", ch2_current_v);
    lv_table_set_cell_value(measurement_table, 1, 0, "Value");
    lv_table_set_cell_value(measurement_table, 1, 1, ch1_cur);
    lv_table_set_cell_value(measurement_table, 1, 2, ch2_cur);

    // 频率行
    char ch1_freq_str[15], ch2_freq_str[15];
    snprintf(ch1_freq_str, sizeof(ch1_freq_str), "%.1fkHz", ch1_freq/1000.0f);
    snprintf(ch2_freq_str, sizeof(ch2_freq_str), "%.1fkHz", ch2_freq/1000.0f);
    lv_table_set_cell_value(measurement_table, 2, 0, "Frequency");
    lv_table_set_cell_value(measurement_table, 2, 1, ch1_freq_str);
    lv_table_set_cell_value(measurement_table, 2, 2, ch2_freq_str);

    // 波形类型行
    lv_table_set_cell_value(measurement_table, 3, 0, "Wave Type");
    lv_table_set_cell_value(measurement_table, 3, 1, ch1_wave_type);
    lv_table_set_cell_value(measurement_table, 3, 2, ch2_wave_type);
}

/* 刷新定时器回调：20Hz刷新（50ms周期） */
static void refresh_scope(lv_timer_t *timer) {
    generate_test_waves();          // 生成新波形数据
    calculate_all_measurements();   // 计算测量参数
    lv_obj_invalidate(scope_canvas); // 重绘画布（更新波形）
    update_measurement_table();     // 更新表格（显示新参数）
}

/* 创建示波器UI：初始化画布、表格、定时器 */
void create_oscilloscope_ui(void) {
    // 1. 主屏幕（黑色背景，避免与示波器背景冲突）
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    // 2. 波形显示区（顶部，占满剩余高度）
    scope_canvas = lv_obj_create(scr);
    lv_obj_set_size(scope_canvas, SCOPE_WIDTH, WAVE_HEIGHT);
    lv_obj_align(scope_canvas, LV_ALIGN_TOP_LEFT, 0, 0);  // 顶左对齐，无偏移
    // 绑定绘图事件：先画背景（含Y轴标识），后画波形
    lv_obj_add_event_cb(scope_canvas, draw_scope_background, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(scope_canvas, draw_waveforms, LV_EVENT_DRAW_PART_END, NULL);

    // 3. 测量表格（底部，固定高度）
    measurement_table = lv_table_create(scr);
    lv_obj_set_size(measurement_table, SCOPE_WIDTH, TABLE_HEIGHT);
    lv_obj_align(measurement_table, LV_ALIGN_BOTTOM_LEFT, 0, 0);  // 底左对齐，无偏移

    // 4. 启动刷新定时器（50ms周期，20Hz刷新，确保波形流畅）
    lv_timer_create(refresh_scope, 50, NULL);

    // 5. 初始更新表格（避免首次显示空白）
    update_measurement_table();
    lv_obj_invalidate(measurement_table);
}

/* 新增：外部频率调节接口（可在其他函数中调用此接口修改频率） */
// 调节通道1正弦波频率（范围建议1Hz~160kHz）
void set_ch1_freq(float freq) {
    if (freq > 0 && freq <= 160000) {  // 限制最高频率为160kHz（硬件上限）
        ch1_target_freq = freq;
    }
}

// 调节通道2方波频率（范围建议1Hz~160kHz）
void set_ch2_freq(float freq) {
    if (freq > 0 && freq <= 160000) {  // 限制最高频率为160kHz
        ch2_target_freq = freq;
    }
}