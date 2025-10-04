#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数
#define ADC_BUFFER_SIZE     1024    // 单个缓冲区的大小（双通道各512点）
#define CHANNEL_BUFFER_SIZE (ADC_BUFFER_SIZE / 2)  // 单通道采样点数

// 使用一个连续的双倍缓冲区
uint16_t adc_double_buffer[ADC_BUFFER_SIZE * 2] __attribute__((aligned(4))) = {0};

// 分离后的通道数据缓冲区
uint16_t adc_ch1[CHANNEL_BUFFER_SIZE] = {0};
uint16_t adc_ch2[CHANNEL_BUFFER_SIZE] = {0};

 // 使用二进制信号量进行通知 ---
osSemaphoreId_t adc_data_sem;

// volatile确保编译器不会优化掉对它的访问，保证在中断和任务间可见
volatile uint16_t *filled_buffer_ptr = NULL;

uint64_t count_buffer = 0; // 记录处理了多少个缓冲区

// 函数前向声明
void ADC_Collection_Init(void);

/**
 * @brief  分离交错存储的双通道ADC数据
 */
void ADC_Separate_Channels(uint16_t *src, uint16_t *ch1, uint16_t *ch2) {
    if (!src || !ch1 || !ch2) return;
    for (uint16_t i = 0; i < CHANNEL_BUFFER_SIZE; i++) {
        ch1[i] = src[2 * i];
        ch2[i] = src[2 * i + 1];
    }
}

/**
 * @brief  ADC采集初始化（采用无缝采集方案）
 */
void ADC_Collection_Init(void) {
    // 创建二进制信号量 
    adc_data_sem = osSemaphoreNew(1, 0, NULL);

    // 启动ADC的DMA传输，仅需调用一次
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_double_buffer, ADC_BUFFER_SIZE * 2) != HAL_OK)
    {
        Error_Handler();
    }
    //重点在stmcubemx配置
     HAL_TIM_Base_Start(&htim2); // 启动TIM2
}

/**
 * @brief  ADC采集任务（优化为事件驱动模型）
 */
void ADC_acquisition_task(void *argument) {
    ADC_Collection_Init();

    while (1) {
        // 阻塞等待信号量，而不是消息 
        osStatus_t status = osSemaphoreAcquire(adc_data_sem, osWaitForever);

        if (status == osOK) {
            // 从全局指针获取已填满的缓冲区地址
            uint16_t *buffer_to_process = (uint16_t *)filled_buffer_ptr;

            // 分离数据
            ADC_Separate_Channels(buffer_to_process, adc_ch1, adc_ch2);

            count_buffer++; // 计数处理了多少个缓冲区
        }
    }
}

/**
 * @brief  DMA传输半满回调 (当双缓冲区的前半部分填满时调用)
 * @note   此函数在中断上下文中执行，必须快进快出
 */
__attribute__((section(".RamFunc")))
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    // 1. 更新全局指针，指向已填满的前半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[0];
    // 2. 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}

/**
 * @brief  DMA传输完成回调 (当双缓冲区的后半部分填满时调用)
 * @note   此函数在中断上下文中执行，必须快进快出
 */
__attribute__((section(".RamFunc")))
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    // 1. 更新全局指针，指向已填满的后半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[ADC_BUFFER_SIZE];
    // 2. 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}
