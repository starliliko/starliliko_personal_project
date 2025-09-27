#ifndef ADC_TASK_H
#define ADC_TASK_H

#include "main.h"
#include "cmsis_os.h"

// 外部变量声明
extern osMessageQueueId_t adc_data_queue;
extern osThreadId_t adc_task_handle;

// 函数声明
void ADC_Collection_Init(void);
void ADC_acquisition_task(void *argument);
void ADC_Separate_Channels(uint16_t *src, uint16_t *ch1, uint16_t *ch2);


#endif // ADC_TASK_H