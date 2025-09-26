#include "lvgl_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lv_demo_stress.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"

#include "oscilloscope.h"
void lvgl_init(void)
{
    lv_init();            /* lvglϵͳ��ʼ�� */
    lv_port_disp_init();  /* lvgl��ʾ�ӿڳ�ʼ��,����lv_init()�ĺ��� */
    lv_port_indev_init(); /* lvgl����ӿڳ�ʼ��,����lv_init()�ĺ��� */
}

void lvgl_task(void *argument)
{
    lvgl_init();      /* lvgl初始化 */
    //lv_demo_stress(); /* ���Ե�demo */
    create_oscilloscope_ui();
    while (1)
    {
        lv_timer_handler(); /* LVGL��ʱ�� */
        vTaskDelay(5);
    }
}