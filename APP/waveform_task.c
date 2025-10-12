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

// FFT相关定义
#define FFT_SIZE 1024    // FFT长度 (必须等于CHANNEL_BUFFER_SIZE)
#define FFT_SIZE_LOG2 10 // log2(1024) = 10

// FFT工作缓冲区
static float32_t fft_input_buffer[FFT_SIZE * 2]; // 复数输入 (实部+虚部)
static float32_t fft_output_buffer[FFT_SIZE];    // 幅度输出
static arm_rfft_fast_instance_f32 fft_instance;  // FFT实例

// 全局波形数据
waveform_data_t g_waveform_data = {0};
waveform_config_t g_waveform_config = {
    .v_scale = 1.0f,
    .t_scale = 0.001f,
    .trigger_mode = TRIGGER_NORMAL,
    .trigger_level = 1.65f,
    .trigger_edge = TRIGGER_RISING,
    .channel_enable = 0x03};
// 初始设置

// 函数声明
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len);
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge);
static void measure_voltage_params_fft(float *data, uint16_t len, float sample_rate, wave_params_t *params);
static float calculate_frequency_fft(float *data, uint16_t len, float sample_rate);
static void fft_init(void);

/**
 * @brief  ADC原始值转换为电压值
 */
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len)
{
    if (!adc_buf || !volt_buf || len == 0)
        return;

    // 转换为浮点数
    for (uint16_t i = 0; i < len; i++)
    {
        volt_buf[i] = (float)adc_buf[i];
    }

    // 使用CMSIS-DSP库加速浮点运算
    arm_scale_f32(volt_buf, ADC_TO_VOLT_SCALE, volt_buf, len); // 向量缩放
}

/**
 * @brief  初始化FFT实例
 */
static void fft_init(void)
{
    // 初始化ARM CMSIS-DSP FFT实例
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
}

/**
 * @brief  使用FFT计算信号频率
 * @param  data: 输入电压数据
 * @param  len: 数据长度
 * @param  sample_rate: 采样频率
 * @return 主要频率分量 (Hz)
 */
static float calculate_frequency_fft(float *data, uint16_t len, float sample_rate)
{
    if (!data || len != FFT_SIZE)
        return 0.0f;

    // 1. 准备FFT输入数据
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
        fft_input_buffer[i] = data[i];
    }

    // 2. 执行实数FFT
    arm_rfft_fast_f32(&fft_instance, fft_input_buffer, fft_output_buffer, 0);

    // 3. 计算幅度谱 |X(k)| = sqrt(Re^2 + Im^2)
    float32_t magnitude_spectrum[FFT_SIZE / 2];
    arm_cmplx_mag_f32(fft_output_buffer, magnitude_spectrum, FFT_SIZE / 2);

    // 4. 忽略DC分量，查找最大幅度对应的频率
    uint32_t max_index = 1; // 从index=1开始 (忽略DC)
    float32_t max_magnitude = magnitude_spectrum[1];

    for (uint16_t i = 2; i < FFT_SIZE / 2; i++)
    {
        if (magnitude_spectrum[i] > max_magnitude)
        {
            max_magnitude = magnitude_spectrum[i];
            max_index = i;
        }
    }

    // 5. 计算对应频率: f = (index * sample_rate) / FFT_SIZE
    float frequency = ((float)max_index * sample_rate) / FFT_SIZE;

    // 6. 抛物线插值提高精度 (可选)
    if (max_index > 1 && max_index < (FFT_SIZE / 2 - 1))
    {
        float y1 = magnitude_spectrum[max_index - 1];
        float y2 = magnitude_spectrum[max_index];
        float y3 = magnitude_spectrum[max_index + 1];

        // 抛物线拟合峰值位置
        float delta = 0.5f * (y3 - y1) / (2.0f * y2 - y1 - y3);
        frequency = ((float)max_index + delta) * sample_rate / FFT_SIZE;
    }

    return frequency;
}

/**
 * @brief  测量电压参数 - 使用FFT计算频率
 */
static void measure_voltage_params_fft(float *data, uint16_t len, float sample_rate, wave_params_t *params)
{
    if (!data || !params || len == 0)
        return;

    // 1. 计算最大值和最小值
    float max_v, min_v;
    uint32_t max_index, min_index;
    arm_max_f32(data, len, &max_v, &max_index);
    arm_min_f32(data, len, &min_v, &min_index);

    params->max_voltage = max_v;
    params->min_voltage = min_v;
    params->amplitude = max_v - min_v;

    // 2. 计算平均值和RMS
    arm_mean_f32(data, len, &params->avg_voltage);
    arm_rms_f32(data, len, &params->rms_voltage);

    // 3. 使用FFT计算频率 - 高精度方法
    params->frequency = calculate_frequency_fft(data, len, sample_rate);

    if (params->frequency > 0.1f) // 避免除零
    {
        params->period = 1.0f / params->frequency;
    }
    else
    {
        params->period = 0.0f;
    }

    // 4. 占空比计算
    float threshold = (max_v + min_v) * 0.5f;
    uint16_t high_count = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] > threshold)
            high_count++;
    }
    params->duty_cycle = (high_count * 100.0f) / len;
}

