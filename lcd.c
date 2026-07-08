/**
 * @file    lcd.c
 * @brief   3.5寸 TFT LCD 驱动 (FSMC接口)
 * @note    基于正点原子实验13代码改编，适配DNV307开发板
 *          支持 ST7796S 驱动IC，320x480 分辨率
 *
 *          DNV307 FSMC 引脚（原理图）：
 *            CS(NE1)=PD7, WR(NWE)=PD5, RD(NOE)=PD4
 *            RS(A19)=PE3, RST=PD10(共用), BL=PB4
 *            D0=PE7,  D1=PD15, D2=PD0,  D3=PD1
 *            D4=PE8,  D5=PE9,  D6=PE10, D7=PE11
 *            D8=PE12, D9=PE13, D10=PE14,D11=PE15
 *            D12=PD8, D13=PD9, D14=PD10,D15=PD11
 *          PD10(RST/D14) 和 PD11(蜂鸣器/D15) 为复用引脚
 */

#include "lcd.h"
#include "lcdfont.h"
#include <stdio.h>

/* 画笔颜色和背景色 */
uint32_t g_point_color = 0xF800;    /* 默认红色 */
uint32_t g_back_color  = 0xFFFF;    /* 背景色默认白色 */

/* LCD 重要参数 */
_lcd_dev lcddev;

/* 复用引脚：PD10(RST/D14), PD11(BUZZER/D15) */
#define LCD_SHARED_PINS  (GPIO_Pin_10 | GPIO_Pin_11)

/******************************************************************************************/
/* 延时函数（简单软件延时，约1ms @144MHz） */

static void lcd_delay_ms(volatile uint32_t ms)
{
    while (ms--)
    {
        volatile uint32_t count = 6000;  /* ~1ms @144MHz */
        while (count--);
    }
}

/******************************************************************************************/
/* 复用引脚模式切换（DNV307 特有） */

void lcd_shared_pins_gpio(void)
{
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LCD_SHARED_PINS;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &g);
}

void lcd_shared_pins_fsmc(void)
{
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LCD_SHARED_PINS;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &g);
}

/******************************************************************************************/
/* LED 引脚切换（PD12/PD13 = FSMC_D12/D13，需关闭 FSMC 才能控制 LED） */

#define LCD_LED_PINS  (GPIO_Pin_12 | GPIO_Pin_13)

void lcd_led_pins_gpio(void)
{
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, DISABLE);  /* 关闭 FSMC 总线 */
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LCD_LED_PINS;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &g);
}

void lcd_led_pins_fsmc(void)
{
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LCD_LED_PINS;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &g);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);   /* 重新开启 FSMC 总线 */
}

/**
 * @brief  直接控制 LED（内部自动处理 FSMC 切换）
 * @param  green: 0=灭, 1=亮（低电平点亮）
 * @param  red:   0=灭, 1=亮（低电平点亮）
 * @note   设置 LED 后 FSMC 保持关闭，PD12/PD13 由 GPIO 控制。
 *         下次 LCD 操作前需调用 lcd_fsmc_enable()。
 */
void lcd_led_set(int green, int red)
{
    lcd_led_pins_gpio();
    /* 注意：实际硬件 PD12=绿灯, PD13=红灯（与丝印相反） */
    if (green) GPIO_ResetBits(GPIOD, GPIO_Pin_12);  /* PD12=绿灯, 低电平亮 */
    else       GPIO_SetBits(GPIOD, GPIO_Pin_12);
    if (red)   GPIO_ResetBits(GPIOD, GPIO_Pin_13);  /* PD13=红灯, 低电平亮 */
    else       GPIO_SetBits(GPIOD, GPIO_Pin_13);
    /* FSMC 保持关闭，LED 状态可保持 */
}

/* FSMC 总线控制（供 main.c 在 LCD 操作前后调用） */
void lcd_fsmc_enable(void)
{
    /* PD11 切回 AF 模式（FSMC_D15） */
    {
        GPIO_InitTypeDef g = {0};
        g.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
        g.GPIO_Mode = GPIO_Mode_AF_PP;
        g.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOD, &g);
    }
    lcd_led_pins_fsmc();   /* PD12/PD13 → AF, FSMC 开启 */
}

