/**
 * @file    pattern.h
 * @brief   模式识别模块头文件（3次敲击判定）
 */

#ifndef __PATTERN_H
#define __PATTERN_H

#include "ch32v30x.h"

/**
 * @brief  模式识别状态机
 * @param  peak_count: 本次扫描新检测到的敲击数
 * @param  now_tick:   当前系统毫秒时刻
 * @retval 1表示触发报警，0表示未触发
 */
int Pattern_Update(int peak_count, uint32_t now_tick);

/**
 * @brief  获取当前系统状态（供OLED显示用）
 * @retval 0=空闲监测, 1=已检测到1次, 2=已检测到2次, 3=报警中
 */
int Pattern_GetState(void);

/**
 * @brief  手动触发报警（调试用）
 * @param  now_tick: 当前系统毫秒时刻
 */
void Pattern_ForceAlarm(uint32_t now_tick);

/**
 * @brief  强制复位模式识别状态机
 */
void Pattern_Reset(void);

#endif /* __PATTERN_H */
