/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for LEDTask */
osThreadId_t LEDTaskHandle;
const osThreadAttr_t LEDTask_attributes = {
  .name = "LEDTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LVGL_Task */
osThreadId_t LVGL_TaskHandle;
const osThreadAttr_t LVGL_Task_attributes = {
  .name = "LVGL_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for ADCTask */
osThreadId_t ADCTaskHandle;
const osThreadAttr_t ADCTask_attributes = {
  .name = "ADCTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for Waveform_Task */
osThreadId_t Waveform_TaskHandle;
const osThreadAttr_t Waveform_Task_attributes = {
  .name = "Waveform_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for Waveform_Feature_Queue */
osMessageQueueId_t Waveform_Feature_QueueHandle;
const osMessageQueueAttr_t Waveform_Feature_Queue_attributes = {
  .name = "Waveform_Feature_Queue"
};
/* Definitions for ADC_Raw_Data_Queue */
osMessageQueueId_t ADC_Raw_Data_QueueHandle;
const osMessageQueueAttr_t ADC_Raw_Data_Queue_attributes = {
  .name = "ADC_Raw_Data_Queue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void LEDtestTask(void *argument);
void lvgl_task(void *argument);
void ADC_acquisition_task(void *argument);
void waveform_process_task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of Waveform_Feature_Queue */
  Waveform_Feature_QueueHandle = osMessageQueueNew (3, 22, &Waveform_Feature_Queue_attributes);

  /* creation of ADC_Raw_Data_Queue */
  ADC_Raw_Data_QueueHandle = osMessageQueueNew (5, 4, &ADC_Raw_Data_Queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LEDTask */
  LEDTaskHandle = osThreadNew(LEDtestTask, NULL, &LEDTask_attributes);

  /* creation of LVGL_Task */
  LVGL_TaskHandle = osThreadNew(lvgl_task, NULL, &LVGL_Task_attributes);

  /* creation of ADCTask */
  ADCTaskHandle = osThreadNew(ADC_acquisition_task, NULL, &ADCTask_attributes);

  /* creation of Waveform_Task */
  Waveform_TaskHandle = osThreadNew(waveform_process_task, NULL, &Waveform_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_LEDtestTask */
/**
 * @brief  Function implementing the LEDTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_LEDtestTask */
void LEDtestTask(void *argument)
{
  /* USER CODE BEGIN LEDtestTask */
    /* Infinite loop */
    for (;;)
    {
        HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
        osDelay(200);
    }
  /* USER CODE END LEDtestTask */
}

/* USER CODE BEGIN Header_lvgl_task */
/**
* @brief Function implementing the LVGL_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_lvgl_task */
__weak void lvgl_task(void *argument)
{
  /* USER CODE BEGIN lvgl_task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END lvgl_task */
}

/* USER CODE BEGIN Header_ADC_acquisition_task */
/**
* @brief Function implementing the ADCTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ADC_acquisition_task */
__weak void ADC_acquisition_task(void *argument)
{
  /* USER CODE BEGIN ADC_acquisition_task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END ADC_acquisition_task */
}

/* USER CODE BEGIN Header_waveform_process_task */
/**
* @brief Function implementing the Waveform_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_waveform_process_task */
__weak void waveform_process_task(void *argument)
{
  /* USER CODE BEGIN waveform_process_task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END waveform_process_task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

