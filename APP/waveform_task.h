#ifndef __WAVEFORM_TASK_H
#define __WAVEFORM_TASK_H

#include "main.h"

// 波形缓冲区大小（与ADC一致）
#define WAVEFORM_BUFFER_SIZE 512

// 触发模式定义
#define TRIGGER_AUTO    0  // 自动触发
#define TRIGGER_NORMAL  1  // 正常触发
#define TRIGGER_SINGLE  2  // 单次触发
#define TRIGGER_STOP    0xFF  // 停止状态

// 触发边沿定义
#define TRIGGER_RISING  0  // 上升沿
#define TRIGGER_FALLING 1  // 下降沿

// 波形测量参数结构
typedef struct {
    float frequency;      // 频率 (Hz)
    float period;         // 周期 (s)
    float amplitude;      // 峰峰值 (V)
    float max_voltage;    // 最大电压 (V)
    float min_voltage;    // 最小电压 (V)
    float avg_voltage;    // 平均电压 (V)
    float rms_voltage;    // 有效值电压 (V)
    float duty_cycle;     // 占空比 (%)
} wave_params_t;

// 波形显示配置
typedef struct {
    float v_scale;          // 电压刻度 (V/div)
    float t_scale;          // 时间刻度 (s/div)
    uint8_t trigger_mode;   // 触发模式：0=自动，1=正常，2=单次
    float trigger_level;    // 触发电平 (V)
    uint8_t trigger_edge;   // 触发边沿：0=上升沿，1=下降沿
    uint8_t channel_enable; // 通道使能：bit0=CH1, bit1=CH2
} waveform_config_t;

// 波形数据结构
typedef struct {
    float ch1_data[WAVEFORM_BUFFER_SIZE];  // 通道1电压数据
    float ch2_data[WAVEFORM_BUFFER_SIZE];  // 通道2电压数据
    wave_params_t ch1_params;              // 通道1参数
    wave_params_t ch2_params;              // 通道2参数
    uint8_t data_ready;                    // 数据就绪标志
    uint16_t trigger_position;             // 触发点位置
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
waveform_data_t* waveform_get_data(void);
waveform_config_t* waveform_get_config(void);

#endif /* __WAVEFORM_TASK_H */