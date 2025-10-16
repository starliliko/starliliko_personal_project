#ifndef __WAVEFORM_TASK_H
#define __WAVEFORM_TASK_H

#include "main.h"

// 波形缓冲区大小（与ADC一致）
#define WAVEFORM_BUFFER_SIZE 1024

// 触发模式定义
#define TRIGGER_AUTO 0
#define TRIGGER_NORMAL 1
#define TRIGGER_SINGLE 2
#define TRIGGER_STOP 0xFF

// ✅ 修复: 避免与HAL库冲突,使用不同的名称
#define WAVE_TRIGGER_RISING 0
#define WAVE_TRIGGER_FALLING 1

// 波形类型定义
typedef enum
{
    WAVE_TYPE_UNKNOWN = 0, // 未知波形
    WAVE_TYPE_SINE,        // 正弦波
    WAVE_TYPE_SQUARE,      // 方波
    WAVE_TYPE_TRIANGLE,    // 三角波
    WAVE_TYPE_SAWTOOTH,    // 锯齿波
    WAVE_TYPE_DC,          // 直流
    WAVE_TYPE_NOISE        // 噪声
} wave_type_t;

// 波形测量参数结构（仅保留电压相关）
typedef struct
{
    float frequency;       // 频率 (Hz)
    float period;          // 周期 (s)
    float amplitude;       // 峰峰值 (V)
    float max_voltage;     // 最大电压 (V)
    float min_voltage;     // 最小电压 (V)
    float avg_voltage;     // 平均电压 (V)
    float rms_voltage;     // 有效值电压 (V)
    float duty_cycle;      // 占空比 (%)
    wave_type_t wave_type; // 波形类型
} wave_params_t;

// 波形显示配置
typedef struct
{
    float v_scale;          // 电压刻度
    float t_scale;          // 时间刻度
    uint8_t trigger_mode;   // 触发模式
    float trigger_level;    // 触发电平
    uint8_t trigger_edge;   // 触发边沿
    uint8_t channel_enable; // 通道使能
} waveform_config_t;

// 波形数据结构
typedef struct
{
    float ch1_data[WAVEFORM_BUFFER_SIZE];
    float ch2_data[WAVEFORM_BUFFER_SIZE];
    wave_params_t ch1_params;
    wave_params_t ch2_params;
    uint8_t data_ready;
    uint16_t trigger_position;
} waveform_data_t;

// 外部访问的全局变量
extern waveform_data_t g_waveform_data;
extern waveform_config_t g_waveform_config;

// 任务函数
void waveform_process_task(void *argument);

// 配置函数
void waveform_set_vscale(uint8_t channel, float v_per_div);
void waveform_set_tscale(float t_per_div);
void waveform_set_trigger(uint8_t mode, float level, uint8_t edge);
void waveform_enable_channel(uint8_t channel, uint8_t enable);

// 数据访问函数
waveform_data_t *waveform_get_data(void);
waveform_config_t *waveform_get_config(void);

#endif /* __WAVEFORM_TASK_H */