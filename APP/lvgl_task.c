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
    lv_init();            /* lvgl??????? */
    lv_port_disp_init();  /* lvgl??????????,????lv_init()????? */
    lv_port_indev_init(); /* lvgl??????????,????lv_init()????? */
}

void lvgl_task(void *argument)
{
    lvgl_init();      /* lvgl≥ı ºªØ */
    //lv_demo_stress(); /* ?????demo */
    create_oscilloscope_ui();
    while (1)
    {
        lv_timer_handler(); /* LVGL????? */
        vTaskDelay(5);
    }
}