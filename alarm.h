/**
 * @file    alarm.h
 * @brief   报警控制模块头文件
 * @note    蜂鸣器(PD11)、绿色LED(PD13)、红色LED(PD12)
 *          板载蜂鸣器和LED，GPIO已在板子上引出
 */

#ifndef __ALARM_H
#define __ALARM_H

#include "ch32v30x.h"

/**
 * @brief  报警模块初始化（GPIO配置）
 *         蜂鸣器、LED均为板载，只需配置GPIO输出模式
 */
void Alarm_Init(void);

/**
 * @brief  触发报警（蜂鸣器开 + 红灯亮）
 */
void Alarm_On(void);

/**
 * @brief  解除报警（蜂鸣器关 + 绿灯亮 + 红灯灭）
 */
void Alarm_Off(void);

/**
 * @brief  红灯闪烁（报警过程中调用）
 */
void Alarm_ToggleRed(void);

/**
 * @brief  绿灯点亮（心跳指示）
 */
void Alarm_GreenOn(void);

/**
 * @brief  绿灯熄灭
 */
void Alarm_GreenOff(void);

#endif /* __ALARM_H */