void lcd_fsmc_disable(void)
{
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, DISABLE);
    /* PD11 切回 GPIO 模式，输出 LOW（蜂鸣器关） */
    {
        GPIO_InitTypeDef g = {0};
        g.GPIO_Pin = GPIO_Pin_11;
        g.GPIO_Mode = GPIO_Mode_Out_PP;
        g.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOD, &g);
        GPIO_ResetBits(GPIOD, GPIO_Pin_11);  /* 蜂鸣器关 */
    }
}

/******************************************************************************************/
/* LCD 硬件复位（PD10 与 FSMC_D14 共用，需临时切换模式） */

static void lcd_hardware_reset(void)
{
    lcd_shared_pins_gpio();           /* PD10/PD11 切为 GPIO */
    GPIO_ResetBits(GPIOD, GPIO_Pin_10);
    lcd_delay_ms(20);
    GPIO_SetBits(GPIOD, GPIO_Pin_10);
    lcd_delay_ms(100);
    lcd_shared_pins_fsmc();           /* 切回 FSMC 模式 */
}

/******************************************************************************************/
/* 底层读写函数 */

void lcd_wr_data(volatile uint16_t data)
{
    LCD->LCD_RAM = data;
}

void lcd_wr_regno(volatile uint16_t regno)
{
    LCD->LCD_REG = regno;
}

void lcd_write_reg(uint16_t regno, uint16_t data)
{
    LCD->LCD_REG = regno;
    LCD->LCD_RAM = data;
}

static uint16_t lcd_rd_data(void)
{
    volatile uint16_t ram;
    ram = LCD->LCD_RAM;
    return ram;
}

void lcd_write_ram_prepare(void)
{
    LCD->LCD_REG = lcddev.wramcmd;
}

/******************************************************************************************/
/* 读点颜色 */

uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t r = 0, g = 0, b = 0;

    if (x >= lcddev.width || y >= lcddev.height) return 0;

    lcd_set_cursor(x, y);

    lcd_wr_regno(0x2E);    /* 读GRAM指令 */
    r = lcd_rd_data();     /* dummy read */
    r = lcd_rd_data();     /* 实际读取 (ST7796 一次读出16位) */

    if (lcddev.id == 0x7796) return r;

    b = lcd_rd_data();
    g = r & 0xFF;
    g <<= 8;

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}

/******************************************************************************************/
/* 显示开关 */

void lcd_display_on(void)
{
    lcd_wr_regno(0x29);
}

void lcd_display_off(void)
{
    lcd_wr_regno(0x28);
}

/******************************************************************************************/
/* 设置光标 */

