#include "waveform_task.h"
#include "adc_task.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "arm_math.h"

// 常量定义
#define ADC_VREF 3.3f
#define ADC_TO_VOLT_SCALE (ADC_VREF / 4095.0f)

// 全局波形数据
waveform_data_t g_waveform_data = {0};
waveform_config_t g_waveform_config = {
    .v_scale = 1.0f,
    .t_scale = 0.001f,
    .trigger_mode = TRIGGER_AUTO,
    .trigger_level = 1.65f,
    .trigger_edge = TRIGGER_RISING,
    .channel_enable = 0x03
};


// 函数声明
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len);
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge);
static void measure_voltage_params(float *data, uint16_t len, float sample_interval, wave_params_t *params);

/**
 * @brief  ADC原始值转换为电压值
 */
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len) {
    if (!adc_buf || !volt_buf || len == 0) return;
    
    // 转换为浮点数
    for (uint16_t i = 0; i < len; i++) {
        volt_buf[i] = (float)adc_buf[i];
    }
    
    // 使用DSP库进行缩放
    arm_scale_f32(volt_buf, ADC_TO_VOLT_SCALE, volt_buf, len);
}

/**
 * @brief  测量电压参数
 */
static void measure_voltage_params(float *data, uint16_t len, float sample_interval, wave_params_t *params) {
    if (!data || !params || len == 0) return;
    
    // 1. 计算最大值和最小值
    float max_v, min_v;
    uint32_t max_index, min_index;
    arm_max_f32(data, len, &max_v, &max_index);
    arm_min_f32(data, len, &min_v, &min_index);
    
    params->max_voltage = max_v;
    params->min_voltage = min_v;
    params->amplitude = max_v - min_v;  // 峰峰值
    
    // 2. 计算平均值
    arm_mean_f32(data, len, &params->avg_voltage);
    
    // 3. 计算有效值（RMS）
    arm_rms_f32(data, len, &params->rms_voltage);
    
    // 4. 简单频率计算（过零检测）
    uint16_t zero_crossings = 0;
    float threshold = (max_v + min_v) * 0.5f;
    
    for (uint16_t i = 0; i < len - 1; i++) {
        if ((data[i] <= threshold && data[i + 1] > threshold) ||
            (data[i] >= threshold && data[i + 1] < threshold)) {
            zero_crossings++;
        }
    }
    
    if (zero_crossings > 2) {
        params->frequency = (zero_crossings / 2.0f) / (len * sample_interval);
        params->period = 1.0f / params->frequency;
    } else {
        params->frequency = 0.0f;
        params->period = 0.0f;
    }
    
    // 5. 占空比计算
    uint16_t high_count = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] > threshold) high_count++;
    }
    params->duty_cycle = (high_count * 100.0f) / len;
}

/**
 * @brief  查找触发点
 */
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge) {
    if (!data || len < 2) return -1;
    
    // 在中间区域查找，避免边界
    uint16_t start = len / 5;
    uint16_t end = len - len / 5;
    
    for (uint16_t i = start; i < end - 1; i++) {
        if (edge == TRIGGER_RISING) {
            if (data[i] <= level && data[i + 1] > level) {
                return i;
            }
        } else {
            if (data[i] >= level && data[i + 1] < level) {
                return i;
            }
        }
    }
    
    return -1;
}

/**
 * @brief  波形处理任务
 */
void waveform_process_task(void *argument) {
    static uint16_t temp_ch1[WAVEFORM_BUFFER_SIZE];
    static uint16_t temp_ch2[WAVEFORM_BUFFER_SIZE];
    static uint64_t last_count = 0;
    
    const float sample_interval = ADC_Get_Sample_Interval();
    
    while (1) {
        // 检查是否有新数据
        if (count_buffer > last_count) {
            last_count = count_buffer;
            
            // 复制ADC数据
            taskENTER_CRITICAL();
            memcpy(temp_ch1, adc_ch1, sizeof(temp_ch1));
            memcpy(temp_ch2, adc_ch2, sizeof(temp_ch2));
            taskEXIT_CRITICAL();
            
            // ADC转电压
            if (g_waveform_config.channel_enable & 0x01) {
                ADC_to_Voltage_DSP(temp_ch1, g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE);
            }
            if (g_waveform_config.channel_enable & 0x02) {
                ADC_to_Voltage_DSP(temp_ch2, g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE);
            }
            
            // 触发处理
            int trigger_pos = -1;
            if (g_waveform_config.trigger_mode != TRIGGER_AUTO) {
                trigger_pos = find_trigger_point(
                    g_waveform_data.ch1_data,
                    WAVEFORM_BUFFER_SIZE,
                    g_waveform_config.trigger_level,
                    g_waveform_config.trigger_edge
                );
                
                if (g_waveform_config.trigger_mode == TRIGGER_NORMAL && trigger_pos < 0) {
                    osDelay(1);
                    continue;
                }
            }
            
            g_waveform_data.trigger_position = (trigger_pos >= 0) ? trigger_pos : 0;
            
            // 电压参数测量
            if (g_waveform_config.channel_enable & 0x01) {
                measure_voltage_params(g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE, 
                                      sample_interval, &g_waveform_data.ch1_params);
            }
            
            if (g_waveform_config.channel_enable & 0x02) {
                measure_voltage_params(g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE,
                                      sample_interval, &g_waveform_data.ch2_params);
            }
            
            g_waveform_data.data_ready = 1;
            
            // 单次触发后停止
            if (g_waveform_config.trigger_mode == TRIGGER_SINGLE) {
                g_waveform_config.trigger_mode = TRIGGER_STOP;
            }
        }
        
        osDelay(10);
    }
}

// ==================== 配置函数 ====================

void waveform_set_vscale(uint8_t channel, float v_per_div) {
    if (v_per_div > 0.0f && v_per_div <= 10.0f) {
        g_waveform_config.v_scale = v_per_div;
    }
}

void waveform_set_tscale(float t_per_div) {
    if (t_per_div > 0.0f && t_per_div <= 1.0f) {
        g_waveform_config.t_scale = t_per_div;
    }
}

void waveform_set_trigger(uint8_t mode, float level, uint8_t edge) {
    if (mode <= TRIGGER_SINGLE) {
        g_waveform_config.trigger_mode = mode;
    }
    if (level >= 0.0f && level <= ADC_VREF) {
        g_waveform_config.trigger_level = level;
    }
    if (edge <= TRIGGER_FALLING) {
        g_waveform_config.trigger_edge = edge;
    }
}

void waveform_enable_channel(uint8_t channel, uint8_t enable) {
    if (channel == 1) {
        if (enable) {
            g_waveform_config.channel_enable |= 0x01;
        } else {
            g_waveform_config.channel_enable &= ~0x01;
        }
    } else if (channel == 2) {
        if (enable) {
            g_waveform_config.channel_enable |= 0x02;
        } else {
            g_waveform_config.channel_enable &= ~0x02;
        }
    }
}

waveform_data_t* waveform_get_data(void) {
    return &g_waveform_data;
}

waveform_config_t* waveform_get_config(void) {
    return &g_waveform_config;
}