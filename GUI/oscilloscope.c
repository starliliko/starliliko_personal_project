#include "lvgl/lvgl.h"
#include <stdlib.h>
#include "arm_math.h"
#include <stdio.h>
#include <string.h>  
#include "oscilloscope.h"  
#include "wave_processing.h"  


/* 全局变量 */
static lv_obj_t *scope_canvas;
static lv_obj_t *measurement_table;  // 保留表格组件

// 通道参数
static int16_t wave1_buffer[BUFFER_SIZE], wave2_buffer[BUFFER_SIZE];  // 绘制用缓冲区（像素偏移）
static float ch1_current_v, ch1_freq;  // 表格显示的测量值（电压、频率）
static float ch2_current_v, ch2_freq;

// 外部波形相关变量
static wave_display_mode_t ch1_mode = WAVE_MODE_TEST;  // 通道1显示模式
static wave_display_mode_t ch2_mode = WAVE_MODE_TEST;  // 通道2显示模式
static float ch1_external_v[BUFFER_SIZE] = {0};       // 通道1外部输入
static float ch2_external_v[BUFFER_SIZE] = {0};       // 通道2外部输入
static float ch1_test_v[BUFFER_SIZE] = {0};           // 通道1测试波形
static float ch2_test_v[BUFFER_SIZE] = {0};           // 通道2测试波形

// 测量参数存储
static float ch1_max_v, ch1_min_v, ch1_avg_v, ch1_rms_v;
static float ch2_max_v, ch2_min_v, ch2_avg_v, ch2_rms_v;
static float ch1_freq, ch2_freq;
static wave_type_t ch1_type, ch2_type;

/* 函数原型声明 */
static void generate_sine_wave(float target_freq, float amp, float sample_interval, float *v_buf, uint16_t buf_len);
static void generate_square_wave(float target_freq, float amp, float sample_interval, float *v_buf, uint16_t buf_len);
static void generate_waveforms(void);
static float calculate_frequency(int16_t *wave_buf, uint16_t buf_len);  // 保留频率计算（表格数据来源）
static void calculate_all_measurements(void);  // 保留测量计算（表格数据来源）
static void draw_scope_background(lv_event_t *e);
static void draw_waveforms(lv_event_t *e);
static void update_measurement_table(void);  // 保留表格更新函数
static void refresh_scope(lv_timer_t *timer);

/* 频率计算（表格显示频率数据来源） */
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
                break;  // 最多检测10个过零点
            }
        }
    }

    // 至少2个过零点计算周期
    if (zero_cross_cnt >= 2) {
        uint16_t cycle_samples = zero_cross_pos[1] - zero_cross_pos[0];
        float sample_interval = (TIME_DIV * 1e-3f * GRID_X_COUNT) / buf_len;
        float cycle = sample_interval * cycle_samples;
        freq = (cycle > 1e-6f) ? (1.0f / cycle) : 0.0f;
    }

    return freq;
}

/* 计算测量参数（表格数据来源） */
static void calculate_all_measurements(void) {
    // 计算采样间隔（与波形生成保持一致）
    const float total_display_time = TIME_DIV * 1e-3f * GRID_X_COUNT;
    const float sample_interval = total_display_time / BUFFER_SIZE;
    
    // 通道1测量
    const float *ch1_v_buf = (ch1_mode == WAVE_MODE_TEST) ? ch1_test_v : ch1_external_v;
    wave_params_t ch1_params = {0};
    
    // 使用wave_processing模块处理波形
    ch1_params.frequency = calculate_wave_frequency(ch1_v_buf, BUFFER_SIZE, sample_interval);
    measure_wave_voltage(ch1_v_buf, BUFFER_SIZE, 
                        &ch1_max_v, &ch1_min_v, &ch1_current_v, &ch1_rms_v);
    ch1_params.type = determine_wave_type(ch1_v_buf, BUFFER_SIZE, 
                                         ch1_params.frequency, sample_interval);
    ch1_params.amplitude = (ch1_max_v - ch1_min_v) / 2.0f;  // 计算幅度
    
    // 更新通道1显示参数
    ch1_freq = ch1_params.frequency;
    ch1_type = ch1_params.type;  // 存储波形类型用于表格显示

    // 通道2测量
    const float *ch2_v_buf = (ch2_mode == WAVE_MODE_TEST) ? ch2_test_v : ch2_external_v;
    wave_params_t ch2_params = {0};
    
    // 使用wave_processing模块处理波形
    ch2_params.frequency = calculate_wave_frequency(ch2_v_buf, BUFFER_SIZE, sample_interval);
    measure_wave_voltage(ch2_v_buf, BUFFER_SIZE, 
                        &ch2_max_v, &ch2_min_v, &ch2_current_v, &ch2_rms_v);
    ch2_params.type = determine_wave_type(ch2_v_buf, BUFFER_SIZE, 
                                         ch2_params.frequency, sample_interval);
    ch2_params.amplitude = (ch2_max_v - ch2_min_v) / 2.0f;  // 计算幅度
    
    // 更新通道2显示参数
    ch2_freq = ch2_params.frequency;
    ch2_type = ch2_params.type;  // 存储波形类型用于表格显示
}


