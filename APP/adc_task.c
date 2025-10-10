#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数
#define ADC_BUFFER_SIZE     1024    // 单个缓冲区的大小（双通道各512点）
#define CHANNEL_BUFFER_SIZE (ADC_BUFFER_SIZE / 2)  // 单通道采样点数

// 使用一个连续的双倍缓冲区，确保内存对齐优化 DMA 性能
uint16_t adc_double_buffer[ADC_BUFFER_SIZE * 2] __attribute__((aligned(32))) = {0};

// 分离后的通道数据缓冲区（32字节对齐优化 FPU 访问）
uint16_t adc_ch1[CHANNEL_BUFFER_SIZE] __attribute__((aligned(32))) = {0};
uint16_t adc_ch2[CHANNEL_BUFFER_SIZE] __attribute__((aligned(32))) = {0};

// 使用二进制信号量进行通知
osSemaphoreId_t adc_data_sem;

// volatile确保编译器不会优化掉对它的访问，保证在中断和任务间可见
volatile uint16_t *filled_buffer_ptr = NULL;
volatile uint64_t count_buffer = 0; // 记录处理了多少个缓冲区

// 采样率相关变量
static float current_sample_rate = 1000000.0f;  // 默认1MHz

// 函数前向声明
static void ADC_Collection_Init(void);

/**
 * @brief  设置采样率（通过调整TIM2周期）
 * @param  target_rate: 目标采样率 (Hz)
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef ADC_Set_Sample_Rate(float target_rate) {
    if (target_rate <= 0 || target_rate > 2000000) {  // 最大2MHz
        return HAL_ERROR;
    }
    
    // 计算所需的定时器配置
    uint32_t tim2_clk = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clk_init;
    uint32_t flash_latency;
    HAL_RCC_GetClockConfig(&clk_init, &flash_latency);
    
    if (clk_init.APB1CLKDivider != RCC_HCLK_DIV1) {
        tim2_clk *= 2;
    }
    
    // 固定预分频器，调整周期值
    uint32_t prescaler = htim2.Init.Prescaler + 1;
    uint32_t new_period = (tim2_clk / prescaler / (uint32_t)target_rate) - 1;
    
    if (new_period > 0xFFFFFFFF) {  // TIM2是32位定时器
        return HAL_ERROR;
    }
    
    // 停止定时器
    HAL_TIM_Base_Stop(&htim2);
    
    // 更新配置
    htim2.Init.Period = new_period;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        return HAL_ERROR;
    }
    
    // 重启定时器
    HAL_TIM_Base_Start(&htim2);
    
    // 更新当前采样率
    current_sample_rate = target_rate;
    
    return HAL_OK;
}

/**
 * @brief  获取当前采样率
 * @return 当前采样率 (Hz)
 */
float ADC_Get_Sample_Rate(void) {
    return current_sample_rate;
}

/**
 * @brief  获取采样间隔
 * @return 采样间隔 (秒)
 */
float ADC_Get_Sample_Interval(void) {
    return 1.0f / current_sample_rate;
}

/**
 * @brief  分离交错存储的双通道ADC数据（优化版本）
 * @param  src: 源数据缓冲区
 * @param  ch1: 通道1输出缓冲区
 * @param  ch2: 通道2输出缓冲区
 */
static void ADC_Separate_Channels(uint16_t *src, uint16_t *ch1, uint16_t *ch2) {
    if (!src || !ch1 || !ch2) return;
    
    // 使用指针直接访问，减少数组索引计算
    uint16_t *src_ptr = src;
    uint16_t *ch1_ptr = ch1;
    uint16_t *ch2_ptr = ch2;
    
    // 循环展开 4 次，减少循环开销
    for (uint16_t i = 0; i < CHANNEL_BUFFER_SIZE / 4; i++) {
        // 第1组
        *ch1_ptr++ = *src_ptr++;
        *ch2_ptr++ = *src_ptr++;
        
        // 第2组
        *ch1_ptr++ = *src_ptr++;
        *ch2_ptr++ = *src_ptr++;
        
        // 第3组
        *ch1_ptr++ = *src_ptr++;
        *ch2_ptr++ = *src_ptr++;
        
        // 第4组
        *ch1_ptr++ = *src_ptr++;
        *ch2_ptr++ = *src_ptr++;
    }
    
    // 处理剩余数据
    uint16_t remainder_start = (CHANNEL_BUFFER_SIZE / 4) * 4;
    for (uint16_t i = remainder_start; i < CHANNEL_BUFFER_SIZE; i++) {
        ch1[i] = src[2 * i];
        ch2[i] = src[2 * i + 1];
    }
}

/**
 * @brief  ADC采集初始化
 */
static void ADC_Collection_Init(void) {
    // 创建二进制信号量
    adc_data_sem = osSemaphoreNew(1, 0, NULL);
    
    if (adc_data_sem == NULL) {
        Error_Handler();
    }

    // 设置采样率
    ADC_Set_Sample_Rate(500000);  // 500kHz

    // 启动ADC的DMA传输
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_double_buffer, ADC_BUFFER_SIZE * 2);

    
    // 启动TIM2触发ADC采样
    HAL_TIM_Base_Start(&htim2);
}

/**
 * @brief  ADC采集任务
 * @param  argument: 任务参数（未使用）
 */
void ADC_acquisition_task(void *argument) {
    ADC_Collection_Init();

    while (1) {
        // 阻塞等待信号量
        osStatus_t status = osSemaphoreAcquire(adc_data_sem, osWaitForever);

        if (status == osOK) {
            // 从全局指针获取已填满的缓冲区地址
            uint16_t *buffer_to_process = (uint16_t *)filled_buffer_ptr;

            // 分离双通道数据
            ADC_Separate_Channels(buffer_to_process, adc_ch1, adc_ch2);

            // 增加处理计数器
            count_buffer++;
        }
    }
}

/**
 * @brief  DMA传输半满回调 (当双缓冲区的前半部分填满时调用)
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    // 更新全局指针，指向已填满的前半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[0];
    
    // 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}

/**
 * @brief  DMA传输完成回调 (当双缓冲区的后半部分填满时调用)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    // 更新全局指针，指向已填满的后半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[ADC_BUFFER_SIZE];
    
    // 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}
