#include "esp8266.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "state_led.h"
#include "buzzer.h"

extern UART_HandleTypeDef huart2;
extern uint8_t esp_rx_buf[];
extern uint16_t esp_rx_index;
extern uint8_t esp_byte_tmp;

// 全局标志：是否连接成功
volatile uint8_t mqtt_ready_flag = 0; 

SemaphoreHandle_t xESPMutex = NULL;

void ESP_Process_Rx_Data(void);

// ==========================================================
// 二进制内存字符串查找 (无视 0x00 中断)
// ==========================================================
char* ESP_Memstr(uint8_t* buf, uint16_t buf_len, const char* str) {
    uint16_t str_len = strlen(str);
    if (str_len == 0 || buf_len < str_len) return NULL;
    for (uint16_t i = 0; i <= buf_len - str_len; i++) {
        uint16_t j;
        for (j = 0; j < str_len; j++) {
            if (buf[i + j] != str[j]) break;
        }
        if (j == str_len) return (char*)&buf[i]; // 找到了！
    }
    return NULL; // 没找到
}

void ESP_Clear_Rx(void) {
    memset(esp_rx_buf, 0, 512);
    esp_rx_index = 0;
}

void ESP_Start_Receive(void) {
    HAL_UART_Receive_IT(&huart2, &esp_byte_tmp, 1);
}

// 发送 AT 指令并等待响应 
uint8_t ESP_Send_Cmd(char *cmd, char *ack, uint16_t timeout_ms) {
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    __HAL_UART_CLEAR_NEFLAG(&huart2);
    __HAL_UART_CLEAR_FEFLAG(&huart2);
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);

    // 🚨 升级防误杀：使用 ESP_Memstr，突破 \0 限制！
    if(ESP_Memstr(esp_rx_buf, esp_rx_index, "+IPD,") != NULL || ESP_Memstr(esp_rx_buf, esp_rx_index, "relay") != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100)); 
        ESP_Process_Rx_Data();          
    }

    ESP_Clear_Rx();
    HAL_UART_Receive_IT(&huart2, &esp_byte_tmp, 1);
    
    if (cmd != NULL) {
        HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 100);
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);
    }

    uint16_t time = 0;
    while(time < timeout_ms) {
        if(strstr((const char*)esp_rx_buf, ack) != NULL) return 1;
        
        // 🚨 同样在这里升级
        if(ESP_Memstr(esp_rx_buf, esp_rx_index, "+IPD,") != NULL || ESP_Memstr(esp_rx_buf, esp_rx_index, "relay") != NULL) {
            ESP_Process_Rx_Data();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        time += 50;
    }
    return 0;
}

