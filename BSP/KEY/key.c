/**
 ****************************************************************************************************
 * @file        key.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2021-10-14
 * @brief       按键输入 驱动代码
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

#include "./BSP/KEY/key.h"
#include "./SYSTEM/delay/delay.h"

uint8_t key_scan(uint8_t mode)
{
    static uint8_t key_up = 1; /* 按键松开标志 */
    uint8_t keyval = 0;

    if (mode)
        key_up = 1; /* 支持连按 */

    if (key_up && (KEY_0 == 1 || KEY_UP == 1)) /* 有任意一个按键按下 */
    {
        delay_ms(10); /* 去抖动 */
        key_up = 0;

        // 没有优先级，哪个先检测到就返回哪个
        if (KEY_0 == 0)
            keyval = KEY_0_PRES;
        else if (KEY_UP == 1)
            keyval = KEY_UP_PRES;
    }
    else if (KEY_0 == 0 && KEY_UP == 0) /* 没有任何按键按下, 标记按键松开 */
    {
        key_up = 1;
    }

    return keyval; /* 返回键值 */
}
