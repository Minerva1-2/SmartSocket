#ifndef __STATE_LED_H_
#define __STATE_LED_H_

#define MQTTSEND_TIME 1000
#define WIFI_TIME     200
#define ALIYUN_TIME   2000

void mqttSend_normal(void);
void mqttSend_abnormal(void);
void mqttSend_normal(void);
void wifi_connected(void);
void wifi_disconnected(void);
void aliyun_connected(void);
void aliyun_disconnected(void);

#endif
