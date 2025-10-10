#ifndef __ADC_TASK_H
#define __ADC_TASK_H

#include "main.h"

// 缓冲区大小定义  
#define ADC_BUFFER_SIZE     1024
#define CHANNEL_BUFFER_SIZE (ADC_BUFFER_SIZE / 2)

// 外部访问的ADC数据
extern uint16_t adc_ch1[CHANNEL_BUFFER_SIZE];
extern uint16_t adc_ch2[CHANNEL_BUFFER_SIZE]; 
extern volatile uint64_t count_buffer;

// 任务函数
void ADC_acquisition_task(void *argument);

// 采样率相关函数
void ADC_SampleRate_Init(void);
float ADC_Get_Sample_Rate(void);
float ADC_Get_Sample_Interval(void);
float ADC_Get_Measured_Sample_Rate(void);
HAL_StatusTypeDef ADC_Set_Sample_Rate(float target_rate);

// 状态和调试函数
void ADC_Print_Status(void);

#endif /* __ADC_TASK_H */