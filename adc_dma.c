/**
 * @file    adc_dma.c
 * @brief   ADC+DMA采样模块
 * @note    ADC1通道0(PA0) + DMA1通道1 + TIM3触发, 采样率1kHz
 *          后台由DMA硬件自动完成数据采集，前台主循环读取缓冲区即可
 */

#include "adc_dma.h"

#define ADC_BUF_SIZE 512 /* DMA缓冲区大小（采样点数） */

volatile uint16_t adc_buf[ADC_BUF_SIZE]; /* ADC数据缓冲区 */
volatile uint8_t adc_data_ready = 0;     /* 数据就绪标志 */

/* ============================================================
 * ADC1 + DMA1 + TIM3 初始化
 * 采样率 = TIM3更新频率 = 1kHz
 *
 * 注意：CH32V307 APB1时钟 = HCLK/2 = 72MHz，但TIM2~5有倍频器，
 *       实际定时器时钟仍为72MHz（不是144MHz），所以按72MHz计算。
 * ============================================================ */
void ADC_DMA_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    ADC_InitTypeDef ADC_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_InitStructure = {0};

    /* 第1步：开启时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* 第2步：配置PA0为模拟输入（ADC1通道0） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; /* 模拟输入 */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 第3步：配置TIM3，产生1kHz更新事件 */
    /* APB1 = 72MHz，TIM3时钟 = 72MHz */
    /* 预分频72-1 → 计数频率1MHz，重装载1000-1 → 更新频率1kHz */
    TIM_InitStructure.TIM_Prescaler = 72 - 1;
    TIM_InitStructure.TIM_Period = 1000 - 1;
    TIM_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_InitStructure);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update); /* 更新事件触发ADC */

    /* 第4步：配置DMA1通道1（ADC1固定使用DMA1通道1） */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)adc_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; /* 循环模式 */
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    /* 使能DMA传输完成中断 */
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    /* NVIC 使能 DMA1 通道1 中断 */
    {
        NVIC_InitTypeDef nvic = {0};
        nvic.NVIC_IRQChannel = DMA1_Channel1_IRQn;
        nvic.NVIC_IRQChannelPreemptionPriority = 0;
        nvic.NVIC_IRQChannelSubPriority = 0;
        nvic.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&nvic);
    }

    /* 第5步：配置ADC1 */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; /* 单次转换，由TIM触发 */
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    ADC_DMACmd(ADC1, ENABLE);       /* 使能ADC的DMA请求 */
    ADC_ExternalTrigConvCmd(ADC1, ENABLE); /* 使能外部触发 */
    ADC_Cmd(ADC1, ENABLE);

    /* 第6步：ADC校准（上电后必须执行一次，加超时防止死循环） */
    ADC_ResetCalibration(ADC1);
    {
        volatile uint32_t timeout = 100000;
        while (ADC_GetResetCalibrationStatus(ADC1) && --timeout);
    }
    ADC_StartCalibration(ADC1);
    {
        volatile uint32_t timeout = 100000;
        while (ADC_GetCalibrationStatus(ADC1) && --timeout);
    }

    /* 第7步：启动DMA与TIM3 */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

/* ============================================================
 * 获取最新一批ADC数据（供主循环调用）
 * 返回值：当前缓冲区指针，数据长度ADC_BUF_SIZE
 * ============================================================ */
uint16_t* ADC_GetData(void)
{
    adc_data_ready = 0;
    return (uint16_t*)adc_buf;
}

/* ============================================================
 * DMA1 通道1 中断处理（防止未处理中断导致崩溃）
 * ============================================================ */
void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        adc_data_ready = 1;
    }
}