void lcd_set_cursor(uint16_t x, uint16_t y)
{
    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(x >> 8);
    lcd_wr_data(x & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(y >> 8);
    lcd_wr_data(y & 0xFF);
}

/******************************************************************************************/
/* 设置扫描方向 */

void lcd_scan_dir(uint8_t dir)
{
    uint16_t regval = 0, dirreg = 0;
    uint16_t temp;

    switch (dir)
    {
        case L2R_U2D: regval |= (0<<7)|(0<<6)|(0<<5); break;
        case L2R_D2U: regval |= (1<<7)|(0<<6)|(0<<5); break;
        case R2L_U2D: regval |= (0<<7)|(1<<6)|(0<<5); break;
        case R2L_D2U: regval |= (1<<7)|(1<<6)|(0<<5); break;
        case U2D_L2R: regval |= (0<<7)|(0<<6)|(1<<5); break;
        case U2D_R2L: regval |= (0<<7)|(1<<6)|(1<<5); break;
        case D2U_L2R: regval |= (1<<7)|(0<<6)|(1<<5); break;
        case D2U_R2L: regval |= (1<<7)|(1<<6)|(1<<5); break;
    }

    /* ST7796 需要设置 BGR 位 */
    if (lcddev.id == 0x7796) regval |= 0x08;

    dirreg = 0x36;
    lcd_write_reg(dirreg, regval);

    if (regval & 0x20)
    {
        if (lcddev.width < lcddev.height)
        {
            temp = lcddev.width; lcddev.width = lcddev.height; lcddev.height = temp;
        }
    }
    else
    {
        if (lcddev.width > lcddev.height)
        {
            temp = lcddev.width; lcddev.width = lcddev.height; lcddev.height = temp;
        }
    }

    /* 设置显示区域 */
    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(0); lcd_wr_data(0);
    lcd_wr_data((lcddev.width - 1) >> 8);
    lcd_wr_data((lcddev.width - 1) & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(0); lcd_wr_data(0);
    lcd_wr_data((lcddev.height - 1) >> 8);
    lcd_wr_data((lcddev.height - 1) & 0xFF);
}

/******************************************************************************************/
/* 设置显示方向 */

void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir;

    if (dir == 0)   /* 竖屏 */
    {
        lcddev.wramcmd = 0x2C;
        lcddev.setxcmd = 0x2A;
        lcddev.setycmd = 0x2B;
        lcddev.width = 320;
        lcddev.height = 480;
    }
    else            /* 横屏 */
    {
        lcddev.wramcmd = 0x2C;
        lcddev.setxcmd = 0x2A;
        lcddev.setycmd = 0x2B;
        lcddev.width = 480;
        lcddev.height = 320;
    }

    lcd_scan_dir(DFT_SCAN_DIR);
}

/******************************************************************************************/
/* 设置窗口 */

void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth = sx + width - 1;
    uint16_t theight = sy + height - 1;

    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(sx >> 8);   lcd_wr_data(sx & 0xFF);
    lcd_wr_data(twidth >> 8); lcd_wr_data(twidth & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(sy >> 8);   lcd_wr_data(sy & 0xFF);
    lcd_wr_data(theight >> 8); lcd_wr_data(theight & 0xFF);
}

/******************************************************************************************/
/* ST7796 寄存器初始化（来自实验13 lcd_ex.c，已验证） */

static void lcd_ex_st7796_reginit(void)
{
    lcd_wr_regno(0x11);             /* Sleep Out */
    lcd_delay_ms(120);

    lcd_wr_regno(0x36);             /* Memory Access Control */
    lcd_wr_data(0x08);              /* BGR=1, 正常扫描方向 */

    lcd_wr_regno(0x3A);             /* Pixel Format */
    lcd_wr_data(0x55);              /* 16bit RGB565 */

    lcd_wr_regno(0xF0);             /* *** 解锁命令1 *** */
    lcd_wr_data(0xC3);
    lcd_wr_regno(0xF0);             /* *** 解锁命令2 *** */
    lcd_wr_data(0x96);

    lcd_wr_regno(0xB4);             /* Display Inversion */
    lcd_wr_data(0x01);

    lcd_wr_regno(0xB6);             /* Display Function Control */
    lcd_wr_data(0x0A);
    lcd_wr_data(0xA2);

    lcd_wr_regno(0xB7);             /* Gate Control */
    lcd_wr_data(0xC6);

    lcd_wr_regno(0xB9);             /* Power Control */
    lcd_wr_data(0x02);
    lcd_wr_data(0xE0);

    lcd_wr_regno(0xC0);             /* VCOM */
    lcd_wr_data(0x80);
    lcd_wr_data(0x16);

    lcd_wr_regno(0xC1);             /* VCOM Control */
    lcd_wr_data(0x19);

    lcd_wr_regno(0xC2);             /* VRH Set */
    lcd_wr_data(0xA7);

    lcd_wr_regno(0xC5);             /* VDV Set */
    lcd_wr_data(0x16);

    lcd_wr_regno(0xE8);             /* Display Output Ctrl */
    lcd_wr_data(0x40);
    lcd_wr_data(0x8A);
    lcd_wr_data(0x00);
    lcd_wr_data(0x00);
    lcd_wr_data(0x29);
    lcd_wr_data(0x19);
    lcd_wr_data(0xA5);
    lcd_wr_data(0x33);

    /* Positive Gamma */
    lcd_wr_regno(0xE0);
    lcd_wr_data(0xF0); lcd_wr_data(0x07);
    lcd_wr_data(0x0D); lcd_wr_data(0x04);
    lcd_wr_data(0x05); lcd_wr_data(0x14);
    lcd_wr_data(0x36); lcd_wr_data(0x54);
    lcd_wr_data(0x4C); lcd_wr_data(0x38);
    lcd_wr_data(0x13); lcd_wr_data(0x14);
    lcd_wr_data(0x2E); lcd_wr_data(0x34);

    /* Negative Gamma */
    lcd_wr_regno(0xE1);
    lcd_wr_data(0xF0); lcd_wr_data(0x10);
    lcd_wr_data(0x14); lcd_wr_data(0x0E);
    lcd_wr_data(0x0C); lcd_wr_data(0x08);
    lcd_wr_data(0x35); lcd_wr_data(0x44);
    lcd_wr_data(0x4C); lcd_wr_data(0x26);
    lcd_wr_data(0x10); lcd_wr_data(0x12);
    lcd_wr_data(0x2C); lcd_wr_data(0x32);

    lcd_wr_regno(0xF0);             /* 锁定命令1 */
    lcd_wr_data(0x3C);
    lcd_wr_regno(0xF0);             /* 锁定命令2 */
    lcd_wr_data(0x69);

    lcd_delay_ms(120);

    lcd_wr_regno(0x20);             /* Display Inversion Off */
    lcd_wr_regno(0x29);             /* Display ON */
}

/******************************************************************************************/
/* LCD 初始化入口 */

void lcd_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    FSMC_NORSRAMInitTypeDef fsmc = {0};
    FSMC_NORSRAMTimingInitTypeDef fsmc_read = {0};
    FSMC_NORSRAMTimingInitTypeDef fsmc_write = {0};

    /* 第1步：使能时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOD |
                           RCC_APB2Periph_GPIOE, ENABLE);

    /* 第2步：FSMC 数据线 GPIO 初始化 */
    /* PD: D0(PD0), D1(PD1), D8(PD8), D9(PD9), D10(PD10),
     *     D12(PD8→实际PD12), D13(PD9→实际PD13),
     *     NOE(PD4), NWE(PD5), NE1(PD7), D14(PD14→实际PD15?) */
    /* 按实验13官方代码 + DNV307原理图，初始化所有 FSMC 数据引脚 */
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
                    GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                    GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &gpio);

    /* PE: RS/A19(PE3), D4(PE8), D5(PE9), D6(PE10), D7(PE11),
     *     D8(PE12), D9(PE13), D10(PE14), D11(PE15), D0(PE7) */
    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 |
                    GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 |
                    GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &gpio);

    /* PD10/PD11 先设为 GPIO（RST 和蜂鸣器需要手动控制） */
    lcd_shared_pins_gpio();

    /* PB4: 背光 */
    gpio.GPIO_Pin = LCD_BL_GPIO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(LCD_BL_GPIO_PORT, &gpio);

    /* 第3步：FSMC 控制器配置 */
    fsmc.FSMC_Bank = FSMC_Bank1_NORSRAM1;
    fsmc.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    fsmc.FSMC_MemoryType = FSMC_MemoryType_SRAM;
    fsmc.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
    fsmc.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    fsmc.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
    fsmc.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    fsmc.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    fsmc.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    fsmc.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    fsmc.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;   /* 读写不同时序 */
    fsmc.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    fsmc.FSMC_ReadWriteTimingStruct = &fsmc_read;
    fsmc.FSMC_WriteTimingStruct = &fsmc_write;

    /* 读时序（保守，兼容各种屏幕） */
    fsmc_read.FSMC_AddressSetupTime = 15;
    fsmc_read.FSMC_AddressHoldTime = 0;
    fsmc_read.FSMC_DataSetupTime = 60;
    fsmc_read.FSMC_BusTurnAroundDuration = 0;
    fsmc_read.FSMC_CLKDivision = 0;
    fsmc_read.FSMC_DataLatency = 0;
    fsmc_read.FSMC_AccessMode = FSMC_AccessMode_A;

    /* 写时序（ST7796 可用较快速率） */
    fsmc_write.FSMC_AddressSetupTime = 9;
    fsmc_write.FSMC_AddressHoldTime = 0;
    fsmc_write.FSMC_DataSetupTime = 9;
    fsmc_write.FSMC_BusTurnAroundDuration = 0;
    fsmc_write.FSMC_CLKDivision = 0;
    fsmc_write.FSMC_DataLatency = 0;
    fsmc_write.FSMC_AccessMode = FSMC_AccessMode_A;

    FSMC_NORSRAMInit(&fsmc);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);

    /* 第4步：硬件复位 */
    lcd_delay_ms(50);
    lcd_hardware_reset();

    /* 第5步：读取 LCD ID（自动检测驱动IC型号） */
    /* 尝试读取 ST7796 ID */
    lcd_wr_regno(0xD3);
    lcddev.id = lcd_rd_data();      /* dummy read */
    lcddev.id = lcd_rd_data();      /* 0x00 */
    lcddev.id = lcd_rd_data();      /* 0x77 */
    lcddev.id <<= 8;
    lcddev.id |= lcd_rd_data();     /* 0x96 → ID = 0x7796 */

    if (lcddev.id != 0x7796)
    {
        /* 不是 ST7796，尝试 ILI9341 */
        lcd_wr_regno(0xD3);
        lcddev.id = lcd_rd_data();
        lcddev.id = lcd_rd_data();
        lcddev.id = lcd_rd_data();
        lcddev.id <<= 8;
        lcddev.id |= lcd_rd_data();

        if (lcddev.id != 0x9341)
        {
            /* 尝试 ST7789 */
            lcd_wr_regno(0x04);
            lcddev.id = lcd_rd_data();
            lcddev.id = lcd_rd_data();
            lcddev.id = lcd_rd_data();
            lcddev.id <<= 8;
            lcddev.id |= lcd_rd_data();

            if (lcddev.id == 0x8552) lcddev.id = 0x7789;
        }
    }

    printf("LCD ID: %x\r\n", lcddev.id);

    /* 第6步：根据 ID 执行对应的初始化序列 */
    if (lcddev.id == 0x7796)
    {
        lcd_ex_st7796_reginit();
    }
    else if (lcddev.id == 0x9341 || lcddev.id == 0x7789)
    {
        /* ILI9341/ST7789 基本初始化（简化版） */
        lcd_wr_regno(0x11); lcd_delay_ms(120);
        lcd_wr_regno(0x36); lcd_wr_data(0x48);
        lcd_wr_regno(0x3A); lcd_wr_data(0x55);
        lcd_wr_regno(0x29); lcd_delay_ms(50);
    }
    else
    {
        /* 未知 ID，尝试 ST7796 初始化 */
        printf("Warning: Unknown LCD ID, trying ST7796 init\r\n");
        lcd_ex_st7796_reginit();
        lcddev.id = 0x7796;
    }

    /* 第7步：适当提速写时序（不能太快，否则绘图操作会失败） */
    if (lcddev.id == 0x7796)
    {
        fsmc_write.FSMC_AddressSetupTime = 3;
        fsmc_write.FSMC_DataSetupTime = 6;
    }
    else if (lcddev.id == 0x9341)
    {
        fsmc_write.FSMC_AddressSetupTime = 3;
        fsmc_write.FSMC_DataSetupTime = 6;
    }
    FSMC_NORSRAMInit(&fsmc);

    /* 第8步：设置显示方向和背光 */
    lcd_display_dir(0);     /* 默认竖屏 */
    LCD_BL(1);              /* 点亮背光 */
    lcd_clear(WHITE);       /* 清屏 */

    /* 第9步：恢复 PD10/PD11 为 FSMC 模式（D14/D15） */
    lcd_shared_pins_fsmc();

    /* 第10步：关闭 FSMC，让 PD12/PD13 可由 GPIO 控制 LED */
    /* LCD 操作前需调用 lcd_fsmc_enable() 临时开启 FSMC */
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, DISABLE);
}

