/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include <string.h>
#include "buzzer.h"
#include "ds18b20.h"
#include "dht11.h"
#include "pzem_drive.h"
#include "queue.h"   
#include "sensor_msg.h"
#include "usart.h"
#include "esp8266.h"
#include "state_led.h"
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
//在线状态标志位 (1=在线, 0=掉线)
static uint8_t mqtt_online_flag = 0; 
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
//打印队列
QueueHandle_t xQueueSensor;
QueueHandle_t xQueueDHT11;
QueueHandle_t xQueueDS18B20;
QueueHandle_t xQueuePZEM;
//串口打印
void PrintTask(void *argument);
void xUploadTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

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

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  //创建队列
	xQueueSensor = xQueueCreate(10, sizeof(SensorMsg_t));
	xQueueDHT11   = xQueueCreate(1, sizeof(SensorMsg_t));
	xQueueDS18B20 = xQueueCreate(1, sizeof(SensorMsg_t));
	xQueuePZEM    = xQueueCreate(1, sizeof(SensorMsg_t));

	  if(xQueueSensor == NULL || xQueueDHT11 == NULL || xQueueDS18B20 == NULL || xQueuePZEM == NULL) {
		  printf("Queues Creation Failed!\r\n");
	  } else {
		  printf("xQueuesSensor Create succeed!,size is : %d\r\n", sizeof(SensorMsg_t) * 10);
		  printf("xQueueDHT11 Create succeed!,size is : %d\r\n", sizeof(SensorMsg_t) * 1);
		  printf("xQueueDS18B20 Create succeed!,size is : %d\r\n", sizeof(SensorMsg_t) * 1);
		  printf("xQueuePZEM Create succeed!,size is : %d\r\n", sizeof(SensorMsg_t) * 1);
	  }
	printf("\r\n");
	//==============================================================================================//
	//dht11任务
	xTaskCreate(dht11_task, "dht11Task", 128, NULL, osPriorityAboveNormal, NULL);
	//ds18b20任务
	xTaskCreate(ds18b20_task, "ds18b20Task", 128, NULL, osPriorityAboveNormal, NULL);
	//pzem004t任务
	xTaskCreate(pzem004t_task, "pzem004tTask", 128, NULL, osPriorityAboveNormal, NULL);
	//打印任务
	xTaskCreate(PrintTask, "PrintTask", 256, NULL, osPriorityNormal, NULL);
	//esp8266任务
	xTaskCreate(xUploadTask, "esp8266Task", 512, NULL, osPriorityNormal, NULL);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	
  /* Infinite loop */
  for(;;)
  {
	  
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void PrintTask(void *argument)
{
    SensorMsg_t msg;

    printf("PrintTask Ready.\r\n");
    
    while(1)
    {
        if(((xQueueReceive(xQueueSensor, &msg, portMAX_DELAY) == pdPASS)) && (mqtt_online_flag == 1))
        {
            switch(msg.source)
            {
                case SRC_DHT11:
                    printf("[DHT11]   Temp: %.1f C, Humi: %.1f %%\r\n", 
                           msg.data.dht11.temp, msg.data.dht11.humi);
                    break;

                case SRC_DS18B20:
                    printf("[DS18B20] Temp: %.2f C\r\n", 
                           msg.data.ds18b20_temp);
                    break;

                case SRC_PZEM:
                    printf("=== PZEM-004T ===\r\n");
                    printf(" Volt  : %.1f V\r\n", msg.data.pzem.voltage);
                    printf(" Curr  : %.3f A\r\n", msg.data.pzem.current);
                    printf(" Power : %.1f W\r\n", msg.data.pzem.power);
                    printf(" Energy: %.0f Wh\r\n", msg.data.pzem.energy);
                    printf(" Freq  : %.1f Hz\r\n", msg.data.pzem.frequency);
                    printf(" PF    : %.2f\r\n", msg.data.pzem.pf);
                    printf("=================\r\n");
                    break; 
            }
        }
    }
}

void xUploadTask(void *argument)
{
    SensorMsg_t msg;
    char params_json[256];
    uint8_t sensor_index = 0; 
    uint8_t data_found = 0;

    printf("Aliyun Upload Task Started.\r\n");
	printf("\r\n");

    for(;;)
    {
        // 检查连接状态
        if(mqtt_online_flag == 0)
        {
            printf("[Task] MQTT Disconnected. Trying to Reconnect...\r\n");
            
            // 尝试连接 (ESP_Init_MQTT 现在会返回 1 或 0)
            if(ESP_Init_MQTT() == 1) 
            {
                mqtt_online_flag = 1; // 标记为在线
                printf("[Task] Reconnect Success!\r\n");
            }
            else 
            {
                printf("[Task] Reconnect Failed. Retry in 5s...\r\n");
                vTaskDelay(pdMS_TO_TICKS(5000));
				
                continue;
            }
        }

        data_found = 0;
        memset(params_json, 0, sizeof(params_json));

        switch(sensor_index)
        {
            case 0: // PZEM
                if(xQueueReceive(xQueuePZEM, &msg, 0) == pdPASS) {
                    printf("[Upload] Processing PZEM...\r\n");
                    printf("\r\n");
                    sprintf(params_json, "{\"Voltage\":%.1f,\"Current\":%.3f,\"Power\":%.1f,\"Energy\":%.0f}", 
                            msg.data.pzem.voltage, msg.data.pzem.current, msg.data.pzem.power, msg.data.pzem.energy);
                    data_found = 1;
                }
                break;

            case 1: // DHT11
                if(xQueueReceive(xQueueDHT11, &msg, 0) == pdPASS) {
                    printf("[Upload] Processing DHT11...\r\n");
                    sprintf(params_json, "{\"Temperature\":%.1f,\"Humidity\":%.1f}", 
                            msg.data.dht11.temp, msg.data.dht11.humi);
                    data_found = 1;
                }
                break;

            case 2: // DS18B20
                if(xQueueReceive(xQueueDS18B20, &msg, 0) == pdPASS) {
                    printf("[Upload] Processing DS18B20...\r\n");
                    sprintf(params_json, "{\"WaterTemp\":%.2f}", 
                            msg.data.ds18b20_temp);
                    data_found = 1;
                }
                break;
        }
        // 发送数据与错误检测
        if(data_found && strlen(params_json) > 0)
        {
            // 尝试发送，并获取结果
            uint8_t send_result = ESP_MQTT_Publish(params_json);
            
            if(send_result == 1)
            {
                vTaskDelay(pdMS_TO_TICKS(250)); 
				mqttSend_normal();
            }
            else
            {
                // 发送失败，判定为断线
                printf("[Task] Send Failed! Triggering Reconnect...\r\n");
                mqtt_online_flag = 0; // 标记掉线，下次循环自动重连
				mqttSend_abnormal();
            }
        }
        // 索引切换
        sensor_index++;
        if(sensor_index > 2) sensor_index = 0; 

        if(!data_found) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
/* USER CODE END Application */


