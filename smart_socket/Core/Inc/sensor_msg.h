#ifndef __SENSOR_MSG_H
#define __SENSOR_MSG_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef enum {
    SRC_DHT11,
    SRC_DS18B20,
    SRC_PZEM
} SensorSource_t;

typedef struct {
	//数据标签
    SensorSource_t source;
    
    union {
        // DHT11 数据
        struct {
            float temp;
            float humi;
        } dht11;

        // DS18B20 数据
        float ds18b20_temp;

        struct {
            float voltage;      // 电压 (V)
            float current;      // 电流 (A)
            float power;        // 功率 (W)
            float energy;       // 电能 (Wh)
            float frequency;    // 频率 (Hz)
            float pf;           // 功率因数
        } pzem;
    } data;
} SensorMsg_t;
//多车道轮询实现数据均匀传递
extern QueueHandle_t xQueueDHT11;
extern QueueHandle_t xQueueDS18B20;
extern QueueHandle_t xQueuePZEM;

#endif
