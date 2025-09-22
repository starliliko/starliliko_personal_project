#ifndef LVGL_TASK_H
#define LVGL_TASK_H

#include <stdbool.h>
#include <stdint.h>

void lvgl_init(void);
void lvgl_task(void *pvParameters);

#endif // LVGL_TASK_H