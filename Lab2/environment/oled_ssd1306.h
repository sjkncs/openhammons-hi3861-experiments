/**
 * OLED SSD1306 显示屏驱动头文件
 * I2C 地址: 0x3C (7-bit), 128x64 像素
 */
#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

void OledInit(void);
void OledFillScreen(unsigned char fillData);
void OledShowString(unsigned char x, unsigned char y, const char *string);

#endif /* OLED_SSD1306_H */