/* 测试波形生成函数 */
static void generate_sine_wave(float target_freq, float amp, float sample_interval, float *v_buf, uint16_t buf_len) {
    static float phase = 0.0f;
    float phase_step = 2 * M_PI * target_freq * sample_interval;

    for (uint16_t i = 0; i < buf_len; i++) {
        v_buf[i] = amp * sin(phase);
        phase += phase_step;
        if (phase >= 2 * M_PI) phase -= 2 * M_PI;
    }
}

static void generate_square_wave(float target_freq, float amp, float sample_interval, float *v_buf, uint16_t buf_len) {
    static float phase = 0.0f;
    float phase_step = 2 * M_PI * target_freq * sample_interval;

    for (uint16_t i = 0; i < buf_len; i++) {
        v_buf[i] = (phase < M_PI) ? amp : -amp;
        phase += phase_step;
        if (phase >= 2 * M_PI) phase -= 2 * M_PI;
    }
}

/* 波形数据处理（转换为像素偏移） */
static void generate_waveforms(void) {
    const float total_display_time = TIME_DIV * 1e-3f * GRID_X_COUNT;
    const float sample_interval = total_display_time / BUFFER_SIZE;

    /* 通道1波形处理 */
    switch (ch1_mode) {
        case WAVE_MODE_TEST:
            generate_sine_wave(CH1_TEST_FREQ, CH1_TEST_AMP, sample_interval, ch1_test_v, BUFFER_SIZE);
            for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
                wave1_buffer[i] = (int16_t)(ch1_test_v[i] * PIXELS_PER_VOLT);
            }
            break;
        case WAVE_MODE_EXTERNAL:
            for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
                float clamped_v = fminf(fmaxf(ch1_external_v[i], -2.0f), 2.0f);
                wave1_buffer[i] = (int16_t)(clamped_v * PIXELS_PER_VOLT);
            }
            break;
    }

    /* 通道2波形处理 */
    switch (ch2_mode) {
        case WAVE_MODE_TEST:
            generate_square_wave(CH2_TEST_FREQ, CH2_TEST_AMP, sample_interval, ch2_test_v, BUFFER_SIZE);
            for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
                wave2_buffer[i] = (int16_t)(ch2_test_v[i] * PIXELS_PER_VOLT);
            }
            break;
        case WAVE_MODE_EXTERNAL:
            for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
                float clamped_v = fminf(fmaxf(ch2_external_v[i], -2.0f), 2.0f);
                wave2_buffer[i] = (int16_t)(clamped_v * PIXELS_PER_VOLT);
            }
            break;
    }
}

/* 绘制背景、网格、坐标轴 */
static void draw_scope_background(lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN) return;

    // 绘制背景
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_100;
    rect_dsc.bg_color = lv_color_hex(0x222222);
    lv_draw_rect(dsc->draw_ctx, &rect_dsc, dsc->draw_area);

    // 绘制网格线
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x333333);
    line_dsc.width = 1;

    // X轴竖线
    lv_point_t x_line[2] = {{0, dsc->draw_area->y1}, {0, dsc->draw_area->y2}};
    for (uint16_t i = 1; i < GRID_X_COUNT; i++) {
        x_line[0].x = x_line[1].x = dsc->draw_area->x1 + 
            (dsc->draw_area->x2 - dsc->draw_area->x1) * i / GRID_X_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);
    }

    // Y轴横线
    lv_point_t y_line[2] = {{dsc->draw_area->x1, 0}, {dsc->draw_area->x2, 0}};
    for (uint16_t i = 1; i < GRID_Y_COUNT; i++) {
        y_line[0].y = y_line[1].y = dsc->draw_area->y1 + 
            (dsc->draw_area->y2 - dsc->draw_area->y1) * i / GRID_Y_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);
    }

    // 绘制坐标轴（绿色）
    line_dsc.color = lv_color_hex(0x00FF00);
    line_dsc.width = 2;

    // X轴（0V参考线）
    y_line[0].y = y_line[1].y = dsc->draw_area->y1 + (dsc->draw_area->y2 - dsc->draw_area->y1) / 2;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);

    // Y轴（左侧参考线）
    x_line[0].x = x_line[1].x = dsc->draw_area->x1 + 30;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);

    // 绘制触发线（红色虚线）
    line_dsc.color = lv_color_hex(0xFF0000);
    line_dsc.width = 1;
    line_dsc.dash_width = 2;
    line_dsc.dash_gap = 2;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);

    // Y轴单位标识
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_hex(0xFFFFFF);
    label_dsc.font = LV_FONT_DEFAULT;

    lv_area_t y_unit_area = {
        .x1 = dsc->draw_area->x1 + 10,
        .y1 = dsc->draw_area->y1 + 10,
        .x2 = dsc->draw_area->x1 + 80,
        .y2 = dsc->draw_area->y1 + 30
    };
    lv_draw_label(dsc->draw_ctx, &label_dsc, &y_unit_area, "0.5V", NULL);
}

