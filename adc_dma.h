/**
 * @file    adc_dma.h
 * @brief   ADC+DMA采样模块头文件
 * @note    ADC1通道0(PA0) + DMA1通道1 + TIM3触发, 采样率1kHz
 */

#ifndef __ADC_DMA_H
#define __ADC_DMA_H

#include "ch32v30x.h"

#define ADC_BUF_SIZE  512    /* DMA缓冲区大小（采样点数） */

/**
 * @brief  ADC1 + DMA1 + TIM3 初始化
 *         采样率 = TIM3更新频率 = 1kHz
 */
void ADC_DMA_Init(void);

/**
 * @brief  获取最新一批ADC数据
 * @retval 当前缓冲区指针，数据长度 ADC_BUF_SIZE
 */
uint16_t* ADC_GetData(void);

#endif /* __ADC_DMA_H */
