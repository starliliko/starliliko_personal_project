#include "waveform_task.h"
#include "adc_task.h"
#include "cmsis_os.h"
#include <string.h>

// 引入 ARM DSP 库
#include "arm_math.h"

// ADC参考电压和分辨率
#define ADC_VREF        3.3f
#define ADC_RESOLUTION  4096.0f

// 默认配置
#define DEFAULT_V_SCALE 0.5   // 1V/div
#define DEFAULT_T_SCALE 0.001f  // 1ms/div

// 预计算常量（编译时优化）
#define ADC_TO_VOLT_SCALE (ADC_VREF / ADC_RESOLUTION)

// ADC采集数据
extern uint16_t adc_ch1[WAVEFORM_BUFFER_SIZE];
extern uint16_t adc_ch2[WAVEFORM_BUFFER_SIZE];
extern volatile uint64_t count_buffer;

// 全局波形数据（32字节对齐优化FPU访问）
waveform_data_t g_waveform_data __attribute__((aligned(32))) = {0};

waveform_config_t g_waveform_config = {
    .v_scale = DEFAULT_V_SCALE,
    .t_scale = DEFAULT_T_SCALE,
    .trigger_mode = TRIGGER_AUTO,
    .trigger_level = 1.65f,
    .trigger_edge = TRIGGER_RISING,
    .channel_enable = 0x03
};

// 内部函数声明
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len);
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge);
static void measure_waveform_DSP(float *data, uint16_t len, float sample_interval, wave_params_t *params);
static float calculate_frequency_DSP(float *data, uint16_t len, float sample_interval);

/**
 * @brief  ADC原始值转换为电压值（DSP优化版本）
 * @note   使用 CMSIS-DSP 库的向量运算加速
 */
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len) {
    if (!adc_buf || !volt_buf || len == 0) return;
    
    // 方法1: 使用 ARM DSP 库的类型转换和缩放
    // 步骤1: uint16 → float32
    for (uint16_t i = 0; i < len; i++) {
        volt_buf[i] = (float)adc_buf[i];
    }
    
    // 步骤2: 使用 DSP 库缩放 (volt_buf = volt_buf * scale)
    arm_scale_f32(volt_buf, ADC_TO_VOLT_SCALE, volt_buf, len);
}

/**
 * @brief  查找触发点位置（优化版本）
 */
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge) {
    if (!data || len < 2) return -1;
    
    uint16_t start = len / 5;
    
    // 使用指针优化访问
    float *ptr = data + start;
    float prev, curr;
    
    for (uint16_t i = start; i < len - 1; i++, ptr++) {
        prev = *ptr;
        curr = *(ptr + 1);
        
        if (edge == TRIGGER_RISING) {
            if (prev < level && curr >= level) {
                return i;
            }
        } else {
            if (prev > level && curr <= level) {
                return i;
            }
        }
    }
    
    return -1;
}

/**
 * @brief  计算波形频率（DSP优化版本 - FFT法）
 * @note   使用 FFT 提高频率测量精度
 */
static float calculate_frequency_DSP(float *data, uint16_t len, float sample_interval) {
    if (!data || len < 10 || sample_interval <= 0.0f) return 0.0f;
    
    // 方法1: 过零检测法（快速但精度一般）
    // 计算平均值作为阈值（使用DSP库）
    float mean_value;
    arm_mean_f32(data, len, &mean_value);
    
    // 统计过零次数
    uint16_t zero_crossings = 0;
    for (uint16_t i = 1; i < len; i++) {
        if ((data[i-1] < mean_value && data[i] >= mean_value) ||
            (data[i-1] > mean_value && data[i] <= mean_value)) {
            zero_crossings++;
        }
    }
    
    if (zero_crossings < 2) return 0.0f;
    
    float total_time = sample_interval * len;
    return (zero_crossings / 2.0f) / total_time;
    
    /* 方法2: FFT法（精度高但计算量大，可选）
    // 需要 512 或 1024 点 FFT
    #define FFT_SIZE 512
    static float fft_input[FFT_SIZE * 2];  // 实部+虚部
    static float fft_output[FFT_SIZE];
    
    arm_rfft_fast_instance_f32 fft_instance;
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
    
    // 复制数据到FFT输入缓冲区
    memcpy(fft_input, data, FFT_SIZE * sizeof(float));
    
    // 执行FFT
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);
    
    // 计算幅度谱
    arm_cmplx_mag_f32(fft_output, fft_input, FFT_SIZE / 2);
    
    // 找到最大幅度对应的频率
    uint32_t max_index;
    float max_value;
    arm_max_f32(fft_input + 1, FFT_SIZE / 2 - 1, &max_value, &max_index);
    
    float freq_resolution = 1.0f / (sample_interval * FFT_SIZE);
    return (max_index + 1) * freq_resolution;
    */
}