/* 绘制双通道波形 */
static void draw_waveforms(lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN) return;

    int start_x = dsc->draw_area->x1 + 30;
    int end_x = dsc->draw_area->x2;
    int center_y = dsc->draw_area->y1 + (dsc->draw_area->y2 - dsc->draw_area->y1) / 2;
    int wave_width = end_x - start_x;

    // 通道1波形（青色）
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x00FFFF);
    line_dsc.width = 2;

    for (uint16_t i = 1; i < BUFFER_SIZE; i++) {
        lv_point_t p1 = {
            .x = start_x + (wave_width * (i-1)) / (BUFFER_SIZE - 1),
            .y = center_y - wave1_buffer[i-1]
        };
        lv_point_t p2 = {
            .x = start_x + (wave_width * i) / (BUFFER_SIZE - 1),
            .y = center_y - wave1_buffer[i]
        };
        lv_draw_line(dsc->draw_ctx, &line_dsc, &p1, &p2);
    }

    // 通道2波形（黄色）
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

/* 更新测量表格（保留表格功能） */
static void update_measurement_table(void) {
    // 设置表格行列数
    lv_table_set_row_cnt(measurement_table, 4);
    lv_table_set_col_cnt(measurement_table, 3);

    // 表格样式设置
    lv_obj_set_style_bg_color(measurement_table, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(measurement_table, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_text_color(measurement_table, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_border_color(measurement_table, lv_color_hex(0x666666), LV_PART_ITEMS);
    lv_obj_set_style_border_width(measurement_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_height(measurement_table, 16, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(measurement_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_text_font(measurement_table, LV_FONT_DEFAULT, LV_PART_ITEMS);

    // 列宽设置
    lv_table_set_col_width(measurement_table, 0, 100);
    lv_table_set_col_width(measurement_table, 1, 120);
    lv_table_set_col_width(measurement_table, 2, 120);

    // 表格内容填充
    lv_table_set_cell_value(measurement_table, 0, 0, "Parameter");
    lv_table_set_cell_value(measurement_table, 0, 1, "CH1 (Cyan)");
    lv_table_set_cell_value(measurement_table, 0, 2, "CH2 (Yellow)");

    // 当前电压行
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
    const char *ch1_type_str = (ch1_mode == WAVE_MODE_TEST) ? CH1_TEST_TYPE : "External";
    const char *ch2_type_str = (ch2_mode == WAVE_MODE_TEST) ? CH2_TEST_TYPE : "External";
    lv_table_set_cell_value(measurement_table, 3, 0, "Wave Type");
    lv_table_set_cell_value(measurement_table, 3, 1, ch1_type_str);
    lv_table_set_cell_value(measurement_table, 3, 2, ch2_type_str);
}

/* 刷新定时器回调 */
static void refresh_scope(lv_timer_t *timer) {
    generate_waveforms();           // 更新波形数据
    calculate_all_measurements();   // 计算表格所需测量值
    lv_obj_invalidate(scope_canvas); // 重绘画布
    update_measurement_table();     // 更新表格显示
}

/* 创建示波器UI（包含波形区和表格） */
void create_oscilloscope_ui(void) {
    // 主屏幕
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    // 波形显示区
    scope_canvas = lv_obj_create(scr);
    lv_obj_set_size(scope_canvas, SCOPE_WIDTH, WAVE_HEIGHT);
    lv_obj_align(scope_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(scope_canvas, draw_scope_background, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(scope_canvas, draw_waveforms, LV_EVENT_DRAW_PART_END, NULL);

    // 测量表格（保留表格创建）
    measurement_table = lv_table_create(scr);
    lv_obj_set_size(measurement_table, SCOPE_WIDTH, TABLE_HEIGHT);
    lv_obj_align(measurement_table, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // 启动刷新定时器
    lv_timer_create(refresh_scope, 50, NULL);

    // 初始更新表格
    update_measurement_table();
    lv_obj_invalidate(measurement_table);
}

/* 外部控制接口 */
void set_ch1_display_mode(wave_display_mode_t mode) {
    ch1_mode = mode;
}

void set_ch2_display_mode(wave_display_mode_t mode) {
    ch2_mode = mode;
}

void set_ch1_external_wave(const float *v_buf, uint16_t len) {
    if (v_buf && len == BUFFER_SIZE) {
        memcpy(ch1_external_v, v_buf, len * sizeof(float));
    }
}

void set_ch2_external_wave(const float *v_buf, uint16_t len) {
    if (v_buf && len == BUFFER_SIZE) {
        memcpy(ch2_external_v, v_buf, len * sizeof(float));
    }
}