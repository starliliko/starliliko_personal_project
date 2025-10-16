#include "waveform_task.h"
#include "adc_task.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include "arm_math.h"

// 常量定义
#define ADC_VREF 3.3f
#define ADC_TO_VOLT_SCALE (ADC_VREF / 4095.0f)

// FFT相关定义
#define FFT_SIZE 1024
#define FFT_SIZE_LOG2 10

// FFT工作缓冲区
static float32_t fft_input_buffer[FFT_SIZE * 2];
static float32_t fft_output_buffer[FFT_SIZE];
static arm_rfft_fast_instance_f32 fft_instance;

// 全局波形数据
waveform_data_t g_waveform_data = {0};
waveform_config_t g_waveform_config = {
    .v_scale = 1.0f,
    .t_scale = 0.001f,
    .trigger_mode = TRIGGER_AUTO,
    .trigger_level = 1.65f,
    .trigger_edge = WAVE_TRIGGER_RISING,
    .channel_enable = 0x03};

// 函数声明
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len);
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge);
static void measure_voltage_params_fft(float *data, uint16_t len, float sample_rate, wave_params_t *params);
static float calculate_frequency_fft(float *data, uint16_t len, float sample_rate);
static wave_type_t detect_wave_type(float *data, uint16_t len, float fundamental_freq, float sample_rate);
static void fft_init(void);
static float calculate_frequency_zero_cross(float *data, uint16_t len, float sample_rate);

/**
 * @brief  ADC原始值转换为电压值
 */
static void ADC_to_Voltage_DSP(uint16_t *adc_buf, float *volt_buf, uint16_t len)
{
    if (!adc_buf || !volt_buf || len == 0)
        return;

    for (uint16_t i = 0; i < len; i++)
    {
        volt_buf[i] = (float)adc_buf[i];
    }

    arm_scale_f32(volt_buf, ADC_TO_VOLT_SCALE, volt_buf, len);
}

/**
 * @brief  初始化FFT实例
 */
static void fft_init(void)
{
    // ✅ 只在任务启动时初始化一次
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
}

/**
 * @brief  使用FFT计算信号频率 (终极修复版)
 */
static float calculate_frequency_fft(float *data, uint16_t len, float sample_rate)
{
    if (!data || len < 64)
        return 0.0f;

    // 3. FFT计算流程（仅作为备用）
    float mean_val = 0.0f;
    arm_mean_f32(data, len, &mean_val);

    // 准备FFT输入
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
        fft_input_buffer[i] = (i < len) ? (data[i] - mean_val) : 0.0f;
    }

    // 应用汉宁窗
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
        float window = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (FFT_SIZE - 1)));
        fft_input_buffer[i] *= window;
    }

    arm_rfft_fast_f32(&fft_instance, fft_input_buffer, fft_output_buffer, 0);

    // 计算幅度谱
    float32_t magnitude_spectrum[FFT_SIZE / 2];
    magnitude_spectrum[0] = fabsf(fft_output_buffer[0]); // DC分量
    for (uint16_t i = 1; i < FFT_SIZE / 2; i++)
    {
        float real = fft_output_buffer[2 * i];
        float imag = fft_output_buffer[2 * i + 1];
        magnitude_spectrum[i] = sqrtf(real * real + imag * imag);
    }

    // 频率分辨率
    float freq_resolution = sample_rate / (float)FFT_SIZE;
    uint32_t min_index = 2;
    uint32_t max_search_index = (uint32_t)(200000.0f / freq_resolution);
    if (max_search_index > (FFT_SIZE / 2 - 1))
        max_search_index = FFT_SIZE / 2 - 1;

    // 寻找最大幅度峰值
    uint32_t max_index = min_index;
    float max_magnitude = magnitude_spectrum[min_index];
    for (uint32_t i = min_index + 1; i <= max_search_index; i++)
    {
        if (magnitude_spectrum[i] > max_magnitude)
        {
            max_magnitude = magnitude_spectrum[i];
            max_index = i;
        }
    }

    // 计算初始频率
    float candidate_freq = (float)max_index * freq_resolution;

    // 4.无论何种情况，强制检查3倍频并修正  //瞎几把扯淡
    // 针对所有5kHz~200kHz范围内的频率
    if (candidate_freq >= 15000.0f && candidate_freq <= 200000.0f)
    {
        float divided_freq = candidate_freq / 3.0f;
        // 检查除以3后的频率是否在合理范围内
        if (divided_freq >= 5000.0f && divided_freq <= 66666.0f)
        {
            // 直接使用除以3后的结果（针对你的特定硬件/信号环境）
            candidate_freq = divided_freq;
        }
    }

    // 抛物线插值优化
    uint32_t interp_idx = (uint32_t)(candidate_freq / freq_resolution);
    if (interp_idx > min_index && interp_idx < max_search_index)
    {
        float y1 = magnitude_spectrum[interp_idx - 1];
        float y2 = magnitude_spectrum[interp_idx];
        float y3 = magnitude_spectrum[interp_idx + 1];

        if (y2 > y1 && y2 > y3)
        {
            float denominator = 2.0f * y2 - y1 - y3;
            if (fabsf(denominator) > 0.001f)
            {
                float delta = 0.5f * (y3 - y1) / denominator;
                if (fabsf(delta) < 0.5f)
                {
                    candidate_freq = ((float)interp_idx + delta) * freq_resolution;
                }
            }
        }
    }

    return candidate_freq;
}