/**
 * @brief  测量波形所有参数（DSP优化版本）
 */
static void measure_waveform_DSP(float *data, uint16_t len, float sample_interval, wave_params_t *params) {
    if (!data || !params || len == 0) return;
    
    // 1. 使用 DSP 库计算最大值和最小值
    float max_v, min_v;
    uint32_t max_index, min_index;
    arm_max_f32(data, len, &max_v, &max_index);
    arm_min_f32(data, len, &min_v, &min_index);
    
    params->max_voltage = max_v;
    params->min_voltage = min_v;
    params->amplitude = max_v - min_v;
    
    // 2. 使用 DSP 库计算平均值
    arm_mean_f32(data, len, &params->avg_voltage);
    
    // 3. 使用 DSP 库计算 RMS（有效值）
    arm_rms_f32(data, len, &params->rms_voltage);
    
    // 4. 计算频率
    params->frequency = calculate_frequency_DSP(data, len, sample_interval);
    
    // 5. 计算周期
    params->period = (params->frequency > 0.0f) ? (1.0f / params->frequency) : 0.0f;
    
    // 6. 计算占空比（使用阈值统计）
    float threshold = (max_v + min_v) * 0.5f;  // 使用FPU优化的乘法
    uint16_t high_count = 0;
    
    // 向量化统计（循环展开）
    uint16_t i;
    for (i = 0; i < len / 4; i += 4) {
        if (data[i] > threshold) high_count++;
        if (data[i+1] > threshold) high_count++;
        if (data[i+2] > threshold) high_count++;
        if (data[i+3] > threshold) high_count++;
    }
    
    // 处理剩余数据
    for (; i < len; i++) {
        if (data[i] > threshold) high_count++;
    }
    
    params->duty_cycle = (high_count * 100.0f) / len;
}

/**
 * @brief  波形处理任务（DSP优化版本）
 */
void waveform_process_task(void *argument) {
    // 使用静态变量避免栈溢出，对齐优化内存访问
    static uint16_t temp_ch1[WAVEFORM_BUFFER_SIZE] __attribute__((aligned(32)));
    static uint16_t temp_ch2[WAVEFORM_BUFFER_SIZE] __attribute__((aligned(32)));
    static uint64_t last_count = 0;
    
    const float sample_interval = 1e-6f;  // 1μs
    
    while (1) {
        if (count_buffer > last_count) {
            last_count = count_buffer;
            
            // 复制ADC数据（临界区保护）
            taskENTER_CRITICAL();
            // 使用 memcpy 优化大块内存复制
            memcpy(temp_ch1, (uint16_t*)adc_ch1, sizeof(temp_ch1));
            memcpy(temp_ch2, (uint16_t*)adc_ch2, sizeof(temp_ch2));
            taskEXIT_CRITICAL();
            
            // 转换为电压值（使用DSP加速）
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
            
            // 测量波形参数（使用DSP加速）
            if (g_waveform_config.channel_enable & 0x01) {
                measure_waveform_DSP(g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE, 
                                    sample_interval, &g_waveform_data.ch1_params);
            }
            
            if (g_waveform_config.channel_enable & 0x02) {
                measure_waveform_DSP(g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE,
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

/**
 * @brief  设置电压刻度
 */
void waveform_set_vscale(uint8_t channel, float v_per_div) {
    if (v_per_div > 0.0f && v_per_div <= 10.0f) {
        g_waveform_config.v_scale = v_per_div;
    }
}

/**
 * @brief  设置时间刻度
 */
void waveform_set_tscale(float t_per_div) {
    if (t_per_div > 0.0f && t_per_div <= 1.0f) {
        g_waveform_config.t_scale = t_per_div;
    }
}

/**
 * @brief  设置触发参数
 */
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

/**
 * @brief  使能/禁用通道
 */
void waveform_enable_channel(uint8_t channel, uint8_t enable) {
    if (channel == 1) {
        g_waveform_config.channel_enable = enable ? 
            (g_waveform_config.channel_enable | 0x01) : 
            (g_waveform_config.channel_enable & ~0x01);
    } else if (channel == 2) {
        g_waveform_config.channel_enable = enable ? 
            (g_waveform_config.channel_enable | 0x02) : 
            (g_waveform_config.channel_enable & ~0x02);
    }
}

/**
 * @brief  获取波形数据指针
 */
waveform_data_t* waveform_get_data(void) {
    return &g_waveform_data;
}

/**
 * @brief  获取波形配置指针
 */
waveform_config_t* waveform_get_config(void) {
    return &g_waveform_config;
}