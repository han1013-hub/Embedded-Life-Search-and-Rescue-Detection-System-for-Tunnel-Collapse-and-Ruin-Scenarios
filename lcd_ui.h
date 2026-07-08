/**
 * @file    lcd_ui.h
 * @brief   LCD 显示界面模块头文件
 */

#ifndef __LCD_UI_H
#define __LCD_UI_H

#include "ch32v30x.h"

/**
 * @brief  显示主界面（文本信息 + PIR状态）
 * @param  now:       当前系统毫秒时刻
 * @param  raw:       原始ADC数据指针
 * @param  thresh:    当前检测阈值
 * @param  state:     系统状态（0=空闲 1=敲击1次 2=敲击2次 3=报警）
 * @param  sensor_ok: 传感器连接状态
 * @param  raw_range: ADC原始数据范围
 * @param  pir_motion:HC-SR501人体感应（1=有人,0=无人）
 */
void LCD_ShowMainUI(uint32_t now, uint16_t *raw, uint16_t thresh,
                    int state, int sensor_ok, uint16_t raw_range,
                    int pir_motion);

#endif /* __LCD_UI_H */
