/**
 * @file    signal_proc.h
 * @brief   信号处理模块头文件
 * @note    滑动平均滤波 + 去直流 + 绝对值包络 + 包络平滑 + 动态阈值 + 峰值检测
 */

#ifndef __SIGNAL_PROC_H
#define __SIGNAL_PROC_H

#include "ch32v30x.h"

/**
 * @brief  第1步：滑动平均滤波 + 去直流
 * @param  raw: 原始ADC数据指针
 * @param  n:   数据长度
 */
void Signal_Filter(uint16_t *raw, int n);

/**
 * @brief  第2步：绝对值包络
 * @param  n: 数据长度
 */
void Signal_Envelope(int n);

/**
 * @brief  第3步：包络滑动平均平滑
 * @param  n: 数据长度
 */
void Signal_Smooth(int n);

/**
 * @brief  第4步：动态阈值计算
 * @param  n: 数据长度
 * @retval 当前背景噪声对应的检测阈值
 */
uint16_t Signal_CalcThreshold(int n);

/**
 * @brief  第5步：峰值检测（带去抖）
 * @param  threshold:  检测阈值
 * @param  now_tick:   当前系统毫秒时刻
 * @param  n:          数据长度
 * @retval 本次扫描检测到的敲击次数（0或1）
 */
int Signal_DetectPeak(uint16_t threshold, uint32_t now_tick, int n);

/**
 * @brief  获取处理后的包络数据（供OLED显示波形用）
 * @retval 平滑后包络数据指针
 */
uint16_t* Signal_GetEnvelope(void);

/**
 * @brief  设置固定检测阈值
 * @param  thr: 阈值（ADC差值），设为0则使用动态阈值
 */
void Signal_SetThreshold(uint16_t thr);

/**
 * @brief  获取当前固定检测阈值
 * @retval 当前阈值
 */
uint16_t Signal_GetThreshold(void);

#endif /* __SIGNAL_PROC_H */
