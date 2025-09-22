#include "lvgl_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lv_demo_stress.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"

void lvgl_init(void)
{
    lv_init();            /* lvgl系统初始化 */
    lv_port_disp_init();  /* lvgl显示接口初始化,放在lv_init()的后面 */
    lv_port_indev_init(); /* lvgl输入接口初始化,放在lv_init()的后面 */
}

void lvgl_task(void *argument)
{
    lvgl_init();      /* lvgl初始化 */
    lv_demo_stress(); /* 测试的demo */

    while (1)
    {
        lv_timer_handler(); /* LVGL计时器 */
        vTaskDelay(5);
    }
}