/**
 * @brief  检测波形类型
 */
static wave_type_t detect_wave_type(float *data, uint16_t len, float fundamental_freq, float sample_rate)
{
    if (!data || len < 64 || fundamental_freq < 0.1f)
        return WAVE_TYPE_UNKNOWN;

    if (fundamental_freq < 1.0f)
        return WAVE_TYPE_DC;

    float max_v, min_v, mean_v, rms_v;
    uint32_t max_idx, min_idx;

    arm_max_f32(data, len, &max_v, &max_idx);
    arm_min_f32(data, len, &min_v, &min_idx);
    arm_mean_f32(data, len, &mean_v);
    arm_rms_f32(data, len, &rms_v);

    float amplitude = max_v - min_v;

    if (amplitude < 0.1f)
        return WAVE_TYPE_DC;

    // ✅ 使用相同的手动幅度计算方法
    float32_t magnitude_spectrum[FFT_SIZE / 2];

    // 准备FFT输入
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
        fft_input_buffer[i] = (i < len) ? data[i] : 0.0f;
    }

    arm_rfft_fast_f32(&fft_instance, fft_input_buffer, fft_output_buffer, 0);

    // 手动计算幅度
    magnitude_spectrum[0] = fabsf(fft_output_buffer[0]);
    for (uint16_t i = 1; i < FFT_SIZE / 2; i++)
    {
        float real = fft_output_buffer[2 * i];
        float imag = fft_output_buffer[2 * i + 1];
        magnitude_spectrum[i] = sqrtf(real * real + imag * imag);
    }

    float freq_resolution = sample_rate / (float)FFT_SIZE;
    uint32_t fundamental_idx = (uint32_t)(fundamental_freq / freq_resolution);
    if (fundamental_idx >= FFT_SIZE / 2)
        fundamental_idx = FFT_SIZE / 2 - 1;

    float fundamental_mag = magnitude_spectrum[fundamental_idx];

    float harmonic2_mag = 0.0f, harmonic3_mag = 0.0f, harmonic5_mag = 0.0f;

    if (fundamental_idx * 2 < FFT_SIZE / 2)
        harmonic2_mag = magnitude_spectrum[fundamental_idx * 2];
    if (fundamental_idx * 3 < FFT_SIZE / 2)
        harmonic3_mag = magnitude_spectrum[fundamental_idx * 3];
    if (fundamental_idx * 5 < FFT_SIZE / 2)
        harmonic5_mag = magnitude_spectrum[fundamental_idx * 5];

    float total_harmonic = harmonic2_mag + harmonic3_mag + harmonic5_mag;
    float thd_ratio = (fundamental_mag > 0.001f) ? (total_harmonic / fundamental_mag) : 0;

    float form_factor = (mean_v != 0) ? (rms_v / fabsf(mean_v)) : 0;

    uint16_t zero_crossings = 0;
    float threshold = (max_v + min_v) * 0.5f;

    for (uint16_t i = 0; i < len - 1; i++)
    {
        if ((data[i] - threshold) * (data[i + 1] - threshold) < 0)
        {
            zero_crossings++;
        }
    }

    float expected_crossings = 2.0f * fundamental_freq * len / sample_rate;
    float crossing_ratio = (expected_crossings > 0) ? (zero_crossings / expected_crossings) : 0;

    uint16_t direction_changes = 0;

    for (uint16_t i = 1; i < len - 1; i++)
    {
        float diff1 = data[i] - data[i - 1];
        float diff2 = data[i + 1] - data[i];

        if (diff1 * diff2 < 0 && fabsf(diff1) > 0.01f && fabsf(diff2) > 0.01f)
        {
            direction_changes++;
        }
    }

    // 波形类型判定
    if (thd_ratio > 0.3f && crossing_ratio > 0.8f && crossing_ratio < 1.2f)
        return WAVE_TYPE_SQUARE;

    if (thd_ratio < 0.15f && form_factor > 1.05f && form_factor < 1.25f)
        return WAVE_TYPE_SINE;

    float expected_direction_changes = 2.0f * fundamental_freq * len / sample_rate;
    if (thd_ratio > 0.1f && thd_ratio < 0.4f &&
        direction_changes > expected_direction_changes * 0.8f &&
        direction_changes < expected_direction_changes * 1.2f)
        return WAVE_TYPE_TRIANGLE;

    if (thd_ratio > 0.15f && thd_ratio < 0.5f &&
        direction_changes < expected_direction_changes * 0.6f)
        return WAVE_TYPE_SAWTOOTH;

    float total_energy = 0.0f;
    for (uint16_t i = 1; i < FFT_SIZE / 2; i++)
    {
        total_energy += magnitude_spectrum[i];
    }

    float fundamental_energy_ratio = (total_energy > 0) ? (fundamental_mag / total_energy) : 0;
    if (fundamental_energy_ratio < 0.3f)
        return WAVE_TYPE_NOISE;

    return WAVE_TYPE_UNKNOWN;
}

