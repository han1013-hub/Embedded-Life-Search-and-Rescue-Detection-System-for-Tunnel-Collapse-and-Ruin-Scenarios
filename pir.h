/**
 * @file    pir.h
 * @brief   HC-SR501 人体红外感应模块
 * @note    OUT接PA4，HIGH=有人，LOW=无人
 */

#ifndef __PIR_H
#define __PIR_H

#include "ch32v30x.h"

/* 将引脚从 PE5 改为 PA4 */
#define PIR_GPIO_PORT      GPIOA
#define PIR_GPIO_PIN       GPIO_Pin_4

void PIR_Init(void);
int PIR_Check(void);    /* 1=有人, 0=无人 */

#endif /* __PIR_H */