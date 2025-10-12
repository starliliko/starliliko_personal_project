#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

/* 核心配置：适配320x240小屏幕 */
#define SCOPE_WIDTH 320                           // 屏幕宽度
#define SCOPE_HEIGHT 240                          // 屏幕高度
#define TABLE_HEIGHT 75                           // 表格总高度（4行×16px + 边框）
#define WAVE_HEIGHT (SCOPE_HEIGHT - TABLE_HEIGHT) // 波形区高度

/* 网格配置 */
#define GRID_X_COUNT 10 // 水平10格
#define GRID_Y_COUNT 8  // 垂直8格

/* ✅ 修改: FFT优化的缓冲区配置 */
#define DISPLAY_BUFFER_SIZE 1024 // 与CHANNEL_BUFFER_SIZE保持一致

/* 时基配置 - 控制时间窗口 */
#define TIME_DIV 0.0001f // ✅ 修复: 0.2ms/格 = 2ms总窗口 (适合10-30kHz)
                         // 通过 waveform_set_tscale() 函数设置到波形任务中生效

/* 电压参数 */
#define V_PER_DIV 0.5f                          // 每格电压：0.5V
#define V_RANGE 4.0f                            // 总电压范围：4V
#define PIXELS_PER_VOLT (WAVE_HEIGHT / V_RANGE) // 像素/伏换算

/* 函数声明 */
void create_oscilloscope_ui(void);

#endif // OSCILLOSCOPE_H
