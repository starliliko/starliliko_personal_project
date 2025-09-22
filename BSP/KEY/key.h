/**
 ****************************************************************************************************
 * @file        key.h
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

#ifndef __KEY_H
#define __KEY_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/

#define KEY_UP HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin)
#define KEY_0 HAL_GPIO_ReadPin(KEY_0_GPIO_Port, KEY_0_Pin)

#define KEY_0_PRES 1  /* KEY0按下 */
#define KEY_UP_PRES 2 /* KEYUP按下 */

uint8_t key_scan(uint8_t mode); /* 按键扫描函数 */

#endif
