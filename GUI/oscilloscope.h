#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

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
#define TIME_DIV        0.01     // 时基：0.01ms/格
   
//显示频率10-30K
/* 电压参数 - 明确每格0.5V */
#define V_PER_DIV       0.5f    // 每格电压：0.5V（核心参数，用于Y轴标识）
#define V_RANGE         4.0f    // 总电压范围：4V（-2V ~ +2V）
#define PIXELS_PER_VOLT (WAVE_HEIGHT / V_RANGE)  // 像素/伏换算

//测试波形参数
#define CH1_TEST_FREQ   100*1000.0f // 通道1测试波形频率：
#define CH2_TEST_FREQ   100*1000.0f // 通道2测试波形频率：
#define CH1_TEST_TYPE   "Sine"  // 通道1测试波形类型
#define CH2_TEST_TYPE   "Square"// 通道2测试波形类型
#define CH1_TEST_AMP    1.5f    // 通道1测试波形幅度
#define CH2_TEST_AMP    1.2f    // 通道2测试波形幅度

//波形显示模式枚举
typedef enum {
    WAVE_MODE_TEST,    // 测试波形模式（正弦/方波）
    WAVE_MODE_EXTERNAL // 外部输入波形模式
} wave_display_mode_t;

void create_oscilloscope_ui(void);
void set_ch1_display_mode(wave_display_mode_t mode);
void set_ch2_display_mode(wave_display_mode_t mode);
void set_ch1_external_wave(const float *v_buf, uint16_t len);
void set_ch2_external_wave(const float *v_buf, uint16_t len);


#endif // OSCILLOSCOPE_H