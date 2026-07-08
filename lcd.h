/**
 * @file    lcd.h
 * @brief   3.5寸 TFT LCD 驱动头文件 (FSMC接口)
 * @note    ATK-MD0350 (ST7796S), 320x480
 *          基于正点原子实验13代码改编，适配DNV307开发板
 *
 *          DNV307 FSMC 引脚：
 *            CS(NE1)=PD7, WR(NWE)=PD5, RD(NOE)=PD4
 *            RS(A19)=PE3
 *            RST=PD10 (与 FSMC_D14 共用)
 *            BL=PB4
 *            D0~D15 分布在 PD/PE 多个引脚
 *          注意：PD10(RST/D14) 和 PD11(蜂鸣器/D15) 为复用引脚
 */

#ifndef __LCD_H
#define __LCD_H

#include "ch32v30x.h"

/******************************************************************************************/
/* LCD 控制引脚定义 */

#define LCD_WR_GPIO_PORT                GPIOD
#define LCD_WR_GPIO_PIN                 GPIO_Pin_5

#define LCD_RD_GPIO_PORT                GPIOD
#define LCD_RD_GPIO_PIN                 GPIO_Pin_4

#define LCD_BL_GPIO_PORT                GPIOB
#define LCD_BL_GPIO_PIN                 GPIO_Pin_4        /* PB4 背光 */

#define LCD_CS_GPIO_PORT                GPIOD
#define LCD_CS_GPIO_PIN                 GPIO_Pin_7        /* PD7 = FSMC_NE1 */

#define LCD_RS_GPIO_PORT                GPIOE
#define LCD_RS_GPIO_PIN                 GPIO_Pin_3        /* PE3 = FSMC_A19 */

/* FSMC 地址线：使用 A19 连接 RS */
#define LCD_FSMC_AX                     19

/******************************************************************************************/
/* LCD 重要参数 */

typedef struct
{
    uint16_t width;                     /* LCD 宽度 */
    uint16_t height;                    /* LCD 高度 */
    uint16_t id;                        /* LCD ID */
    uint8_t  dir;                       /* 0=竖屏, 1=横屏 */
    uint16_t wramcmd;                   /* 写GRAM指令 */
    uint16_t setxcmd;                   /* 设置X坐标指令 */
    uint16_t setycmd;                   /* 设置Y坐标指令 */
} _lcd_dev;

extern _lcd_dev lcddev;

/* 画笔颜色和背景色 */
extern uint32_t g_point_color;
extern uint32_t g_back_color;

/* 背光控制 */
#define LCD_BL(x)  do { x ? \
    GPIO_SetBits(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN) : \
    GPIO_ResetBits(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN); \
} while(0)

/******************************************************************************************/
/* FSMC 地址映射（结构体方式） */

typedef struct
{
    volatile uint16_t LCD_REG;          /* RS=0 时访问 → 寄存器/命令 */
    volatile uint16_t LCD_RAM;          /* RS=1 时访问 → 数据 */
} LCD_TypeDef;

/* LCD_BASE 计算：
 * NE1 基址 = 0x60000000
 * A19 偏移 = 2^19 * 2 = 0x100000 (16位模式下 A19 对应 bit20)
 * LCD_RAM 地址 = 0x60000000 + 0x100000 = 0x60100000
 * LCD_BASE = LCD_RAM - 2 = 0x600FFFFE
 */
#define LCD_BASE  (uint32_t)(0x60000000 | (((1 << LCD_FSMC_AX) * 2) - 2))
#define LCD       ((LCD_TypeDef *) LCD_BASE)

/******************************************************************************************/
/* 扫描方向定义 */

#define L2R_U2D     0
#define L2R_D2U     1
#define R2L_U2D     2
#define R2L_D2U     3
#define U2D_L2R     4
#define U2D_R2L     5
#define D2U_L2R     6
#define D2U_R2L     7
#define DFT_SCAN_DIR  L2R_U2D

/******************************************************************************************/
/* 颜色定义 RGB565 */

#define WHITE       0xFFFF
#define BLACK       0x0000
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define GRAY        0x8430
#define DARKBLUE    0x01CF
#define LIGHTBLUE   0x7D7C
#define GRAYBLUE    0x5458
#define LIGHTGREEN  0x841F
#define LGRAY       0xC618
#define LGRAYBLUE   0xA651
#define LBBLUE      0x2B12

/******************************************************************************************/
/* 函数声明 */

/* 底层读写 */
void lcd_wr_data(volatile uint16_t data);
void lcd_wr_regno(volatile uint16_t regno);
void lcd_write_reg(uint16_t regno, uint16_t data);

/* 初始化 */
void lcd_init(void);
void lcd_display_on(void);
void lcd_display_off(void);

/* 显示控制 */
void lcd_scan_dir(uint8_t dir);
void lcd_display_dir(uint8_t dir);
void lcd_set_cursor(uint16_t x, uint16_t y);
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
void lcd_write_ram_prepare(void);

/* 绘图 */
void lcd_clear(uint16_t color);
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);
uint32_t lcd_read_point(uint16_t x, uint16_t y);
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* 文字 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, const char *p, uint16_t color);

/******************************************************************************************/
/* DNV307 特有：蜂鸣器引脚复用控制 */

/**
 * @brief  蜂鸣器控制（PD11 与 FSMC_D15 共用引脚）
 * @param  on: 1=响, 0=停
 * @note   内部自动切换 PD10/PD11 的 GPIO/FSMC 模式
 */
void lcd_buzzer_set(int on);

/**
 * @brief  LED 控制（PD12/PD13 与 FSMC_D12/D13 共用引脚）
 * @param  green: 1=亮, 0=灭
 * @param  red:   1=亮, 0=灭
 * @note   内部自动关闭/开启 FSMC 总线，切换引脚模式
 */
void lcd_led_set(int green, int red);

/* 单独控制函数（供 alarm.c 使用） */
void lcd_led_pins_gpio(void);
void lcd_led_pins_fsmc(void);

/* PD10/PD11 复用引脚切换（RST/蜂鸣器 与 FSMC_D14/D15） */
void lcd_shared_pins_gpio(void);
void lcd_shared_pins_fsmc(void);

/* FSMC 总线手动控制（LED 控制后 FSMC 默认关闭，LCD 操作前需手动开启） */
void lcd_fsmc_enable(void);
void lcd_fsmc_disable(void);

/******************************************************************************************/
/* 兼容旧接口的宏（供 main.c / lcd_ui.c 使用） */

#define LCD_WIDTH   lcddev.width
#define LCD_HEIGHT  lcddev.height

/* 大写 API 别名（兼容项目已有代码） */
#define LCD_Init()                  lcd_init()
#define LCD_Clear(c)                lcd_clear(c)
#define LCD_DrawPoint(x,y,c)        lcd_draw_point(x,y,c)
#define LCD_FillRect(x,y,w,h,c)    lcd_fill(x,y,x+(w)-1,y+(h)-1,c)
#define LCD_DrawLine(x1,y1,x2,y2,c) lcd_draw_line(x1,y1,x2,y2,c)
#define LCD_SetWindow(x,y,w,h)     lcd_set_window(x,y,w,h)
#define LCD_ShowString(x,y,s,fc,bc) do { g_back_color = (bc); lcd_show_string(x,y,200,16,16,s,fc); } while(0)
#define LCD_SetBackLight(on)        LCD_BL(on)
#define LCD_GetDriverType()         lcddev.id
#define LCD_GetRawID()              ((uint32_t)lcddev.id)
#define LCD_BuzzerSet(on)           lcd_buzzer_set(on)

#endif /* __LCD_H */
