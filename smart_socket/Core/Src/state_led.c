#include "freertos.h"
#include "task.h"
#include "state_led.h"
#include "gpio.h"

void mqttSend_normal(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(MQTTSEND_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(MQTTSEND_TIME);
}
void mqttSend_abnormal(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(MQTTSEND_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(MQTTSEND_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(MQTTSEND_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(MQTTSEND_TIME);
}
void wifi_connected(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(WIFI_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(WIFI_TIME);
}
void wifi_disconnected(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(WIFI_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(WIFI_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(WIFI_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(WIFI_TIME);
}
void aliyun_connected(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(ALIYUN_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(ALIYUN_TIME);
}
void aliyun_disconnected(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(ALIYUN_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(ALIYUN_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	vTaskDelay(ALIYUN_TIME);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	vTaskDelay(ALIYUN_TIME);
}
