#include "adc_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "adc.h"
#include "dma.h"
#include "tim.h"

// 硬件配置参数
#define ADC_BUFFER_SIZE     1024    // 总采样点数（双通道各512点）
#define CHANNEL_BUFFER_SIZE (ADC_BUFFER_SIZE / 2)  // 单通道采样点数

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

// 函数前向声明
void ADC_Collection_Init(void);
static inline void ADC_DMA_Stop(void);
static inline uint8_t ADC_DMA_Start(uint16_t *buf, uint16_t len);

/**
 * @brief  分离交错存储的双通道ADC数据（保持原有逻辑）
 */
void ADC_Separate_Channels(uint16_t *src, uint16_t *ch1, uint16_t *ch2) {
    if (!src || !ch1 || !ch2) return;
    for (uint16_t i = 0; i < CHANNEL_BUFFER_SIZE; i++) {
        ch1[i] = src[2 * i];
        ch2[i] = src[2 * i + 1];
    }
}

/**
 * @brief  寄存器级操作：停止ADC DMA传输（替代HAL_ADC_Stop_DMA）
 */
static inline void ADC_DMA_Stop(void) {
    // 1. 关闭DMA流（使用DMA2_Stream4）
    DMA2_Stream4->CR &= ~DMA_SxCR_EN;
    // 2. 等待DMA流确认关闭
    while ((DMA2_Stream4->CR & DMA_SxCR_EN) != 0);

    // 3. 清除DMA所有相关标志（避免残留中断）
    // DMA2 Stream4 对应 HIFCR 寄存器
    DMA2->HIFCR = DMA_HIFCR_CTCIF4 | DMA_HIFCR_CHTIF4 | DMA_HIFCR_CTEIF4 |
                  DMA_HIFCR_CDMEIF4 | DMA_HIFCR_CFEIF4;

    // 4. 关闭ADC模块
    ADC1->CR2 &= ~ADC_CR2_ADON;
    // 5. 等待ADC确认关闭
    while ((ADC1->CR2 & ADC_CR2_ADON) != 0);
}

/**
 * @brief  寄存器级操作：启动ADC DMA传输（替代HAL_ADC_Start_DMA）
 * @return 0-成功，1-失败
 */
static inline uint8_t ADC_DMA_Start(uint16_t *buf, uint16_t len) {
    // // 1. 确保ADC和定时器时钟已使能 (假设TIM2)
    // if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) == 0 ||
    //     (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) == 0) {
    //     return 1;  // 时钟未使能
    // }

    // 2. 清除ADC所有状态标志（特别是OVR标志）
    ADC1->SR = 0;

    // 3. 配置DMA（确保DMA已关闭，使用DMA2_Stream4）
    // 在调用Stop后，DMA应已关闭，此处为双重保险
    while ((DMA2_Stream4->CR & DMA_SxCR_EN) != 0);
    DMA2_Stream4->M0AR = (uint32_t)buf;  // 内存地址
    DMA2_Stream4->NDTR = len;            // 传输长度

    // 使能DMA传输完成中断
    DMA2_Stream4->CR |= DMA_SxCR_TCIE;

    // 4. 使能DMA流
    DMA2_Stream4->CR |= DMA_SxCR_EN;

    // 5. 使能ADC，准备接收外部触发
    ADC1->CR2 |= ADC_CR2_ADON;

    // // 6. 验证ADC是否配置为外部触发模式
    // if ((ADC1->CR2 & ADC_CR2_EXTEN) == 0) {
    //     return 1;  // 未使能外部触发（CubeMX配置错误）
    // }

    // // 7. 验证定时器已启动（TRGO信号源有效）
    // if ((TIM2->CR1 & TIM_CR1_CEN) == 0) {
    //     return 1;  // 定时器未启动，无触发信号
    // }

    return 0;  // 启动成功（等待定时器触发）
}

/**
 * @brief  ADC采集初始化
 */
void ADC_Collection_Init(void) {
    // 创建数据队列（保持原有逻辑）
    adc_data_queue = osMessageQueueNew(5, sizeof(uint16_t*), NULL);

    // 使用HAL函数建立一次链接 
    // 目的：让HAL库设置好DMA句柄和ADC句柄之间的回调链接。
    // 我们只调用一次，然后立即停止，后续的循环控制由我们自己的函数完成。
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)current_buffer, ADC_BUFFER_SIZE) != HAL_OK)
    {
        Error_Handler();
    }

    // 用寄存器操作替代HAL库启动DMA
    ADC_DMA_Stop();  // 停止可能的残留传输
    if (ADC_DMA_Start(current_buffer, ADC_BUFFER_SIZE) != 0) {
        // 启动失败处理
        Error_Handler();
    }
}

/**
 * @brief  ADC采集任务（保持原有标志位轮询逻辑不变）
 */
void ADC_acquisition_task(void *argument) {
    uint16_t *filled_buffer;

    ADC_Collection_Init();

    while (1) {
        // 轮询等待数据就绪标志（保持原有逻辑）
        if (adc_data_ready) {
            // 临界区：获取已完成的缓冲区（禁用中断避免冲突）
            __disable_irq();
            filled_buffer = (current_buffer == adc_buffer_A) ? adc_buffer_B : adc_buffer_A;
            __enable_irq();

            // 分离数据（保持原有逻辑）
            ADC_Separate_Channels(filled_buffer, adc_ch1, adc_ch2);

            // 发送到队列（保持注释状态，与原代码一致）
            // osMessageQueuePut(adc_data_queue, &adc_ch1, 0, osWaitForever);
            // osMessageQueuePut(adc_data_queue, &adc_ch2, 0, osWaitForever);

            // 清除就绪标志（处理完成）
            adc_data_ready = 0;
        }

        // 短暂延迟，降低CPU占用（保持原有逻辑）
        osDelay(1);
    }
}

/**
 * @brief  DMA传输完成回调（仅替换DMA操作，其余逻辑不变）
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {

    // 锁定缓冲区切换（保持原有标志位锁）
    buffer_switch_lock = 1;

    // 切换缓冲区（保持原有逻辑）
    current_buffer = (current_buffer == adc_buffer_A) ? adc_buffer_B : adc_buffer_A;

    // 用寄存器操作替代HAL库的Stop/Start DMA
    ADC_DMA_Stop();
    // 修正：移除不正确的类型转换
    if (ADC_DMA_Start(current_buffer, ADC_BUFFER_SIZE) != 0) {
        Error_Handler();  // 保持原有错误处理
    }

    a++;  // 保持采样计数

    // 设置数据就绪标志（通知任务处理，保持原有逻辑）
    adc_data_ready = 1;

    // 解锁缓冲区切换（保持原有逻辑）
    buffer_switch_lock = 0;
}