/**
 * @brief  查找触发点
 */
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge)
{
    if (!data || len < 2)
        return -1;

    // 在中间区域查找，避免边界
    uint16_t start = len / 5;
    uint16_t end = len - len / 5;

    for (uint16_t i = start; i < end - 1; i++)
    {
        if (edge == TRIGGER_RISING)
        {
            if (data[i] <= level && data[i + 1] > level)
            {
                return i;
            }
        }
        else
        {
            if (data[i] >= level && data[i + 1] < level)
            {
                return i;
            }
        }
    }

    return -1;
}

/**
 * @brief  波形处理任务
 */
void waveform_process_task(void *argument)
{
    static uint16_t temp_ch1[WAVEFORM_BUFFER_SIZE];
    static uint16_t temp_ch2[WAVEFORM_BUFFER_SIZE];
    static uint64_t last_count = 0;
    float sample_rate;

    // 初始化FFT
    fft_init();

    while (1)
    {
        // 检查是否有新数据
        if (count_buffer > last_count)
        {
            last_count = count_buffer;

            // 复制ADC数据
            taskENTER_CRITICAL();
            memcpy(temp_ch1, adc_ch1, sizeof(temp_ch1));
            memcpy(temp_ch2, adc_ch2, sizeof(temp_ch2));
            taskEXIT_CRITICAL();

            // ADC转电压
            if (g_waveform_config.channel_enable & 0x01)
            {
                ADC_to_Voltage_DSP(temp_ch1, g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE);
            }
            if (g_waveform_config.channel_enable & 0x02)
            {
                ADC_to_Voltage_DSP(temp_ch2, g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE);
            }

            // 触发处理
            int trigger_pos = -1;
            if (g_waveform_config.trigger_mode != TRIGGER_AUTO)
            {
                trigger_pos = find_trigger_point(
                    g_waveform_data.ch1_data,
                    WAVEFORM_BUFFER_SIZE,
                    g_waveform_config.trigger_level,
                    g_waveform_config.trigger_edge);

                if (g_waveform_config.trigger_mode == TRIGGER_NORMAL && trigger_pos < 0)
                {
                    osDelay(1);
                    continue;
                }
            }

            g_waveform_data.trigger_position = (trigger_pos >= 0) ? trigger_pos : 0;

            sample_rate = ADC_Get_Sample_Rate(); // 获取实际采样率

            // 电压参数测量 - 使用FFT方法
            if (g_waveform_config.channel_enable & 0x01)
            {
                measure_voltage_params_fft(g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE,
                                           sample_rate, &g_waveform_data.ch1_params);
            }

            if (g_waveform_config.channel_enable & 0x02)
            {
                measure_voltage_params_fft(g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE,
                                           sample_rate, &g_waveform_data.ch2_params);
            }

            g_waveform_data.data_ready = 1;

            // 单次触发后停止
            if (g_waveform_config.trigger_mode == TRIGGER_SINGLE)
            {
                g_waveform_config.trigger_mode = TRIGGER_STOP;
            }
        }

        osDelay(10);
    }
}

// ==================== 配置函数 ====================

void waveform_set_vscale(uint8_t channel, float v_per_div)
{
    if (v_per_div > 0.0f && v_per_div <= 10.0f)
    {
        g_waveform_config.v_scale = v_per_div;
    }
}

void waveform_set_tscale(float t_per_div)
{
    if (t_per_div > 0.0f && t_per_div <= 1.0f)
    {
        g_waveform_config.t_scale = t_per_div;
    }
}

void waveform_set_trigger(uint8_t mode, float level, uint8_t edge)
{
    if (mode <= TRIGGER_SINGLE)
    {
        g_waveform_config.trigger_mode = mode;
    }
    if (level >= 0.0f && level <= ADC_VREF)
    {
        g_waveform_config.trigger_level = level;
    }
    if (edge <= TRIGGER_FALLING)
    {
        g_waveform_config.trigger_edge = edge;
    }
}

void waveform_enable_channel(uint8_t channel, uint8_t enable)
{
    if (channel == 1)
    {
        if (enable)
        {
            g_waveform_config.channel_enable |= 0x01;
        }
        else
        {
            g_waveform_config.channel_enable &= ~0x01;
        }
    }
    else if (channel == 2)
    {
        if (enable)
        {
            g_waveform_config.channel_enable |= 0x02;
        }
        else
        {
            g_waveform_config.channel_enable &= ~0x02;
        }
    }
}

waveform_data_t *waveform_get_data(void)
{
    return &g_waveform_data;
}

waveform_config_t *waveform_get_config(void)
{
    return &g_waveform_config;
}
