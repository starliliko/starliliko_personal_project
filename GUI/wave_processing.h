#ifndef WAVE_PROCESSING_H
#define WAVE_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846
#define BUFFER_SIZE     1024      // 波形缓冲区大小
#define MAX_EXTERNAL_FREQ 200000.0f  // 外部波形最高频率限制：200kHz

// 波形类型枚举
typedef enum {
    WAVE_TYPE_SINE,       // 正弦波
    WAVE_TYPE_SQUARE,     // 方波
    WAVE_TYPE_TRIANGLE,   // 三角波
    WAVE_TYPE_UNKNOWN     // 未知波形
} wave_type_t;

// 波形参数结构体
typedef struct {
    float amplitude;      // 幅度 (V)
    float frequency;      // 频率 (Hz)
    float offset;         // 直流偏移 (V)
    wave_type_t type;     // 波形类型
} wave_params_t;

/**
 * 生成正弦波
 * @param freq      频率 (Hz)
 * @param amp       幅度 (V)
 * @param offset    直流偏移 (V)
 * @param sample_interval 采样间隔 (秒)
 * @param buffer    输出缓冲区
 * @param buf_len   缓冲区长度
 * @param phase     相位指针（用于连续生成波形）
 */
void generate_sine_wave(float freq, float amp, float offset, float sample_interval, 
                       float *buffer, uint16_t buf_len, float *phase);

/**
 * 生成方波
 * @param freq      频率 (Hz)
 * @param amp       幅度 (V)
 * @param offset    直流偏移 (V)
 * @param duty_cycle 占空比 (0.0-1.0)
 * @param sample_interval 采样间隔 (秒)
 * @param buffer    输出缓冲区
 * @param buf_len   缓冲区长度
 * @param phase     相位指针（用于连续生成波形）
 */
void generate_square_wave(float freq, float amp, float offset, float duty_cycle,
                         float sample_interval, float *buffer, uint16_t buf_len, float *phase);

/**
 * 生成三角波
 * @param freq      频率 (Hz)
 * @param amp       幅度 (V)
 * @param offset    直流偏移 (V)
 * @param sample_interval 采样间隔 (秒)
 * @param buffer    输出缓冲区
 * @param buf_len   缓冲区长度
 * @param phase     相位指针（用于连续生成波形）
 */
void generate_triangle_wave(float freq, float amp, float offset, float sample_interval,
                           float *buffer, uint16_t buf_len, float *phase);

/**
 * 计算波形频率
 * @param wave_buf  波形数据缓冲区（电压值）
 * @param buf_len   缓冲区长度
 * @param sample_interval 采样间隔 (秒)
 * @return          计算得到的频率 (Hz)
 */
float calculate_wave_frequency(const float *wave_buf, uint16_t buf_len, float sample_interval);

/**
 * 测量波形电压参数
 * @param wave_buf  波形数据缓冲区（电压值）
 * @param buf_len   缓冲区长度
 * @param max_v     输出：最大电压
 * @param min_v     输出：最小电压
 * @param avg_v     输出：平均电压（直流分量）
 * @param rms_v     输出：有效值（仅对交流信号有效）
 */
void measure_wave_voltage(const float *wave_buf, uint16_t buf_len,
                         float *max_v, float *min_v, float *avg_v, float *rms_v);

/**
 * 判断波形类型
 * @param wave_buf  波形数据缓冲区（电压值）
 * @param buf_len   缓冲区长度
 * @param freq      波形频率 (Hz)
 * @param sample_interval 采样间隔 (秒)
 * @return          波形类型
 */
wave_type_t determine_wave_type(const float *wave_buf, uint16_t buf_len, 
                              float freq, float sample_interval);

/**
 * 处理外部输入波形（限制频率并标准化）
 * @param input_buf 输入波形缓冲区
 * @param output_buf 输出处理后的波形缓冲区
 * @param buf_len   缓冲区长度
 * @param sample_interval 采样间隔 (秒)
 * @return          处理后的波形参数
 */
wave_params_t process_external_wave(const float *input_buf, float *output_buf, 
                                   uint16_t buf_len, float sample_interval);

#endif // WAVE_PROCESSING_H