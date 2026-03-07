#ifndef __OLED_H
#define __OLED_H

#include "main.h" // 包含此文件以引入 HAL 库和 hi2c1

// OLED I2C 地址 (0x3C << 1 = 0x78)
#define OLED_ADDRESS 0x78

// 声明外部的 I2C 句柄 (由 STM32CubeMX 生成)
extern I2C_HandleTypeDef hi2c1;

// OLED 控制函数
void OLED_Write_Cmd(uint8_t cmd);
void OLED_Write_Data(uint8_t data);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Set_Pos(uint8_t x, uint8_t y);

// OLED 显示函数
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t Char_Size);
void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t Char_Size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size2);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t no);

// 辅助函数
uint32_t OLED_Pow(uint8_t m, uint8_t n);

#endif
