/**
 * @file    main.c
 * @brief   坑道废墟震动生命探测信标
 * @note    压电陶瓷：3次敲击报警 → 红灯亮+绿灯灭+蜂鸣器响
 *          HC-SR501：检测到人 → 仅蜂鸣器响（LED不变）
 *          主循环周期 50ms，TIM2 提供 1ms 时基
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ch32v30x.h"
#include "adc_dma.h"
#include "signal_proc.h"
#include "pattern.h"
#include "alarm.h"
#include "lcd.h"
#include "lcd_ui.h"
#include "uart_debug.h"
#include "pir.h"

#define MAIN_LOOP_PERIOD_MS  50
#define ADC_BUF_SIZE         512

static volatile uint32_t s_tick_ms = 0;

/* TIM2 中断（1ms） */
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        s_tick_ms++;
    }
}

uint32_t GetTick(void) { return s_tick_ms; }

static void TIM2_Init_1ms(void)
{
    TIM_TimeBaseInitTypeDef t = {0};
    NVIC_InitTypeDef n = {0};
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    t.TIM_Prescaler = 72 - 1;
    t.TIM_Period = 1000 - 1;
    t.TIM_ClockDivision = TIM_CKD_DIV1;
    t.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &t);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    n.NVIC_IRQChannel = TIM2_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 0;
    n.NVIC_IRQChannelSubPriority = 0;
    n.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&n);
    TIM_Cmd(TIM2, ENABLE);
}

static void Command_Handler(const char *cmd)
{
    if (strcmp(cmd, "help") == 0) {
        printf("help status threshold alarm reset\r\n");
    } else if (strcmp(cmd, "status") == 0) {
        printf("State:%d Th:%d Tick:%lu\r\n",
               Pattern_GetState(), Signal_GetThreshold(), (unsigned long)s_tick_ms);
    } else if (strncmp(cmd, "threshold ", 10) == 0) {
        int val = atoi(cmd + 10);
        Signal_SetThreshold((uint16_t)val);
        printf("Th=%d\r\n", val);
    } else if (strcmp(cmd, "alarm") == 0) {
        Pattern_ForceAlarm(GetTick());
        printf("Alarm!\r\n");
    } else if (strcmp(cmd, "reset") == 0) {
        Pattern_Reset();
        printf("Reset\r\n");
    }
}

int main(void)
{
    uint32_t last_loop_tick = 0;
    uint32_t last_display_tick = 0;
    int prev_state = -1;
    int pir_motion = 0;

    SystemCoreClockUpdate();
    __enable_irq();
    TIM2_Init_1ms();

    UART_Debug_Init();
    UART_Debug_RegisterCmdHandler(Command_Handler);
    printf("\r\nBoot\r\n");

    Alarm_Init();
    LCD_Init();
    ADC_DMA_Init();
    PIR_Init();

    /* 启动画面 */
    g_back_color = BLACK;
    lcd_fsmc_enable();
    lcd_show_string(10, 10, 300, 16, 16, "=== System Boot ===", WHITE);
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "LCD ID: 0x%04X", lcddev.id);
        lcd_show_string(10, 35, 300, 16, 16, buf, lcddev.id ? GREEN : RED);
    }
    lcd_show_string(10, 60, 300, 16, 16, "Starting...", CYAN);
    lcd_fsmc_disable();

    /* 初始：绿灯亮，红灯灭，蜂鸣器关 */
    Alarm_Off();

    printf("Run\r\n");

    while (1) {
        uint32_t now = GetTick();

        if ((now - last_loop_tick) >= MAIN_LOOP_PERIOD_MS) {
            last_loop_tick = now;

            /* 读取HC-SR505人体感应 */
            pir_motion = PIR_Check();

            /* 压电陶瓷信号处理 */
            uint16_t *raw = ADC_GetData();

            uint16_t raw_min = 4095, raw_max = 0;
            int k;
            for (k = 0; k < ADC_BUF_SIZE; k++) {
                if (raw[k] < raw_min) raw_min = raw[k];
                if (raw[k] > raw_max) raw_max = raw[k];
            }

            int sensor_ok = 1;

            Signal_Filter(raw, ADC_BUF_SIZE);
            Signal_Envelope(ADC_BUF_SIZE);
            Signal_Smooth(ADC_BUF_SIZE);
            uint16_t thresh = Signal_CalcThreshold(ADC_BUF_SIZE);

            int peak = Signal_DetectPeak(thresh, now, ADC_BUF_SIZE);

            Pattern_Update(peak, now);
            int cur_state = Pattern_GetState();

            /* 压电陶瓷报警控制（仅状态变化时切换LED） */
            if (cur_state != prev_state) {
                prev_state = cur_state;
                if (cur_state == 3) {
                    Alarm_On();
                } else {
                    Alarm_Off();
                }
            }

            /* 蜂鸣器控制：压电陶瓷报警优先，PIR只在非报警时控制 */
            if (cur_state == 3) {
                /* 压电陶瓷报警中 → 蜂鸣器保持响（PIR不干预） */
            } else if (pir_motion) {
                /* HC-SR505检测到人 → 蜂鸣器响，LED不变 */
                lcd_buzzer_set(1);
            } else {
                /* 无人 + 无报警 → 蜂鸣器关 */
                lcd_buzzer_set(0);
            }

            /* LCD显示更新（每秒） */
            if ((now - last_display_tick) >= 1000) {
                last_display_tick = now;
                lcd_fsmc_enable();
                LCD_ShowMainUI(now, raw, thresh, cur_state, sensor_ok,
                              raw_max - raw_min, pir_motion);
                lcd_fsmc_disable();
                if (cur_state == 3) Alarm_On();
                else Alarm_Off();
            }
        }

        UART_Debug_Poll();
    }
}
