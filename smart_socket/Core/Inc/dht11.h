#ifndef _DHT11_H_
#define _DHT11_H_

#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#define u8 unsigned char 
#define u16 unsigned short
#define u32 unsigned int
	
#define DATA_GPIO_Port GPIOB
#define DATA_Pin GPIO_PIN_3

// GPIO操作宏保持不变
#define DATA_SET() HAL_GPIO_WritePin(DATA_GPIO_Port, DATA_Pin, GPIO_PIN_SET)
#define DATA_RESET() HAL_GPIO_WritePin(DATA_GPIO_Port, DATA_Pin, GPIO_PIN_RESET)
#define DATA_READ() HAL_GPIO_ReadPin(DATA_GPIO_Port,DATA_Pin)

typedef struct
{
  u8 Data[5];    // 数据存放数组
  u8 index;
  float temp;       // 温度
  float humidity;   // 湿度
} DH11_DATA;

extern DH11_DATA DH11_data;

// 修改为 FreeRTOS 任务函数标准形式
void dht11_task(void *argument);

#endif