// 返回 1: 发送成功
// 返回 0: 发送失败
uint8_t ESP_Send_Packet(uint8_t *data, uint16_t len) {
    uint8_t status = 0;

    if(xESPMutex == NULL) xESPMutex = xSemaphoreCreateMutex();

    if(xSemaphoreTake(xESPMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        char cmd[32];
        sprintf(cmd, "AT+CIPSEND=%d", len);
        
        if(ESP_Send_Cmd(cmd, ">", 2000)) { // 稍微加大超时到2s
            HAL_UART_Transmit(&huart2, data, len, 1000);
            vTaskDelay(pdMS_TO_TICKS(300)); // 等待发送完成
            status = 1; // 成功
        } else {
            printf("[ESP] Error: Wait '>' failed. Connection lost...\r\n");
            status = 0; // 失败
        }
        xSemaphoreGive(xESPMutex);
    } else {
        printf("[ESP] Busy: Packet Dropped\r\n");
        status = 0; // 获取锁失败也视为发送未完成
    }
    return status;
}

int MQTT_Encode_Length(uint8_t *buf, uint16_t length) {
    int i = 0;
    uint8_t digit;
    do {
        digit = length % 128;
        length = length / 128;
        if (length > 0) digit |= 0x80;
        buf[i++] = digit;
    } while (length > 0);
    return i;
}

// ===============================================
// 标准 MQTT 连接报文
// ===============================================
void MQTT_Connect(void) {
    static uint8_t tx_buf[512];
    
    uint16_t pos = 0;
    uint16_t id_len = strlen(MQTT_CLIENT_ID);
    uint16_t user_len = strlen(MQTT_USER);
    uint16_t pass_len = strlen(MQTT_PASS);
    
    // 1. 计算剩余长度
    // 协议头(10) + ClientID(2+len) + User(2+len) + Pass(2+len)
    uint16_t rem_len = 10 + (2 + id_len) + (2 + user_len) + (2 + pass_len);
    
    // 2. Fixed Header (0x10 = Connect)
    tx_buf[pos++] = 0x10;
    pos += MQTT_Encode_Length(&tx_buf[pos], rem_len);
    
    // 3. Variable Header (Protocol Level 4)
    // 0xC2 = User(Bit7) + Pass(Bit6) + CleanSession(Bit1)
    uint8_t var_header[] = {0x00,0x04,'M','Q','T','T',0x04,0xC2,0x00,0x78}; 
    memcpy(&tx_buf[pos], var_header, sizeof(var_header));
    pos += sizeof(var_header);
    
    // 4. Payload: Client ID
    tx_buf[pos++] = id_len >> 8; tx_buf[pos++] = id_len & 0xFF;
    memcpy(&tx_buf[pos], MQTT_CLIENT_ID, id_len); pos += id_len;
    
    // 5. Payload: Username
    tx_buf[pos++] = user_len >> 8; tx_buf[pos++] = user_len & 0xFF;
    memcpy(&tx_buf[pos], MQTT_USER, user_len); pos += user_len;
    
    // 6. Payload: Password
    tx_buf[pos++] = pass_len >> 8; tx_buf[pos++] = pass_len & 0xFF;
    memcpy(&tx_buf[pos], MQTT_PASS, pass_len); pos += pass_len;
    
    printf("[MQTT] Sending Connect Packet to Aliyun...\r\n");
    ESP_Send_Packet(tx_buf, pos);
}

// ===============================================
// 标准 MQTT 订阅报文 (Subscribe)
// ===============================================
uint8_t MQTT_Subscribe_Packet(char *topic, uint8_t qos) {
    static uint8_t tx_buf[512];
    uint16_t pos = 0;
    uint16_t topic_len = strlen(topic);
    
    // 剩余长度计算: 报文标识符(2) + 主题长度(2) + 主题字符串 + QoS(1)
    uint16_t rem_len = 2 + 2 + topic_len + 1;
    
    // 1. Fixed Header (0x82 = Subscribe 请求)
    tx_buf[pos++] = 0x82;
    pos += MQTT_Encode_Length(&tx_buf[pos], rem_len);
    
    // 2. Variable Header: 报文标识符 Packet ID (固定用 0x00 0x01)
    tx_buf[pos++] = 0x00; 
    tx_buf[pos++] = 0x01;
    
    // 3. Payload: Topic Length
    tx_buf[pos++] = topic_len >> 8; 
    tx_buf[pos++] = topic_len & 0xFF;
    
    // 4. Payload: Topic String
    memcpy(&tx_buf[pos], topic, topic_len); 
    pos += topic_len;
    
    // 5. Payload: Requested QoS
    tx_buf[pos++] = qos; 
    
    printf("[MQTT] Subscribing to: %s (QoS: %d)\r\n", topic, qos);
    
    return ESP_Send_Packet(tx_buf, pos);
}

uint8_t MQTT_Publish_Packet(char *topic, char *json) {
    static uint8_t tx_buf[1024];
    uint16_t pos = 0;
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(json);
    
    uint16_t rem_len = (2 + topic_len) + payload_len;
    
    tx_buf[pos++] = 0x30;
    pos += MQTT_Encode_Length(&tx_buf[pos], rem_len);
    
    tx_buf[pos++] = topic_len >> 8; tx_buf[pos++] = topic_len & 0xFF;
    memcpy(&tx_buf[pos], topic, topic_len); pos += topic_len;
    
    memcpy(&tx_buf[pos], json, payload_len); pos += payload_len;
    
    printf("[MQTT] Pub: %s -> %s\r\n", topic, json);
    
    // 返回底层的发送结果
    return ESP_Send_Packet(tx_buf, pos);
}

// 发布数据
uint8_t ESP_MQTT_Publish(char *json_data) {
    return MQTT_Publish_Packet(TOPIC_PUB, json_data);
}

// 初始化函数
uint8_t ESP_Init_MQTT(void) {
    // 1. 创建互斥锁
    if(xESPMutex == NULL) xESPMutex = xSemaphoreCreateMutex();

    char cmd[128];
    
    printf("\r\n[ESP] Start/Reset Connection sequence...\r\n");

    ESP_Send_Cmd("AT+CIPCLOSE", "ERROR", 500); // 断开旧连接
    ESP_Send_Cmd("AT+RST", "OK", 2000);        // 发送复位
    vTaskDelay(pdMS_TO_TICKS(3000)); 
    ESP_Clear_Rx(); 
    ESP_Send_Cmd("AT+CWMODE=1", "OK", 1000);
	
	ESP_Send_Cmd("AT+CWAUTOCONN=0", "OK", 1000);

    // ==========================================
    // 2. 连接 WiFi
    // ==========================================
    printf("[ESP] Connecting to WiFi: %s...\r\n", WIFI_SSID);
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASS);
    
    if(ESP_Send_Cmd(cmd, "GOT IP", 20000)) {
        printf("[ESP] WiFi Connected.\r\n");
		wifi_connected();
    } else {
        printf("[ESP] WiFi Connect Failed!\r\n");
        printf("[Debug] ESP Response: %s\r\n", esp_rx_buf);
        
        if(strstr((char*)esp_rx_buf, "No AP")) {
            printf("[Analysis] Error: Cannot find SSID (Check 2.4G/Antenna)\r\n");
        } else if(strstr((char*)esp_rx_buf, "FAIL")) {
            printf("[Analysis] Error: Password Wrong or Signal weak\r\n");
        }
		
        wifi_disconnected();
		
        return 0; // 触发重连
    }

    // 强制休息 2 秒
    printf("[ESP] Waiting 2s for TCP Stack stabilization...\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000)); 

    // ==========================================
    // 4. 连接 TCP (阿里云)
    // ==========================================
    printf("[ESP] Connecting to %s:%d...\r\n", MQTT_BROKER, MQTT_PORT);
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", MQTT_BROKER, MQTT_PORT);
    
    if(ESP_Send_Cmd(cmd, "CONNECT", 10000) || ESP_Send_Cmd(NULL, "OK", 1000)) {
        printf("[ESP] TCP Connected.\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
		
		aliyun_connected();
        
        ESP_Clear_Rx();
        MQTT_Connect(); // 发送 MQTT 登录包

        // 等待回复
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        if(strstr((char*)esp_rx_buf, "CLOSED")) {
             printf("[Error] Server closed connection. (Auth Failed...)\r\n");
			 aliyun_disconnected();
			
             return 0;
        } else {
             printf("[Success] MQTT Connected!\r\n");
			 aliyun_connected();
			
             // ==========================================
             // 连接成功后发送订阅报文
             // ==========================================
             vTaskDelay(pdMS_TO_TICKS(500)); 
             MQTT_Subscribe_Packet(TOPIC_SUB, 0); 
			
			vTaskDelay(pdMS_TO_TICKS(1000));
             // ==========================================

             return 1;
        }
    } else {
        printf("[ESP] TCP Connect Failed. (Check Code Timeout...)\r\n");
		printf("[Debug] ESP Response: %s\r\n", esp_rx_buf);
		aliyun_disconnected();
		
        return 0;
    }
}

// ===============================================
// 处理接收到的 MQTT 下发数据
// ===============================================
void ESP_Process_Rx_Data(void) {
    if (esp_rx_index == 0) return;

    // 🚨 摒弃 strstr，全面改用 ESP_Memstr！就算前面有一万个乱码和 0x00，也能精准揪出指令！
    
    if (ESP_Memstr(esp_rx_buf, esp_rx_index, "\"relay\":0") != NULL || 
        ESP_Memstr(esp_rx_buf, esp_rx_index, "\"relay\": 0") != NULL) {
        
        printf("\r\n===================================\r\n");
        printf("[MQTT RX] 成功捕获指令 -> 🚨 自动断开继电器 (OFF)\r\n");
        printf("===================================\r\n");
        
        buzzer_off();
        
        ESP_Clear_Rx();
        return;
    }
    else if (ESP_Memstr(esp_rx_buf, esp_rx_index, "\"relay\":1") != NULL || 
             ESP_Memstr(esp_rx_buf, esp_rx_index, "\"relay\": 1") != NULL) {
             
        printf("\r\n===================================\r\n");
        printf("[MQTT RX] 成功捕获指令 -> ⚡ 恢复通电 (ON)\r\n");
        printf("===================================\r\n");
        
        buzzer_on();
        
        ESP_Clear_Rx();
        return;
    }

    // 清理无用的确认包
    char *ipd_ptr = ESP_Memstr(esp_rx_buf, esp_rx_index, "+IPD,");
    if (ipd_ptr != NULL) {
        int ipd_len = atoi(ipd_ptr + 5); 
        if (ipd_len > 0 && ipd_len < 10) {
            ESP_Clear_Rx();
            return;
        }
    }
    
    // 防卡死保护
    if (esp_rx_index > 250) {
        ESP_Clear_Rx();
    }
}