/******************************************************************************************/
/* 清屏 */

void lcd_clear(uint16_t color)
{
    uint32_t i, total;
    total = (uint32_t)lcddev.width * lcddev.height;
    lcd_set_cursor(0, 0);
    lcd_write_ram_prepare();
    for (i = 0; i < total; i++) LCD->LCD_RAM = color;
}

/******************************************************************************************/
/* 画点 */

void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    lcd_set_cursor(x, y);
    lcd_write_ram_prepare();
    LCD->LCD_RAM = color;
}

/******************************************************************************************/
/* 区域填充 */

void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i, j;
    uint16_t xlen = ex - sx + 1;

    for (i = sy; i <= ey; i++)
    {
        lcd_set_cursor(sx, i);
        lcd_write_ram_prepare();
        for (j = 0; j < xlen; j++) LCD->LCD_RAM = color;
    }
}

void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height, width;
    uint16_t i, j;
    width = ex - sx + 1;
    height = ey - sy + 1;
    for (i = 0; i < height; i++)
    {
        lcd_set_cursor(sx, sy + i);
        lcd_write_ram_prepare();
        for (j = 0; j < width; j++) LCD->LCD_RAM = color[i * width + j];
    }
}

/******************************************************************************************/
/* 画线 (Bresenham) */

void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }

    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }

    if (delta_x > delta_y) distance = delta_x;
    else distance = delta_y;

    for (t = 0; t <= distance + 1; t++)
    {
        lcd_draw_point(row, col, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height)) return;
    lcd_fill(x, y, x + len - 1, y, color);
}

