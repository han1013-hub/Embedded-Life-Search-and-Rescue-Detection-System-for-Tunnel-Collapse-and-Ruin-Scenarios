/**
 * @file    uart_debug.c
 * @brief   UART1 调试串口模块
 * @note    PA9=TX, PA10=RX, 115200 8N1
 *          重定向 printf，并提供简单的命令行接口
 */

#include "uart_debug.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* 命令接收缓冲区 */
#define CMD_BUF_SIZE  64
static char s_cmd_buf[CMD_BUF_SIZE];
static uint8_t s_cmd_len = 0;

/* 命令回调函数指针 */
static UART_CmdHandler s_cmd_handler = 0;

/* ============================================================
 * UART1 初始化
 * ============================================================ */
void UART_Debug_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    /* 开启 GPIOA 和 USART1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 - TX 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10 - RX 浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART1 配置：115200, 8N1 */
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);

    /* 使能 USART1 */
    USART_Cmd(USART1, ENABLE);
}

/* ============================================================
 * 发送单个字符（阻塞式）
 * ============================================================ */
static void UART_Debug_PutChar(char c)
{
    USART_SendData(USART1, (uint8_t)c);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
        /* 等待发送寄存器空 */
    }
}

/* ============================================================
 * 发送字符串
 * ============================================================ */
void UART_Debug_SendString(const char *str)
{
    while (*str) {
        UART_Debug_PutChar(*str++);
    }
}

/* ============================================================
 * printf 重定向（Newlib _write）
 * 注意：项目里 Debug/debug.c 已经实现了 _write，
 *       这里不再重复定义；调用 UART_Debug_SendString 即可输出。
 *       如果需要 printf 重定向，请删除 Debug/debug.c 中的 _write。
 * ============================================================ */
// int _write(int fd, char *ptr, int len)   /* 已禁用，避免与 debug.c 冲突 */
// {
//     int i;
//     (void)fd;
//     for (i = 0; i < len; i++) {
//         UART_Debug_PutChar(ptr[i]);
//     }
//     return len;
// }

/* ============================================================
 * 注册命令处理回调
 * ============================================================ */
void UART_Debug_RegisterCmdHandler(UART_CmdHandler handler)
{
    s_cmd_handler = handler;
}

/* ============================================================
 * 轮询接收并处理命令
 * 在 main loop 中调用
 * ============================================================ */
void UART_Debug_Poll(void)
{
    char c;

    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET) {
        return;
    }

    c = (char)USART_ReceiveData(USART1);

    /* 回显 */
    UART_Debug_PutChar(c);

    if (c == '\r' || c == '\n') {
        if (s_cmd_len > 0) {
            s_cmd_buf[s_cmd_len] = '\0';
            UART_Debug_PutChar('\r');
            UART_Debug_PutChar('\n');

            if (s_cmd_handler != 0) {
                s_cmd_handler(s_cmd_buf);
            } else {
                printf("Unknown cmd: %s\r\n", s_cmd_buf);
            }

            s_cmd_len = 0;
        }
    } else if (c == '\b' || c == 0x7F) {
        /* 退格处理 */
        if (s_cmd_len > 0) {
            s_cmd_len--;
            UART_Debug_PutChar('\b');
            UART_Debug_PutChar(' ');
            UART_Debug_PutChar('\b');
        }
    } else {
        if (s_cmd_len < CMD_BUF_SIZE - 1) {
            s_cmd_buf[s_cmd_len++] = c;
        }
    }
}
