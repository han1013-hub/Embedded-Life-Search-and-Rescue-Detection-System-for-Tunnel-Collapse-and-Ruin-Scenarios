/**
 * @file    lcd_ui.c
 * @brief   LCD 显示界面 — 文本信息 + PIR状态
 */

#include "lcd.h"
#include "signal_proc.h"
#include "pattern.h"
#include "adc_dma.h"
#include <stdio.h>
#include <string.h>

void LCD_ShowMainUI(uint32_t now, uint16_t *raw, uint16_t thresh,
                    int state, int sensor_ok, uint16_t raw_range,
                    int pir_motion)
{
    char buf[64];
    uint16_t *env = Signal_GetEnvelope();
    int i;

    /* 计算统计数据 */
    uint16_t raw_min = 4095, raw_max = 0;
    uint16_t env_max = 0;
    for (i = 0; i < 512; i++) {
        if (raw[i] < raw_min) raw_min = raw[i];
        if (raw[i] > raw_max) raw_max = raw[i];
        if (env[i] > env_max) env_max = env[i];
    }

    g_back_color = BLACK;
    lcd_clear(BLACK);

    /* 标题 */
    lcd_show_string(10, 10, 300, 16, 16, "=== Vibration Detector ===", WHITE);

    /* ADC范围 */
    snprintf(buf, sizeof(buf), "ADC: %d ~ %d  R:%d", raw_min, raw_max, raw_range);
    lcd_show_string(10, 35, 300, 16, 16, buf, YELLOW);

    /* 阈值 + 峰值 */
    snprintf(buf, sizeof(buf), "Th:%d  Peak:%d", thresh, env_max);
    lcd_show_string(10, 60, 300, 16, 16, buf, CYAN);

    /* PIR人体感应状态 */
    if (pir_motion) {
        lcd_show_string(10, 85, 300, 16, 16, "PIR: MOTION DETECTED!", RED);
    } else {
        lcd_show_string(10, 85, 300, 16, 16, "PIR: No motion", GREEN);
    }

    /* 压电陶瓷状态 */
    switch (state) {
        case 0: lcd_show_string(10, 110, 300, 16, 16, "State: IDLE", GREEN); break;
        case 1: lcd_show_string(10, 110, 300, 16, 16, "State: 1 TAP", YELLOW); break;
        case 2: lcd_show_string(10, 110, 300, 16, 16, "State: 2 TAPS", YELLOW); break;
        case 3: lcd_show_string(10, 110, 300, 16, 16, "!!! ALARM !!!", RED); break;
    }

    /* 运行时间 */
    snprintf(buf, sizeof(buf), "Time: %lu s", now / 1000);
    lcd_show_string(10, 135, 300, 16, 16, buf, WHITE);
}
