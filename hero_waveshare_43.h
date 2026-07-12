#pragma once
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"

// 微雪 4.3 吋官方精準硬體腳位
#define LCD_GPIO_RGB_Hsync      40
#define LCD_GPIO_RGB_Vsync      41
#define LCD_GPIO_RGB_Pclk       39
#define LCD_GPIO_RGB_De         42
#define LCD_GPIO_RGB_Disp       -1
#define LCD_GPIO_RGB_Bl         2

#define LCD_GPIO_RGB_D0         8
#define LCD_GPIO_RGB_D1         15
#define LCD_GPIO_RGB_D2         16
#define LCD_GPIO_RGB_D3         7
#define LCD_GPIO_RGB_D4         6
#define LCD_GPIO_RGB_D5         5
#define LCD_GPIO_RGB_D6         4
#define LCD_GPIO_RGB_D7         9
#define LCD_GPIO_RGB_D8         3
#define LCD_GPIO_RGB_D9         46
#define LCD_GPIO_RGB_D10        10
#define LCD_GPIO_RGB_D11        11
#define LCD_GPIO_RGB_D12        12
#define LCD_GPIO_RGB_D13        13
#define LCD_GPIO_RGB_D14        14
#define LCD_GPIO_RGB_D15        0

// 微雪官方 ST7701S 初始化序列
static const uint8_t vendor_specific_init_code[] = {
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    0xC0, 2, 0x3B, 0x00,
    0xC1, 2, 0x0D, 0x02,
    0xC2, 2, 0x31, 0x05,
    0xCD, 1, 0x08,
    0xB0, 16, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,
    0xB1, 16, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,
    0x11, 0,
    0x29, 0
};