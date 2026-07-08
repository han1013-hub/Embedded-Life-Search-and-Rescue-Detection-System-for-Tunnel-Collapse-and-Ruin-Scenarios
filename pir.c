#include "pir.h"

void PIR_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;  /* 下拉输入，更稳定 */
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

int PIR_Check(void)
{
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) ? 1 : 0;
}