#include "wave_processing.h"
#include "arm_math.h"
/**
 * 计算波形频率
 */
float calculate_wave_frequency(const float *wave_buf, uint16_t buf_len, float sample_interval) {
    if (!wave_buf || buf_len < 2) return 0.0f;
    
    uint16_t zero_cross_cnt = 0;
    uint16_t zero_cross_pos[10] = {0};
    const float trigger_level = 0.0f;  // 0V过零点
    
    // 检测上升沿过零点（前一采样点<0，当前采样点>=0）
    for (uint16_t i = 1; i < buf_len; i++) {
        if (wave_buf[i-1] < trigger_level && wave_buf[i] >= trigger_level) {
            zero_cross_pos[zero_cross_cnt++] = i;
            if (zero_cross_cnt >= sizeof(zero_cross_pos)/sizeof(zero_cross_pos[0])) {
                break;  // 最多检测10个过零点
            }
        }
    }
    
    // 至少2个过零点才能计算周期
    if (zero_cross_cnt >= 2) {
        uint16_t cycle_samples = zero_cross_pos[1] - zero_cross_pos[0];  // 周期对应的采样数
        float cycle = sample_interval * cycle_samples;  // 周期（秒）
        float freq = (cycle > 1e-6f) ? (1.0f / cycle) : 0.0f;  // 频率=1/周期
        
        // 限制外部波形最高频率
        return (freq > MAX_EXTERNAL_FREQ) ? MAX_EXTERNAL_FREQ : freq;
    }
    
    return 0.0f;
}

/**
 * 测量波形电压参数
 */
void measure_wave_voltage(const float *wave_buf, uint16_t buf_len,
                         float *max_v, float *min_v, float *avg_v, float *rms_v) {
    if (!wave_buf || !max_v || !min_v || !avg_v || !rms_v || buf_len == 0) return;
    
    *max_v = wave_buf[0];
    *min_v = wave_buf[0];
    *avg_v = 0.0f;
    *rms_v = 0.0f;
    
    // 计算最大、最小、平均和有效值
    for (uint16_t i = 0; i < buf_len; i++) {
        if (wave_buf[i] > *max_v) *max_v = wave_buf[i];
        if (wave_buf[i] < *min_v) *min_v = wave_buf[i];
        *avg_v += wave_buf[i];
        *rms_v += wave_buf[i] * wave_buf[i];
    }
    
    *avg_v /= buf_len;
    *rms_v = sqrt(*rms_v / buf_len);
}

/**
 * 判断波形类型
 */
wave_type_t determine_wave_type(const float *wave_buf, uint16_t buf_len, 
                              float freq, float sample_interval) {
    if (!wave_buf || buf_len < 100 || freq <= 0) return WAVE_TYPE_UNKNOWN;
    
    // 计算一个周期内的采样点数
    float period = 1.0f / freq;
    uint16_t samples_per_period = (uint16_t)(period / sample_interval);
    
    // 确保有足够的采样点来分析一个周期
    if (samples_per_period < 10 || samples_per_period > buf_len / 2) {
        return WAVE_TYPE_UNKNOWN;
    }
    
    // 计算波形参数
    float max_v, min_v, avg_v, rms_v;
    measure_wave_voltage(wave_buf, buf_len, &max_v, &min_v, &avg_v, &rms_v);
    float amplitude = (max_v - min_v) / 2.0f;
    
    // 如果幅度太小，无法准确判断
    if (amplitude < 0.01f) return WAVE_TYPE_UNKNOWN;
    
    // 计算过零点数量（一个周期内）
    uint16_t zero_crossings = 0;
    for (uint16_t i = 1; i < samples_per_period; i++) {
        if ((wave_buf[i-1] - avg_v) * (wave_buf[i] - avg_v) <= 0) {
            zero_crossings++;
        }
    }
    
    // 方波特征：陡峭的上升沿和下降沿，低次谐波丰富
    bool is_square = false;
    uint16_t sharp_edges = 0;
    float max_slope = 0.0f;
    
    // 计算斜率来检测陡峭边缘
    for (uint16_t i = 1; i < buf_len; i++) {
        float slope = fabs(wave_buf[i] - wave_buf[i-1]) / sample_interval;
        if (slope > max_slope) max_slope = slope;
        if (slope > 1000.0f * amplitude * freq) {  // 陡峭边缘判断阈值
            sharp_edges++;
        }
    }
    
    // 方波通常有2个过零点和较多的陡峭边缘
    if (zero_crossings == 2 && sharp_edges > 2 * (buf_len / samples_per_period)) {
        is_square = true;
    }
    
    if (is_square) {
        return WAVE_TYPE_SQUARE;
    }
    
    // 正弦波特征：有效值与振幅的关系 rms ≈ amplitude / √2
    float sine_rms_ratio = rms_v / amplitude;
    bool is_sine = (fabs(sine_rms_ratio - 1.0f / sqrt(2.0f)) < 0.1f);
    
    if (is_sine) {
        return WAVE_TYPE_SINE;
    }
    
    // 三角波特征：有效值与振幅的关系 rms ≈ amplitude / √3
    float triangle_rms_ratio = rms_v / amplitude;
    bool is_triangle = (fabs(triangle_rms_ratio - 1.0f / sqrt(3.0f)) < 0.15f);
    
    if (is_triangle) {
        return WAVE_TYPE_TRIANGLE;
    }
    
    // 无法识别的波形类型
    return WAVE_TYPE_UNKNOWN;
}

/**
 * 处理外部输入波形
 */
wave_params_t process_external_wave(const float *input_buf, float *output_buf, 
                                   uint16_t buf_len, float sample_interval) {
    wave_params_t params = {0};
    
    if (!input_buf || !output_buf || buf_len != BUFFER_SIZE) {
        return params;
    }
    
    // 复制并处理输入波形
    memcpy(output_buf, input_buf, buf_len * sizeof(float));
    
    // 计算波形参数
    params.frequency = calculate_wave_frequency(output_buf, buf_len, sample_interval);
    measure_wave_voltage(output_buf, buf_len, 
                        &params.amplitude, &params.offset, &params.offset, &params.amplitude);
    params.amplitude = (params.amplitude - params.offset) / 2.0f;  // 修正幅度计算
    
    // 判断波形类型
    params.type = determine_wave_type(output_buf, buf_len, params.frequency, sample_interval);
    
    return params;
}