void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

/******************************************************************************************/
/* 显示字符（支持 12/16/24/32 号字体） */

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize;
    uint8_t *pfont;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' ';

    switch (size)
    {
        case 12: pfont = (uint8_t *)asc2_1206[chr]; break;
        case 16: pfont = (uint8_t *)asc2_1608[chr]; break;
        case 24: pfont = (uint8_t *)asc2_2412[chr]; break;
        case 32: pfont = (uint8_t *)asc2_3216[chr]; break;
        default: return;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80) lcd_draw_point(x, y, color);
            else if (mode == 0) lcd_draw_point(x, y, g_back_color);
            temp <<= 1;
            y++;
            if (y >= lcddev.height) return;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= lcddev.width) return;
                break;
            }
        }
    }
}

/******************************************************************************************/
/* 显示数字 */

static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, 0, color);
                continue;
            }
            else enshow = 1;
        }
        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, 0, color);
    }
}

void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, mode, color);
                continue;
            }
            else enshow = 1;
        }
        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, mode, color);
    }
}

/******************************************************************************************/
/* 显示字符串 */

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, const char *p, uint16_t color)
{
    uint8_t x0 = x;
    width += x;
    height += y;

    while ((*p >= ' ') && (*p <= '~'))
    {
        if (x >= width) { x = x0; y += size; }
        if (y >= height) break;
        lcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}

/******************************************************************************************/
/* 蜂鸣器控制（PD11 与 FSMC_D15 共用引脚） */

void lcd_buzzer_set(int on)
{
    /* 把 PD11 切到 GPIO 模式，设置电平后保持 GPIO 模式
     * 注意：不再调用 lcd_shared_pins_fsmc()，让 PD11 保持 GPIO 状态
     * 下次 LCD 操作前 lcd_fsmc_enable() 会切回 AF 模式 */
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = GPIO_Pin_11;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &g);
    if (on) GPIO_SetBits(GPIOD, GPIO_Pin_11);
    else    GPIO_ResetBits(GPIOD, GPIO_Pin_11);
}
