#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数
#define ADC_BUFFER_SIZE     1024    // 总采样点数（双通道各512点）
#define CHANNEL_BUFFER_SIZE (ADC_BUFFER_SIZE / 2)  // 单通道采样点数

HAL_StatusTypeDef status=HAL_OK;
// 双缓冲设计
uint16_t adc_buffer_A[ADC_BUFFER_SIZE] __attribute__((aligned(4))) = {0};
uint16_t adc_buffer_B[ADC_BUFFER_SIZE] __attribute__((aligned(4))) = {0};
uint16_t *current_buffer = adc_buffer_A;  // 当前DMA写入的缓冲区

// 分离后的通道数据缓冲区
uint16_t adc_ch1[CHANNEL_BUFFER_SIZE] = {0};
uint16_t adc_ch2[CHANNEL_BUFFER_SIZE] = {0};

// 全局标志位（volatile确保多线程可见性）
volatile uint8_t adc_data_ready = 0;      // 数据就绪标志（0：未就绪，1：有新数据）
volatile uint8_t buffer_switch_lock = 0;  // 缓冲区切换锁（0：未锁定，1：锁定中）

// 数据队列
osMessageQueueId_t adc_data_queue;

uint64_t a = 0;  // 采样计数

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
 * @brief  ADC采集初始化（仅创建队列和启动DMA）
 */
void ADC_Collection_Init(void) {
    // 创建数据队列（存储分离后的通道数据指针）
    adc_data_queue = osMessageQueueNew(5, sizeof(uint16_t*), NULL);

    HAL_ADC_Stop(&hadc1);          // 停止ADC常规转换（清除ADC内部忙碌状态）
    HAL_ADC_Stop_DMA(&hadc1);      // 停止DMA传输（清除DMA忙碌状态）
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t*)current_buffer, ADC_BUFFER_SIZE);
    if (status != HAL_OK) {
        // 打印错误码（在调试器中观察此值，或通过串口输出）
        //printf("ADC DMA启动失败！错误码：%d\n", status);  // 需要初始化串口
        Error_Handler();
    }
}

/**
 * @brief  ADC采集任务（通过标志位检测数据就绪）
 */
void ADC_acquisition_task(void *argument) {
    uint16_t *filled_buffer;

    ADC_Collection_Init();

    while (1) {
        // 轮询等待数据就绪标志
        if (adc_data_ready) {
            // 临界区：获取已完成的缓冲区（禁用中断避免冲突）
            __disable_irq();
            filled_buffer = (current_buffer == adc_buffer_A) ? adc_buffer_B : adc_buffer_A;
            __enable_irq();

            // 分离数据
            ADC_Separate_Channels(filled_buffer, adc_ch1, adc_ch2);

            // 发送到队列
            // osMessageQueuePut(adc_data_queue, &adc_ch1, 0, osWaitForever);
            // osMessageQueuePut(adc_data_queue, &adc_ch2, 0, osWaitForever);

            // 清除就绪标志（处理完成）
            adc_data_ready = 0;
        }

        // 短暂延迟，降低CPU占用（替代阻塞等待）
        osDelay(1);
    }
}

/**
 * @brief  DMA传输完成回调（通过标志位通知任务）
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc != &hadc1) return;

    // 锁定缓冲区切换（避免与任务冲突）
    buffer_switch_lock = 1;

    // 切换缓冲区（标志位保护，替代互斥锁）
    current_buffer = (current_buffer == adc_buffer_A) ? adc_buffer_B : adc_buffer_A;

    HAL_ADC_Stop_DMA(&hadc1);  // 强制停止当前DMA传输（清除忙碌状态）
    // 重启DMA
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)current_buffer, ADC_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }

    a++;

    // 设置数据就绪标志（通知任务处理）
    adc_data_ready = 1;

    // 解锁缓冲区切换
    buffer_switch_lock = 0;
}
