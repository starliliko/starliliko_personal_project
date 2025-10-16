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

/* ✅ 修改: 扩大缓冲区支持高速采样 */
#define DISPLAY_BUFFER_SIZE 2048 // 保持不变

/* 时基配置 - 控制时间窗口 */
// ✅ 关键修改: 时基调整为5μs/格
#define TIME_DIV 0.000005f // 5μs/格 → 总窗口50μs → 显示5个100kHz周期

/* 电压参数 */
#define V_PER_DIV 0.5f                          // 每格电压：0.5V
#define V_RANGE 4.0f                            // 总电压范围：4V
#define PIXELS_PER_VOLT (WAVE_HEIGHT / V_RANGE) // 像素/伏换算

/* 函数声明 */
void create_oscilloscope_ui(void);

#endif // OSCILLOSCOPE_H
