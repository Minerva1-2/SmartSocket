#include "oled_iic.h"
#include "oled_iic_font.h"
#include "cmsis_os.h"

// 1. 向 OLED 写入命令 (使用 Master_Transmit 提升兼容性，并加入 100ms 超时)
void OLED_Write_Cmd(uint8_t cmd) {
    uint8_t buf[2];
    buf[0] = 0x00; // 0x00 表示控制字节，后面是命令
    buf[1] = cmd;
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, buf, 2, 100);
}

// 2. 向 OLED 写入数据 (使用 Master_Transmit)
void OLED_Write_Data(uint8_t data) {
    uint8_t buf[2];
    buf[0] = 0x40; // 0x40 表示控制字节，后面是数据
    buf[1] = data;
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, buf, 2, 100);
}

// 设置光标位置 (页寻址模式)
void OLED_Set_Pos(uint8_t x, uint8_t y) {
    OLED_Write_Cmd(0xB0 + y);
    OLED_Write_Cmd(((x & 0xF0) >> 4) | 0x10);
    OLED_Write_Cmd(x & 0x0F);
}

// 清屏函数
void OLED_Clear(void) {
    uint8_t i, n;
    for (i = 0; i < 8; i++) {
        OLED_Write_Cmd(0xB0 + i);    // 设置页地址（0~7）
        OLED_Write_Cmd(0x00);        // 设置显示位置—列低地址
        OLED_Write_Cmd(0x10);        // 设置显示位置—列高地址
        for (n = 0; n < 128; n++) {
            OLED_Write_Data(0);      // 写入全0清空
        }
    }
}

// 初始化 OLED
void OLED_Init(void) {
    vTaskDelay(200); // 极度重要：上电后给屏幕充足的复位时间
    
    OLED_Write_Cmd(0xAE); // 关闭显示
    OLED_Write_Cmd(0x20); // 设置内存寻址模式
    OLED_Write_Cmd(0x02); // 页寻址模式
    OLED_Write_Cmd(0xB0); // 设置起始页地址
    OLED_Write_Cmd(0xC8); // COM 输出扫描方向 (上下反转的话改为 0xC0)
    OLED_Write_Cmd(0x00); // 设置低列地址
    OLED_Write_Cmd(0x10); // 设置高列地址
    OLED_Write_Cmd(0x40); // 设置起始行地址
    OLED_Write_Cmd(0x81); // 设置对比度控制
    OLED_Write_Cmd(0xFF); // 对比度值
    OLED_Write_Cmd(0xA1); // 设置段重映射 (左右反转的话改为 0xA0)
    OLED_Write_Cmd(0xA6); // 正常显示 (A7为反相显示)
    OLED_Write_Cmd(0xA8); // 设置多路复用率
    OLED_Write_Cmd(0x3F); // 1/64 duty
    OLED_Write_Cmd(0xA4); // 全局显示开启 (不依赖 RAM)
    OLED_Write_Cmd(0xD3); // 设置显示偏移
    OLED_Write_Cmd(0x00); // 不偏移
    OLED_Write_Cmd(0xD5); // 设置显示时钟分频比/振荡器频率
    OLED_Write_Cmd(0xF0); // 设置分频比
    OLED_Write_Cmd(0xD9); // 设置预充电周期
    OLED_Write_Cmd(0x22); 
    OLED_Write_Cmd(0xDA); // 设置 COM 硬件引脚配置
    OLED_Write_Cmd(0x12);
    OLED_Write_Cmd(0xDB); // 设置 VCOMH 取消选择级别
    OLED_Write_Cmd(0x20);
    OLED_Write_Cmd(0x8D); // 电荷泵设置
    OLED_Write_Cmd(0x14); // 开启电荷泵 (屏幕点亮关键)
    OLED_Write_Cmd(0xAF); // 开启 OLED 显示
    
    OLED_Clear();
}

// 3. 升级版：在指定位置显示一个字符 (8x16大小)
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t Char_Size) {
    uint8_t c = 0, i = 0;
    
    // 关键修复：标准 ASCII 字库是从空格 ' ' (ASCII 32) 开始的。
    // 这样才能完美支持显示字母 'V', 'A' 和空格 ' '
    c = chr - ' '; 
    
    if(x > 120) { x = 0; y += 2; }
    
    OLED_Set_Pos(x, y);
    for (i = 0; i < 8; i++) {
        OLED_Write_Data(F8X16[c * 16 + i]);
    }
    OLED_Set_Pos(x, y + 1);
    for (i = 0; i < 8; i++) {
        OLED_Write_Data(F8X16[c * 16 + i + 8]);
    }
}

// 4. 新增：显示字符串函数 (专用于配合 sprintf 显示传感器数据)
void OLED_ShowString(uint8_t x, uint8_t y, char *chr, uint8_t Char_Size) {
    uint8_t j = 0;
    while (chr[j] != '\0') {
        OLED_ShowChar(x, y, chr[j], Char_Size);
        x += 8; // 8x16字体，每个字符宽8个像素
        if (x > 120) { // 换行逻辑
            x = 0; 
            y += 2; 
        }
        j++;
    }
}

// 求幂函数
uint32_t OLED_Pow(uint8_t m, uint8_t n) {
    uint32_t result = 1;
    while(n--) result *= m;
    return result;
}

// 显示数字 (保留原有功能)
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size2) {
    uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < len; t++) {
        temp = (num / OLED_Pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar(x + (size2 / 2) * t, y, ' ', size2);
                continue;
            } else enshow = 1;
        }
        OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0', size2);
    }
}

// 显示一个汉字 (16x16)
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t no) {
    uint8_t t;
    OLED_Set_Pos(x, y);
    for (t = 0; t < 16; t++) {
        OLED_Write_Data(Hzk[no][t]);
    }
    OLED_Set_Pos(x, y + 1);
    for (t = 0; t < 16; t++) {
        OLED_Write_Data(Hzk[no][t + 16]);
    }
}
