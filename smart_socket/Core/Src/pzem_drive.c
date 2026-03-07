#include "pzem_drive.h"
#include <string.h>
#include <stdio.h>
#include "usart.h" 
#include "queue.h"
#include "sensor_msg.h"

extern UART_HandleTypeDef huart3;

extern QueueHandle_t xQueueSensor;
extern QueueHandle_t xQueuePZEM;

extern void MX_USART3_UART_Init(void);

UART_HandleTypeDef *pzem_uart_handle;
SemaphoreHandle_t pzem_rx_sem = NULL; 
static uint8_t rx_buffer[25];                

PZEM_Data_t electric_data;

// PZEM 读取指令
static const uint8_t PZEM_CMD_READ_ALL[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D};

void PZEM_Init(UART_HandleTypeDef *huart) {
    pzem_uart_handle = huart;
    if(pzem_rx_sem == NULL) {
        pzem_rx_sem = xSemaphoreCreateBinary();
    }
}

uint8_t PZEM_Read_Data_IT(PZEM_Data_t *pData)
{
    xSemaphoreTake(pzem_rx_sem, 0);

    // 启动接收
    if(HAL_UART_Receive_IT(pzem_uart_handle, rx_buffer, 25) != HAL_OK) {
        // 如果启动失败（比如串口忙或锁死），返回0
        return 0; 
    }

    // 发送指令
    HAL_UART_Transmit(pzem_uart_handle, (uint8_t *)PZEM_CMD_READ_ALL, sizeof(PZEM_CMD_READ_ALL), 100);

    // 等待响应
    if(xSemaphoreTake(pzem_rx_sem, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        // 校验头
        if (rx_buffer[0] != 0x01 || rx_buffer[1] != 0x04) return 0; 

        // 解析数据 (已简化，不含报警)
        pData->voltage = ((rx_buffer[3] << 8) | rx_buffer[4]) * 0.1f;
        
        uint32_t curr = ((rx_buffer[7] << 8) | rx_buffer[8]) << 16 | 
                        ((rx_buffer[5] << 8) | rx_buffer[6]);
        pData->current = curr * 0.001f;
        
        uint32_t pow = ((rx_buffer[11] << 8) | rx_buffer[12]) << 16 | 
                       ((rx_buffer[9] << 8) | rx_buffer[10]);
        pData->power = pow * 0.1f;
        
        uint32_t nrg = ((rx_buffer[15] << 8) | rx_buffer[16]) << 16 | 
                       ((rx_buffer[13] << 8) | rx_buffer[14]);
        pData->energy = (float)nrg;

        pData->frequency = ((rx_buffer[17] << 8) | rx_buffer[18]) * 0.1f;
        pData->pf = ((rx_buffer[19] << 8) | rx_buffer[20]) * 0.01f;
        
        return 1; // 成功
    }
    else
    {
        // 超时处理：终止接收，并强制清除溢出标志（防止死锁）
        HAL_UART_AbortReceive(pzem_uart_handle); 
        __HAL_UART_CLEAR_OREFLAG(pzem_uart_handle); 
        return 0;
    }
}

void pzem004t_task(void *argument)
{
    SensorMsg_t msg;
    msg.source = SRC_PZEM;
    
    // 连续失败计数器
    uint8_t continuous_fail_count = 0;

    PZEM_Init(&huart3); 
    vTaskDelay(pdMS_TO_TICKS(1000));
	
    printf("PZEM Task Started!\r\n");
	printf("\r\n");

    while(1)
    {
        // 尝试读取（含内部重试）
        if(PZEM_Read_Data_IT(&electric_data))
        {
            // === 读取成功 ===
            continuous_fail_count = 0; // 清零失败计数

            msg.data.pzem.voltage   = electric_data.voltage;
            msg.data.pzem.current   = electric_data.current;
            msg.data.pzem.power     = electric_data.power;
            msg.data.pzem.energy    = electric_data.energy;
            msg.data.pzem.frequency = electric_data.frequency;
            msg.data.pzem.pf        = electric_data.pf;

            if(xQueueSensor != NULL) {
                xQueueSend(xQueueSensor, &msg, pdMS_TO_TICKS(10));
            }
			if(xQueuePZEM != NULL) {
				xQueueOverwrite(xQueuePZEM, &msg);
			}
        }
        else
        {
            // === 读取失败 ===
            continuous_fail_count++;
            
            // 如果连续失败超过 5 次（约 5-6 秒无数据）
            if(continuous_fail_count >= 5)
            {
                printf("PZEM: Connection Lost! Resetting UART...\r\n");
                
                // 1. 彻底关闭串口
                HAL_UART_DeInit(&huart3);
                
                // 2. 重新初始化串口
                MX_USART3_UART_Init();
                PZEM_Init(&huart3);
                
                // 3. 重置计数器
                continuous_fail_count = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
