#include "ds18b20.h"
#include "sensor_msg.h"
#include "queue.h"

extern QueueHandle_t xQueueSensor;
extern QueueHandle_t xQueueDS18B20;

static float temp = 0;

#define CPU_FREQUENCY_MHZ (int)(HAL_RCC_GetHCLKFreq() / 1000000)

void DS18B20_Delay(__IO uint32_t delay)
{
    int last, curr, val;
    int temp;

    while (delay != 0)
    {
        temp = delay > 900 ? 900 : delay;
        last = SysTick->VAL;
        curr = last - CPU_FREQUENCY_MHZ * temp;
        if (curr >= 0)
        {
            do
            {
                val = SysTick->VAL;
            } while ((val < last) && (val >= curr));
        }
        else
        {
            curr += CPU_FREQUENCY_MHZ * 1000;
            do
            {
                val = SysTick->VAL;
            } while ((val <= last) || (val > curr));
        }
        delay -= temp;
    }
}

/**
 * 函数功能: 使DS18B20-DATA引脚变为上拉输入模式
 */
static void DS18B20_Mode_IPU(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DS18b20_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DS18b20_GPIO_Port, &GPIO_InitStruct);
}

/**
 * 函数功能: 使DS18B20-DATA引脚变为推挽输出模式
 */
static void DS18B20_Mode_Out_PP(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DS18b20_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS18b20_GPIO_Port, &GPIO_InitStruct);
}

static void DS18B20_Rst(void)
{
    DS18B20_Mode_Out_PP(); 
    DS18B20_Dout_LOW();    
    DS18B20_Delay(750);    
    DS18B20_Dout_HIGH();   
    DS18B20_Delay(15);     
}

/**
 * 函数功能: 检测从机给主机返回的存在脉冲
 */
static uint8_t DS18B20_Presence(void)
{
    uint8_t pulse_time = 0;
    DS18B20_Mode_IPU();

    while (DS18B20_Data_IN() && pulse_time < 100)
    {
        pulse_time++;
        DS18B20_Delay(1);
    }
    if (pulse_time >= 100)
        return 1;
    else
        pulse_time = 0;

    while (!DS18B20_Data_IN() && pulse_time < 240)
    {
        pulse_time++;
        DS18B20_Delay(1);
    }
    if (pulse_time >= 240)
        return 1;
    else
        return 0;
}

/**
 * 函数功能: 从DS18B20读取一个bit
 * FreeRTOS修改: 加入临界区保护
 */
static uint8_t DS18B20_ReadBit(void)
{
    uint8_t dat;
    
    taskENTER_CRITICAL();

    DS18B20_Mode_Out_PP();
    DS18B20_Dout_LOW();
    DS18B20_Delay(2); // 拉低 >1us

    DS18B20_Mode_IPU(); // 释放总线
    // 延时等待采样，DS18B20会在拉低后15us内输出数据有效
    // 这里的延时非常关键，一般拉低后到采样点控制在10us左右
    DS18B20_Delay(10); 

    if (DS18B20_Data_IN() == GPIO_PIN_SET)
        dat = 1;
    else
        dat = 0;
    
    taskEXIT_CRITICAL();

    /* 补齐时隙时间，这里不需要关中断，被打断也没关系，只要保证下一次操作前间隔足够 */
    DS18B20_Delay(50);

    return dat;
}

/**
 * 函数功能: 从DS18B20读一个字节
 */
static uint8_t DS18B20_ReadByte(void)
{
    uint8_t i, j, dat = 0;
    for (i = 0; i < 8; i++)
    {
        j = DS18B20_ReadBit(); // ReadBit 内部已经处理了临界区
        dat = (dat) | (j << i);
    }
    return dat;
}

/**
 * 函数功能: 写一个字节到DS18B20
 * FreeRTOS修改: 加入临界区保护
 */
static void DS18B20_WriteByte(uint8_t dat)
{
    uint8_t i, testb;
    DS18B20_Mode_Out_PP();
    
    for (i = 0; i < 8; i++)
    {
        testb = dat & 0x01;
        dat = dat >> 1;
        
        if (testb) // 写 1
        {
            taskENTER_CRITICAL();
            
            DS18B20_Dout_LOW();
            DS18B20_Delay(2); // 拉低 >1us
            DS18B20_Dout_HIGH();
            
            taskEXIT_CRITICAL();
            
            DS18B20_Delay(60); // 写1的后续延时不需要保护
        }
        else // 写 0
        {
            taskENTER_CRITICAL();
            
            DS18B20_Dout_LOW();
            DS18B20_Delay(65); // 拉低 60~120us
            DS18B20_Dout_HIGH();
            
            taskEXIT_CRITICAL();
            
            DS18B20_Delay(2); // 恢复时间
        }
    }
}

static void DS18B20_SkipRom(void)
{
    DS18B20_Rst();
    DS18B20_Presence();
    DS18B20_WriteByte(0XCC);
}

/**
 * 函数功能: 获取 DS18B20 温度值
 * FreeRTOS修改: 增加 vTaskDelay 释放CPU
 */
float DS18B20_GetTemp_SkipRom(void)
{
    uint8_t tpmsb, tplsb;
    short s_tem;
    float f_tem;

    /* 1. 启动转换 */
    DS18B20_SkipRom();
    DS18B20_WriteByte(0X44); /* 开始转换 */

    vTaskDelay(pdMS_TO_TICKS(750)); 

    /* 2. 读取数据 */
    DS18B20_SkipRom();
    DS18B20_WriteByte(0XBE); /* 读温度值 */

    tplsb = DS18B20_ReadByte();
    tpmsb = DS18B20_ReadByte();

    s_tem = tpmsb << 8;
    s_tem = s_tem | tplsb;

    if (s_tem < 0)
        f_tem = (~s_tem + 1) * 0.0625;
    else
        f_tem = s_tem * 0.0625;

    return f_tem;
}

/**
 * 函数功能: DS18B20 初始化函数
 */
uint8_t DS18B20_Init(void)
{
    DS18B20_Mode_Out_PP();
    DS18B20_Dout_HIGH();
    DS18B20_Rst();
    return DS18B20_Presence();
}

void ds18b20_task(void *argument)
{	
    SensorMsg_t msg;
    msg.source = SRC_DS18B20;
	
	DS18B20_Init();
	printf("DS18B20 Task Started!\r\n");

	while(1)
	{
		temp = DS18B20_GetTemp_SkipRom();
        
        if(temp > -50 && temp < 150) 
        {
            msg.data.ds18b20_temp = temp;
            
            // 1. 发送到打印队列
            if(xQueueSensor != NULL) {
                xQueueSend(xQueueSensor, &msg, 0);
            }
			if(xQueueDS18B20 != NULL) {
                xQueueOverwrite(xQueueDS18B20, &msg);
            }
        }

		vTaskDelay(pdMS_TO_TICKS(1500));
	}
}
