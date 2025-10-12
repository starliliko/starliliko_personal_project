#include "lvgl/lvgl.h"
#include <stdlib.h>
#include "arm_math.h"
#include <stdio.h>
#include <string.h>
#include "oscilloscope.h"
#include "waveform_task.h"
#include "adc_task.h"

/* 全局变量 */
static lv_obj_t *scope_canvas;
static lv_obj_t *measurement_table;

/* 通道参数 */
static int16_t wave1_buffer[DISPLAY_BUFFER_SIZE] = {0};
static int16_t wave2_buffer[DISPLAY_BUFFER_SIZE] = {0};
static float ch1_current_v, ch1_freq;
static float ch2_current_v, ch2_freq;

/* 函数原型声明 */
static void generate_waveforms(void);
static void draw_scope_background(lv_event_t *e);
static void draw_waveforms(lv_event_t *e);
static void update_measurement_table(void);
static void refresh_scope(lv_timer_t *timer);

/* 波形数据处理（转换为像素偏移） */
static void generate_waveforms(void)
{
    waveform_data_t *wave_data = waveform_get_data();

    if (!wave_data->data_ready)
        return;

    const int wave_height = WAVE_HEIGHT;
    const int grid_y_count = GRID_Y_COUNT;
    const float voltage_range = V_PER_DIV * (grid_y_count - 1);
    const float pixels_per_volt = (wave_height * (grid_y_count - 1) / grid_y_count) / voltage_range;

    // ✅ 使用TIME_DIV计算时间窗口内的有效采样点数
    float total_time_window = TIME_DIV * GRID_X_COUNT; // 总时间窗口
    float sample_rate = ADC_Get_Sample_Rate();         // 获取ADC采样率
    uint16_t effective_samples = (uint16_t)(total_time_window * sample_rate);

    // 限制有效采样点数不超过缓冲区大小
    if (effective_samples > DISPLAY_BUFFER_SIZE)
    {
        effective_samples = DISPLAY_BUFFER_SIZE;
    }

    for (uint16_t i = 0; i < effective_samples; i++)
    {
        wave1_buffer[i] = (int16_t)(wave_data->ch1_data[i] * pixels_per_volt);
        wave2_buffer[i] = (int16_t)(wave_data->ch2_data[i] * pixels_per_volt);
    }

    // 清空剩余缓冲区
    for (uint16_t i = effective_samples; i < DISPLAY_BUFFER_SIZE; i++)
    {
        wave1_buffer[i] = 0;
        wave2_buffer[i] = 0;
    }
}

/* 绘制背景、网格、坐标轴 */
static void draw_scope_background(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN)
        return;

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
    for (uint16_t i = 1; i < GRID_X_COUNT; i++)
    {
        x_line[0].x = x_line[1].x = dsc->draw_area->x1 +
                                    (dsc->draw_area->x2 - dsc->draw_area->x1) * i / GRID_X_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);
    }

    // Y轴横线
    lv_point_t y_line[2] = {{dsc->draw_area->x1, 0}, {dsc->draw_area->x2, 0}};
    for (uint16_t i = 1; i < GRID_Y_COUNT; i++)
    {
        y_line[0].y = y_line[1].y = dsc->draw_area->y1 +
                                    (dsc->draw_area->y2 - dsc->draw_area->y1) * i / GRID_Y_COUNT;
        lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);
    }

    // 绘制坐标轴（绿色）
    line_dsc.color = lv_color_hex(0x00FF00);
    line_dsc.width = 2;

    int y_axis_top = dsc->draw_area->y1;
    int y_axis_bottom = dsc->draw_area->y2;
    int axis_height = y_axis_bottom - y_axis_top;
    int x_axis_y = y_axis_top + (axis_height * (GRID_Y_COUNT - 1)) / GRID_Y_COUNT;

    // X轴（0V参考线）
    y_line[0].y = y_line[1].y = x_axis_y;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &y_line[0], &y_line[1]);

    // Y轴（左侧参考线）
    x_line[0].x = x_line[1].x = dsc->draw_area->x1 + 30;
    lv_draw_line(dsc->draw_ctx, &line_dsc, &x_line[0], &x_line[1]);

    // Y轴单位标识
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_hex(0xFFFFFF);
    label_dsc.font = LV_FONT_DEFAULT;

    lv_area_t y_unit_area = {
        .x1 = dsc->draw_area->x1 + 10,
        .y1 = dsc->draw_area->y1 + 10,
        .x2 = dsc->draw_area->x1 + 80,
        .y2 = dsc->draw_area->y1 + 30};
    lv_draw_label(dsc->draw_ctx, &label_dsc, &y_unit_area, "0.5V", NULL);
}

