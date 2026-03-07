#ifndef __PZEM_DRIVE_H
#define __PZEM_DRIVE_H

#include "main.h"
#include "cmsis_os.h" // 包含 FreeRTOS 定义
#include "semphr.h"   // 包含信号量定义

typedef struct {
    float voltage;
    float current;
    float power;
    float energy;
    float frequency;
    float pf;
    uint16_t alarms;
} PZEM_Data_t;

extern SemaphoreHandle_t pzem_rx_sem;

// 初始化函数
void PZEM_Init(UART_HandleTypeDef *huart);
// 这里的读取函数不再需要返回 uint8_t，因为流程由信号量控制，解析在内部完成
uint8_t PZEM_Read_Data_IT(PZEM_Data_t *pData);
void pzem004t_task(void *argument);

#endif
