#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数

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

HAL_StatusTypeDef status;
//  采样率相关变量
float current_sample_rate = 1000000.0f; // 默认1MHz

// 函数前向声明
static void ADC_Collection_Init(void);

/**
 * @brief  设置采样率（通过调整TIM2周期）
 * @param  target_rate: 目标采样率 (Hz)
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef ADC_Set_Sample_Rate(float target_rate)
{
    if (target_rate <= 0 || target_rate > 2000000)
    { // 最大2MHz
        return HAL_ERROR;
    }

    // 获取TIM2时钟频率
    uint32_t tim2_clk = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clk_init;
    uint32_t flash_latency;
    HAL_RCC_GetClockConfig(&clk_init, &flash_latency);

    if (clk_init.APB1CLKDivider != RCC_HCLK_DIV1)
    {
        tim2_clk *= 2;  // APB1预分频≠1时，定时器时钟×2
    }

    //  计算最优的预分频器和周期值组合
    uint32_t best_prescaler = 0;
    uint32_t best_period = 0;
    float best_error = 1000000.0f;
    float actual_rate = 0;

    // 尝试不同的预分频器值，寻找最佳匹配
    for (uint32_t psc = 1; psc <= 65536 && psc <= tim2_clk; psc++)
    {
        uint32_t timer_freq = tim2_clk / psc;
        uint32_t period = timer_freq / (uint32_t)target_rate;
        
        // 检查周期值是否在有效范围内
        if (period == 0 || period > 0xFFFFFFFF) continue;
        
        // 计算实际采样率和误差
        float calc_rate = (float)timer_freq / period;
        float error = fabs(calc_rate - target_rate);
        
        // 找到更好的组合
        if (error < best_error)
        {
            best_error = error;
            best_prescaler = psc - 1;  // 寄存器值 = 实际值-1
            best_period = period - 1;  // 寄存器值 = 实际值-1
            actual_rate = calc_rate;
        }
        
        // 如果误差足够小，提前退出
        if (error < 0.1f) break;
    }

    // 检查是否找到有效配置
    if (best_error >= 1000000.0f)
    {
        return HAL_ERROR;
    }

    // 关键：停止ADC和定时器
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_TIM_Base_Stop(&htim2);
    
    // 短暂延时确保完全停止
    osDelay(10);

    //  更新TIM2配置
    htim2.Init.Prescaler = best_prescaler;
    htim2.Init.Period = best_period;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //  重新配置TIM2触发输出
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //  重新启动ADC DMA
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_double_buffer, ADC_BUFFER_SIZE * 2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //  重新启动定时器
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    // 更新当前采样率
    current_sample_rate = actual_rate;

    return HAL_OK;
}

/**
 * @brief  获取当前采样率
 * @return 当前采样率 (Hz)
 */
float ADC_Get_Sample_Rate(void)
{
    return current_sample_rate;
}

/**
 * @brief  获取采样间隔
 * @return 采样间隔 (秒)
 */
float ADC_Get_Sample_Interval(void)
{
    return 1.0f / current_sample_rate;
}

/**
 * @brief  分离交错存储的双通道ADC数据（优化版本）
 * @param  src: 源数据缓冲区
 * @param  ch1: 通道1输出缓冲区
 * @param  ch2: 通道2输出缓冲区
 */
static void ADC_Separate_Channels(uint16_t *src, uint16_t *ch1, uint16_t *ch2)
{
    if (!src || !ch1 || !ch2)
        return;

    // 使用指针直接访问，减少数组索引计算
    uint16_t *src_ptr = src;
    uint16_t *ch1_ptr = ch1;
    uint16_t *ch2_ptr = ch2;

    // 循环展开 4 次，减少循环开销
    for (uint16_t i = 0; i < CHANNEL_BUFFER_SIZE / 4; i++)
    {
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
    for (uint16_t i = remainder_start; i < CHANNEL_BUFFER_SIZE; i++)
    {
        ch1[i] = src[2 * i];
        ch2[i] = src[2 * i + 1];
    }
}

/**
 * @brief  ADC采集初始化
 */
static void ADC_Collection_Init(void)
{
    // 创建二进制信号量
    adc_data_sem = osSemaphoreNew(1, 0, NULL);

    if (adc_data_sem == NULL)
    {
        Error_Handler();
    }

    // 设置采样率
    status = ADC_Set_Sample_Rate(600000.0f); // 600kHz采样率

}

/**
 * @brief  ADC采集任务
 * @param  argument: 任务参数（未使用）
 */
void ADC_acquisition_task(void *argument)
{
    ADC_Collection_Init();

    while (1)
    {
        // 阻塞等待信号量
        osStatus_t status = osSemaphoreAcquire(adc_data_sem, osWaitForever);

        if (status == osOK)
        {
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
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    // 更新全局指针，指向已填满的前半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[0];

    // 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}

/**
 * @brief  DMA传输完成回调 (当双缓冲区的后半部分填满时调用)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    // 更新全局指针，指向已填满的后半个缓冲区
    filled_buffer_ptr = &adc_double_buffer[ADC_BUFFER_SIZE];

    // 释放信号量，唤醒处理任务
    osSemaphoreRelease(adc_data_sem);
}
