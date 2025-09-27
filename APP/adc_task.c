#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数（与CubeMX配置匹配）
#define ADC_BUFFER_SIZE     1024    // 总采样点数（双通道各512点）

// 双缓冲设计（避免DMA覆盖数据）
uint16_t adc_buffer_A[ADC_BUFFER_SIZE];
uint16_t adc_buffer_B[ADC_BUFFER_SIZE];
uint16_t *current_buffer = adc_buffer_A;  // 当前活跃缓冲区

// FreeRTOS通信机制
osMessageQueueId_t adc_data_queue;        // 数据队列句柄
osSemaphoreId_t adc_mutex;                // 缓冲区互斥锁

// 任务句柄
osThreadId_t adc_task_handle;

uint64_t a;

/**
 * @brief  ADC采集任务初始化
 * @param  无
 * @retval 无
 */
void ADC_Collection_Init(void) {
    // 创建互斥锁（保护缓冲区访问）
    adc_mutex = osSemaphoreNew(1, 1, NULL);
    // 创建数据队列（存储缓冲区指针）
    adc_data_queue = osMessageQueueNew(5, sizeof(uint16_t*), NULL);
    // 启动ADC DMA采集
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)current_buffer, ADC_BUFFER_SIZE);
}

/**
 * @brief  ADC采集任务（FreeRTOS任务）
 * @param  argument: 未使用
 * @retval 无
 */
void ADC_acquisition_task(void *argument) {
    uint16_t *filled_buffer;  // 已填满数据的缓冲区

    while (1) 
    {
        // // 等待DMA传输完成通知（由中断触发）
        // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // // 获取互斥锁，切换缓冲区
        // osSemaphoreAcquire(adc_mutex, portMAX_DELAY);
        // filled_buffer = current_buffer;
        // // 切换到另一个缓冲区
        // current_buffer = (current_buffer == adc_buffer_A) ? adc_buffer_B : adc_buffer_A;
        // osSemaphoreRelease(adc_mutex);
        
        // // 将填满的缓冲区指针发送到队列
        // osMessageQueuePut(adc_data_queue, &filled_buffer, 0, 0);
        osDelay(5);
    }
}


/**
 * @brief  DMA传输完成回调函数
 * @param  hadc: ADC句柄
 * @retval 无
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) 
{
        //BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // // 通知ADC任务处理数据（不阻塞中断）
        // vTaskNotifyGiveFromISR(adc_task_handle, &xHigherPriorityTaskWoken);
        
        // // 切换缓冲区并重启DMA（在中断中快速完成）
        // osSemaphoreAcquire(adc_mutex, 0);  // 非阻塞获取锁
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)current_buffer, ADC_BUFFER_SIZE);
        a++;
        // osSemaphoreRelease(adc_mutex);
        
        // // // 触发任务调度（如果需要）
        //  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
