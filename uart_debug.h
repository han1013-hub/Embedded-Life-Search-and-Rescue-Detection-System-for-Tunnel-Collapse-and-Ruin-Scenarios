/**
 * @file    uart_debug.h
 * @brief   UART1 调试串口头文件
 * @note    PA9=TX, PA10=RX, 115200 8N1
 */

#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include "ch32v30x.h"

/* 命令处理回调类型 */
typedef void (*UART_CmdHandler)(const char *cmd);

/**
 * @brief  初始化 UART1 调试串口
 */
void UART_Debug_Init(void);

/**
 * @brief  发送字符串
 * @param  str: 字符串
 */
void UART_Debug_SendString(const char *str);

/**
 * @brief  注册命令处理回调
 * @param  handler: 回调函数指针
 */
void UART_Debug_RegisterCmdHandler(UART_CmdHandler handler);

/**
 * @brief  轮询接收并处理命令
 * @note   在 main loop 中调用
 */
void UART_Debug_Poll(void);

#endif /* __UART_DEBUG_H */
