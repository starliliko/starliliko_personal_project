#ifndef __ADC_TASK_H
#define __ADC_TASK_H

#include "main.h"

// ✅ 修改: FFT优化的缓冲区大小 (必须是2的幂次)
#define ADC_BUFFER_SIZE 2048     // 增加到2048 (2^11) 适合FFT
#define CHANNEL_BUFFER_SIZE 1024 // 每通道1024个样本 (2^10)

// 外部访问的ADC数据
extern uint16_t adc_ch1[CHANNEL_BUFFER_SIZE];
extern uint16_t adc_ch2[CHANNEL_BUFFER_SIZE];
extern volatile uint64_t count_buffer;

// 任务函数
void ADC_acquisition_task(void *argument);

// 采样率相关函数
float ADC_Get_Sample_Rate(void);
HAL_StatusTypeDef ADC_Set_Sample_Rate(float target_rate);

#endif /* __ADC_TASK_H */