/**
 ****************************************************************************************************
 * @file        delay.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2021-10-14
 * @brief       使用SysTick的普通计数模式对延迟进行管理(支持ucosii)
 *              提供delay_init初始化函数， delay_us和delay_ms等延时函数
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211014
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/sys/sys.h"

extern TIM_HandleTypeDef htim7; // TIM7句柄，需在其他地方初始化

/* 如果SYS_SUPPORT_OS定义了,说明要支持OS了(不限于UCOS) */
#if SYS_SUPPORT_OS

/* 添加公共头文件 (FreeRTOS 需要用到) */
#include "FreeRTOS.h"
#include "task.h"

#endif

/**
 * @brief     初始化延迟函数
 * @param     sysclk: 系统时钟频率, 即CPU频率(rcc_c_ck), 168Mhz
 * @retval    无
 */
void delay_init(void)
{
    // __HAL_RCC_TIM7_CLK_ENABLE(); // 使能TIM7时钟

    // htim7.Instance = TIM7;
    // htim7.Init.Prescaler = 83; // 84MHz/84=1MHz，每1us计数一次
    // htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    // htim7.Init.Period = 0xFFFF; // 最大计数值，方便回绕
    // htim7.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    // htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    // HAL_TIM_Base_Init(&htim7);
    // HAL_TIM_Base_Start(&htim7); // 启动定时器
}

#if SYS_SUPPORT_OS // 使用FreeRTOS时

void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = htim7.Instance->ARR; // TIM7自动重载值

    ticks = nus;                          // 1us/计数，无需倍乘
    told = __HAL_TIM_GET_COUNTER(&htim7); // 刚进入时的计数器值
    while (1)
    {
        tnow = __HAL_TIM_GET_COUNTER(&htim7);
        if (tnow != told)
        {
            if (tnow > told)
            {
                tcnt += tnow - told; // 正常计数
            }
            else
            {
                tcnt += reload - told + tnow + 1; // 处理计数器回绕
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break; // 时间超过/等于要延迟的时间,则退出
            }
        }
    }
}

void delay_ms(uint16_t nms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        // 调度器未启动，使用忙等
        while (nms--)
        {
            delay_us(1000);
        }
    }
    else
    {
        vTaskDelay(nms); // 调度器已启动，使用FreeRTOS延时
    }
}

#else // 裸机模式

/**
 * @brief       延时nus
 * @param       nus: 要延时的us数.
 * @note        注意: nus的值,不要大于34952us(最大值即2^24 / g_fac_us @g_fac_us = 168)
 * @retval      无
 */
void delay_us(uint32_t nus)
{
    uint32_t start, now, cnt = 0;
    start = __HAL_TIM_GET_COUNTER(&htim7); // 记录初始计数值
    while (1)
    {
        now = __HAL_TIM_GET_COUNTER(&htim7);
        if (now != start)
        {
            if (now > start)
            {
                cnt += now - start;
            }
            else
            {
                cnt += (htim7.Instance->ARR + 1) - start + now; // 处理计数器回绕
            }
            start = now;
            if (cnt >= nus)
            {
                break; // 达到延时时间，退出
            }
        }
    }
}
/**
 * @brief       延时nms
 * @param       nms: 要延时的ms数 (0< nms <= 65535)
 * @retval      无
 */
void delay_ms(uint16_t nms)
{
    uint32_t repeat = nms / 30; // 每次最多延时30ms，防止溢出
    uint32_t remain = nms % 30;

    while (repeat)
    {
        delay_us(30 * 1000); // 利用delay_us实现30ms延时
        repeat--;
    }

    if (remain)
    {
        delay_us(remain * 1000); // 剩余部分延时
    }
}

#endif
