#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"

// ==========================================
// 1. WiFi 配置
// ==========================================
#define WIFI_SSID       "Minerva"       
#define WIFI_PASS       "ykf040505"     

// ==========================================
// 2. 自建 MQTT 服务器信息
// ==========================================
// 阿里云公网 IP
#define MQTT_BROKER     "8.137.168.209"  
#define MQTT_PORT       1883

// MQTT 认证信息
#define MQTT_CLIENT_ID  "stm32_device_001"  // 客户端ID
#define MQTT_USER       "admin"         // 用户名
#define MQTT_PASS       "123"            //  密码

// ==========================================
// 3. Topic 配置
// ==========================================
// 可以自定义Topic
#define TOPIC_PUB       "home/sensor/data"  // 上传数据的Topic
#define TOPIC_SUB       "home/sensor/cmd"   // 接收命令的Topic

// 函数声明
uint8_t ESP_Init_MQTT(void); 
uint8_t ESP_MQTT_Publish(char *json_data);
uint8_t ESP_Send_Packet(uint8_t *data, uint16_t len);
void ESP_Process_Rx_Data(void);

#endif