/* 绘制双通道波形 */
static void draw_waveforms(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_MAIN)
        return;

    int y_axis_top = dsc->draw_area->y1;
    int y_axis_bottom = dsc->draw_area->y2;
    int axis_height = y_axis_bottom - y_axis_top;
    int x_axis_y = y_axis_top + (axis_height * (GRID_Y_COUNT - 1)) / GRID_Y_COUNT;

    int start_x = dsc->draw_area->x1 + 30;
    int end_x = dsc->draw_area->x2;
    int wave_width = end_x - start_x;

    //  使用TIME_DIV计算实际显示的采样点数
    float total_time_window = TIME_DIV * GRID_X_COUNT;
    float sample_rate = ADC_Get_Sample_Rate();
    uint16_t effective_samples = (uint16_t)(total_time_window * sample_rate);

    if (effective_samples > DISPLAY_BUFFER_SIZE)
    {
        effective_samples = DISPLAY_BUFFER_SIZE;
    }
    if (effective_samples < 2)
    {
        effective_samples = 2; // 至少需要2个点才能绘制线段
    }

    // 通道1波形（青色）
    lv_draw_line_dsc_t ch1_line_dsc;
    lv_draw_line_dsc_init(&ch1_line_dsc);
    ch1_line_dsc.color = lv_color_hex(0x00FFFF);
    ch1_line_dsc.width = 2;

    for (uint16_t i = 1; i < effective_samples; i++)
    {
        lv_point_t p1 = {
            .x = start_x + (wave_width * (i - 1)) / (effective_samples - 1),
            .y = x_axis_y - wave1_buffer[i - 1]};
        lv_point_t p2 = {
            .x = start_x + (wave_width * i) / (effective_samples - 1),
            .y = x_axis_y - wave1_buffer[i]};
        lv_draw_line(dsc->draw_ctx, &ch1_line_dsc, &p1, &p2);
    }

    // 通道2波形（黄色）
    lv_draw_line_dsc_t ch2_line_dsc;
    lv_draw_line_dsc_init(&ch2_line_dsc);
    ch2_line_dsc.color = lv_color_hex(0xFFFF00);
    ch2_line_dsc.width = 2;

    for (uint16_t i = 1; i < effective_samples; i++)
    {
        lv_point_t p1 = {
            .x = start_x + (wave_width * (i - 1)) / (effective_samples - 1),
            .y = x_axis_y - wave2_buffer[i - 1]};
        lv_point_t p2 = {
            .x = start_x + (wave_width * i) / (effective_samples - 1),
            .y = x_axis_y - wave2_buffer[i]};
        lv_draw_line(dsc->draw_ctx, &ch2_line_dsc, &p1, &p2);
    }
}

/* 更新测量表格 */
static void update_measurement_table(void)
{
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

    lv_table_set_col_width(measurement_table, 0, 100);
    lv_table_set_col_width(measurement_table, 1, 120);
    lv_table_set_col_width(measurement_table, 2, 120);

    // 表格内容
    lv_table_set_cell_value(measurement_table, 0, 0, "Parameter");
    lv_table_set_cell_value(measurement_table, 0, 1, "CH1 (Cyan)");
    lv_table_set_cell_value(measurement_table, 0, 2, "CH2 (Yellow)");

    waveform_data_t *wave_data = waveform_get_data();

    ch1_current_v = wave_data->ch1_params.avg_voltage;
    ch1_freq = wave_data->ch1_params.frequency;
    ch2_current_v = wave_data->ch2_params.avg_voltage;
    ch2_freq = wave_data->ch2_params.frequency;

    char ch1_cur[15], ch2_cur[15];
    snprintf(ch1_cur, sizeof(ch1_cur), "%.2fV", ch1_current_v);
    snprintf(ch2_cur, sizeof(ch2_cur), "%.2fV", ch2_current_v);
    lv_table_set_cell_value(measurement_table, 1, 0, "Value");
    lv_table_set_cell_value(measurement_table, 1, 1, ch1_cur);
    lv_table_set_cell_value(measurement_table, 1, 2, ch2_cur);

    char ch1_freq_str[15], ch2_freq_str[15];
    snprintf(ch1_freq_str, sizeof(ch1_freq_str), "%.1fkHz", ch1_freq / 1000.0f);
    snprintf(ch2_freq_str, sizeof(ch2_freq_str), "%.1fkHz", ch2_freq / 1000.0f);
    lv_table_set_cell_value(measurement_table, 2, 0, "Frequency");
    lv_table_set_cell_value(measurement_table, 2, 1, ch1_freq_str);
    lv_table_set_cell_value(measurement_table, 2, 2, ch2_freq_str);

    lv_table_set_cell_value(measurement_table, 3, 2, "External");
}

/* 刷新定时器回调 */
static void refresh_scope(lv_timer_t *timer)
{
    generate_waveforms();
    lv_obj_invalidate(scope_canvas);
    update_measurement_table();
}

/* 创建示波器UI */
void create_oscilloscope_ui(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    scope_canvas = lv_obj_create(scr);
    lv_obj_set_size(scope_canvas, SCOPE_WIDTH, WAVE_HEIGHT);
    lv_obj_align(scope_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(scope_canvas, draw_scope_background, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(scope_canvas, draw_waveforms, LV_EVENT_DRAW_PART_END, NULL);

    measurement_table = lv_table_create(scr);
    lv_obj_set_size(measurement_table, SCOPE_WIDTH, TABLE_HEIGHT);
    lv_obj_align(measurement_table, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_timer_create(refresh_scope, 50, NULL);

    update_measurement_table();
    lv_obj_invalidate(measurement_table);
}
