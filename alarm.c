/**
 * @file    alarm.c
 * @brief   报警控制模块
 * @note    蜂鸣器(PD11)经三极管驱动，高电平响、低电平静音
 *          绿色LED(PD13)、红色LED(PD12)均为低电平点亮（共阳极接法）
 *
 *          重要：PD11 同时作为 FSMC_D15 数据线（LCD驱动），
 *          PD12/PD13 同时作为 FSMC_D12/D13 数据线，
 *          所有引脚操作必须通过 lcd.c 提供的接口进行 FSMC 模式切换，
 *          不能直接操作 GPIO。
 */

#include "ch32v30x.h"
#include "lcd.h"   /* lcd_buzzer_set(), lcd_led_set() */

/* ============================================================
 * 报警模块初始化
 * 注意：此时 LCD 尚未初始化，FSMC 未开启，直接操作 GPIO
 * LCD_Init() 之后 PD11/PD12/PD13 由 lcd.c 接管
 * ============================================================ */
void Alarm_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* 开启GPIOD时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    /* PD11 蜂鸣器、PD12 红灯、PD13 绿灯（推挽输出） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    /* 初始状态：蜂鸣器关、绿灯亮、红灯灭 */
    GPIO_ResetBits(GPIOD, GPIO_Pin_11); /* 蜂鸣器关 */
    GPIO_ResetBits(GPIOD, GPIO_Pin_13); /* 绿灯亮（低电平） */
    GPIO_SetBits(GPIOD, GPIO_Pin_12);   /* 红灯灭（高电平） */
}

/* ============================================================
 * 触发报警（蜂鸣器开 + 绿灯灭 + 红灯亮）
 * ============================================================ */
void Alarm_On(void)
{
    lcd_buzzer_set(1);          /* 蜂鸣器响 */
    lcd_led_set(0, 1);          /* 绿灯灭，红灯亮 */
}

/* ============================================================
 * 解除报警（蜂鸣器关 + 绿灯亮 + 红灯灭）
 * ============================================================ */
void Alarm_Off(void)
{
    lcd_buzzer_set(0);          /* 蜂鸣器停 */
    lcd_led_set(1, 0);          /* 绿灯亮，红灯灭 */
}

/* ============================================================
 * 红灯闪烁（报警过程中调用）
 * ============================================================ */
void Alarm_ToggleRed(void)
{
    /* 读取当前红灯状态（通过 lcd_led_set 切换） */
    /* 由于 FSMC 模式下无法直接读 GPIO，使用静态变量记录 */
    static int s_red_on = 1;
    s_red_on = !s_red_on;
    lcd_led_set(0, s_red_on);   /* 绿灯灭，红灯翻转 */
}

/* ============================================================
 * 绿灯控制（用于心跳指示）
 * ============================================================ */
void Alarm_GreenOn(void)
{
    lcd_led_set(1, 0);          /* 绿灯亮，红灯灭 */
}

void Alarm_GreenOff(void)
{
    lcd_led_set(0, 0);          /* 绿灯灭，红灯灭 */
}
