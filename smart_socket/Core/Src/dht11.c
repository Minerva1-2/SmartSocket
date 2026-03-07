#include "dht11.h"
#include "sensor_msg.h"
#include "queue.h"
#include <stdio.h> // 用于 printf

extern QueueHandle_t xQueueSensor;
extern QueueHandle_t xQueueDHT11;

DH11_DATA DH11_data;

// ==========================================
// 1. 改用 DWT 实现高精度微秒延时 (替换原有的定时器方案)
// ==========================================
static uint32_t dht_fac_us = 0;

// DWT 初始化 (通常在 main 或 dht11 任务开头调用一次)
void DHT11_DWT_Init(void)
{
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    dht_fac_us = SystemCoreClock / 1000000;
}

// 纯寄存器操作，无函数调用开销
void Delay_us(uint32_t us)
{
    uint32_t start_tick = DWT->CYCCNT;
    uint32_t delay_ticks = us * dht_fac_us;
    while ((DWT->CYCCNT - start_tick) < delay_ticks);
}

// ==========================================
// 2. GPIO 操作函数 (保持不变)
// ==========================================
static void DATA_OUTPUT(u8 flg)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DATA_GPIO_Port, &GPIO_InitStruct);

    if(flg==0) DATA_RESET();
    else DATA_SET();
}

static u8 DATA_INPUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    u8 flg=0;
    GPIO_InitStruct.Pin = DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DATA_GPIO_Port, &GPIO_InitStruct);

    if(DATA_READ()==GPIO_PIN_RESET) flg=0;
    else flg=1;
    return flg;
}

// ==========================================
// 3. 读取逻辑 (保持不变，依赖精准的 Delay_us)
// ==========================================
static u8 DH11_Read_Byte(void)
{
    u8 ReadDat=0;
    u8 temp=0;
    u8 retry=0;
    u8 i=0;
    
    for(i=0;i<8;i++)
    {
        // 等待高电平到来
        while(DATA_READ()==0 && retry<100)
        {
            Delay_us(1);
            retry++;
        }
        retry=0;
        
        // 延时 40us 后采样
        // 0信号高电平26us -> 40us时已为低 -> 读到0
        // 1信号高电平70us -> 40us时仍为高 -> 读到1
        Delay_us(30); 
        
        if(DATA_READ()==1) temp=1;
        else temp=0;
        
        // 等待低电平 (准备下一位)
        while(DATA_READ()==1 && retry<100)
        {
            Delay_us(1);
            retry++;
        }
        retry=0;
        
        ReadDat<<=1;
        ReadDat|=temp;
    }
    return ReadDat;
}

u8 DH11_Read(void)
{
    u8 retry=0;
    u8 i=0;
  
    DATA_OUTPUT(0);
    vTaskDelay(pdMS_TO_TICKS(20)); // RTOS 延时 20ms
    DATA_SET();
    Delay_us(30);
  
    DATA_INPUT();
  
    // 关中断保护时序
    taskENTER_CRITICAL(); 

    if(DATA_READ()==0)
    {
        // 等待响应拉高
        while(DATA_READ()==0 && retry<100) { Delay_us(1); retry++; }
        retry=0;
        // 等待响应拉低
        while(DATA_READ()==1 && retry<100) { Delay_us(1); retry++; }
        retry=0;
        
        // 读取 40bit
        for(i=0;i<5;i++)
        {
            DH11_data.Data[i] = DH11_Read_Byte();
        }
        Delay_us(50);
    }
  
    taskEXIT_CRITICAL(); // 开中断

    // 校验
    u32 sum = DH11_data.Data[0] + DH11_data.Data[1] + DH11_data.Data[2] + DH11_data.Data[3];
    if((u8)sum == DH11_data.Data[4])
    {
        // 计算物理值
        DH11_data.humidity = (float)DH11_data.Data[0] + ((float)DH11_data.Data[1] * 0.1f);
        DH11_data.temp     = (float)DH11_data.Data[2] + ((float)DH11_data.Data[3] * 0.1f);
        return 1; 
    }
    else
    {
        return 0; 
    }
}

void dht11_task(void *argument)
{
    SensorMsg_t msg;
    msg.source = SRC_DHT11;
    
    DHT11_DWT_Init();
    DATA_OUTPUT(1);
    printf("DHT11 Task Started!\r\n");

    while(1)
    {
        if(DH11_Read()) 
        {
            if(DH11_data.temp != 0 || DH11_data.humidity != 0) 
            {
                msg.data.dht11.temp = DH11_data.temp;
                msg.data.dht11.humi = DH11_data.humidity;

                // 1. 发送到打印队列 (保持原样)
                if(xQueueSensor != NULL) {
                    xQueueSend(xQueueSensor, &msg, 0); // 0等待
                }

                // 2. 【修改】发送到上传专用队列，使用覆盖写
                if(xQueueDHT11 != NULL) {
                    xQueueOverwrite(xQueueDHT11, &msg); 
                }
            }
            else
            {
                printf("DHT11 Zero Reading\r\n");
            }
        }
        else 
        {
            printf("DHT11 Read Failed\r\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}