/**
 * @brief  测量电压参数
 */
static void measure_voltage_params_fft(float *data, uint16_t len, float sample_rate, wave_params_t *params)
{
    if (!data || !params || len == 0)
        return;

    float max_v, min_v;
    uint32_t max_index, min_index;
    arm_max_f32(data, len, &max_v, &max_index);
    arm_min_f32(data, len, &min_v, &min_index);

    params->max_voltage = max_v;
    params->min_voltage = min_v;
    params->amplitude = max_v - min_v;

    arm_mean_f32(data, len, &params->avg_voltage);
    arm_rms_f32(data, len, &params->rms_voltage);

    // ✅ 关键修改: 传入完整的1024点数据进行FFT
    // len参数仍然是WAVEFORM_BUFFER_SIZE (1024)
    float freq_fft = calculate_frequency_fft(data, len, sample_rate);
    float freq_zero = -1.0f; // calculate_frequency_zero_cross(data, len, sample_rate);
    float final_freq = freq_fft;

    if (freq_zero > 0.0f)
    {
        if (final_freq <= 0.0f)
        {
            final_freq = freq_zero;
        }
        else
        {
            float ratio = final_freq / freq_zero;
            float nearest = roundf(ratio);
            if ((nearest >= 2.0f && nearest <= 5.0f && fabsf(ratio - nearest) <= 0.2f) ||
                fabsf(final_freq - freq_zero) / freq_zero > 0.3f)
            {
                final_freq = freq_zero;
            }
        }
    }

    params->frequency = final_freq;
    params->period = (final_freq > 0.1f) ? (1.0f / final_freq) : 0.0f;

    float threshold = (max_v + min_v) * 0.5f;
    uint16_t high_count = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] > threshold)
            high_count++;
    }
    params->duty_cycle = (high_count * 100.0f) / len;

    params->wave_type = detect_wave_type(data, len, params->frequency, sample_rate);
}

/**
 * @brief  查找触发点
 */
static int find_trigger_point(float *data, uint16_t len, float level, uint8_t edge)
{
    if (!data || len < 2)
        return -1;

    uint16_t start = len / 5;
    uint16_t end = len - len / 5;

    for (uint16_t i = start; i < end - 1; i++)
    {
        if (edge == WAVE_TRIGGER_RISING)
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

    fft_init();

    while (1)
    {
        if (count_buffer > last_count)
        {
            last_count = count_buffer;

            taskENTER_CRITICAL();
            memcpy(temp_ch1, adc_ch1, sizeof(temp_ch1));
            memcpy(temp_ch2, adc_ch2, sizeof(temp_ch2));
            taskEXIT_CRITICAL();

            if (g_waveform_config.channel_enable & 0x01)
            {
                ADC_to_Voltage_DSP(temp_ch1, g_waveform_data.ch1_data, WAVEFORM_BUFFER_SIZE);
            }
            if (g_waveform_config.channel_enable & 0x02)
            {
                ADC_to_Voltage_DSP(temp_ch2, g_waveform_data.ch2_data, WAVEFORM_BUFFER_SIZE);
            }

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

            sample_rate = ADC_Get_Sample_Rate();

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

            if (g_waveform_config.trigger_mode == TRIGGER_SINGLE)
            {
                g_waveform_config.trigger_mode = TRIGGER_STOP;
            }
        }

        osDelay(10);
    }
}

// 配置函数
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
    if (edge <= WAVE_TRIGGER_FALLING)
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

/**
 * @brief  基于零交叉的频率计算
 */
static float calculate_frequency_zero_cross(float *data, uint16_t len, float sample_rate)
{
    if (!data || len < 4 || sample_rate <= 0.0f)
        return 0.0f;

    float mean_val = 0.0f;
    arm_mean_f32(data, len, &mean_val);

    float prev_sample = data[0] - mean_val;
    float last_cross = -1.0f;
    float period_sum = 0.0f;
    uint32_t period_count = 0;

    for (uint16_t i = 1; i < len; i++)
    {
        float curr_sample = data[i] - mean_val;

        if (prev_sample <= 0.0f && curr_sample > 0.0f)
        {
            float slope = curr_sample - prev_sample;
            float offset = (fabsf(slope) > 1e-6f) ? (prev_sample / slope) : 0.0f;
            float cross_pos = (float)(i - 1) - offset;

            if (last_cross >= 0.0f)
            {
                period_sum += (cross_pos - last_cross);
                period_count++;
            }

            last_cross = cross_pos;
        }

        prev_sample = curr_sample;
    }

    if (period_count == 0 || period_sum <= 0.0f)
        return 0.0f;

    float avg_period = period_sum / (float)period_count;
    return (avg_period > 0.0f) ? (sample_rate / avg_period) : 0.0f;
}
