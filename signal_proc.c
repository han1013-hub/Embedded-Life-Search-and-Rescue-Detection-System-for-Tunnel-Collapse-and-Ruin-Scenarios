/**
 * @file    signal_proc.c
 * @brief   信号处理模块
 * @note    依次执行：滑动平均滤波、去直流、绝对值包络、包络平滑、
 *          动态阈值计算、峰值检测（上升沿跳变检测）
 */

#include "signal_proc.h"
#include <string.h>

#define ADC_BUF_SIZE    512
#define MA_FILTER_LEN   10      /* 滑动平均滤波窗口 */
#define ENV_SMOOTH_LEN  20      /* 包络平滑窗口 */
#define NOISE_WIN_LEN   256     /* 噪声估计窗口 */
#define DEBOUNCE_MS     200     /* 峰值去抖（毫秒）— 两次敲击至少间隔200ms */
#define THRESH_K        5       /* 阈值 = RMS × 5 + OFFSET，对噪声强抑制 */
#define THRESH_OFFSET   100     /* 阈值固定偏置 */
#define THRESH_MIN      150     /* 阈值绝对下限 */

/* 静态缓冲区（避免动态分配） */
static int16_t  s_filtered[ADC_BUF_SIZE];  /* 滤波后信号 */
static uint16_t s_envelope[ADC_BUF_SIZE];  /* 包络 */
static uint16_t s_smoothed[ADC_BUF_SIZE];  /* 平滑后包络 */

/* ============================================================
 * 第1步：滑动平均滤波 + 去直流
 * ============================================================ */
void Signal_Filter(uint16_t *raw, int n)
{
    uint32_t sum = 0;
    uint32_t dc_sum = 0;
    int i;

    /* 动态计算DC偏置 */
    for (i = 0; i < n; i++)
        dc_sum += raw[i];
    uint16_t dc_offset = (uint16_t)(dc_sum / n);

    /* 初始化滑动窗口和 */
    for (i = 0; i < MA_FILTER_LEN && i < n; i++)
        sum += raw[i];

    for (i = 0; i < n; i++) {
        if (i + MA_FILTER_LEN < n) {
            sum += raw[i + MA_FILTER_LEN];
        }
        if (i > 0) {
            sum -= raw[i - 1];
        }
        s_filtered[i] = (int16_t)(sum / MA_FILTER_LEN) - (int16_t)dc_offset;
    }
}

/* ============================================================
 * 第2步：绝对值包络
 * ============================================================ */
void Signal_Envelope(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int16_t v = s_filtered[i];
        s_envelope[i] = (v < 0) ? (uint16_t)(-v) : (uint16_t)v;
    }
}

/* ============================================================
 * 第3步：包络滑动平均平滑
 * ============================================================ */
void Signal_Smooth(int n)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < ENV_SMOOTH_LEN && i < n; i++)
        sum += s_envelope[i];

    for (i = 0; i < n; i++) {
        if (i + ENV_SMOOTH_LEN < n) {
            sum += s_envelope[i + ENV_SMOOTH_LEN];
        }
        if (i > 0) {
            sum -= s_envelope[i - 1];
        }
        s_smoothed[i] = (uint16_t)(sum / ENV_SMOOTH_LEN);
    }
}

/* ============================================================
 * 第4步：阈值计算
 * ============================================================ */
static uint16_t s_fixed_threshold = 500;

void Signal_SetThreshold(uint16_t thr)
{
    s_fixed_threshold = thr;
}

uint16_t Signal_GetThreshold(void)
{
    return s_fixed_threshold;
}

uint16_t Signal_CalcThreshold(int n)
{
    if (s_fixed_threshold > 0) {
        return s_fixed_threshold;
    }

    uint32_t sum_sq = 0;
    int i;
    int win = (n < NOISE_WIN_LEN) ? n : NOISE_WIN_LEN;

    for (i = n - win; i < n; i++) {
        int16_t v = s_filtered[i];
        sum_sq += (uint32_t)v * (uint32_t)v;
    }

    uint16_t rms = 0;
    uint16_t bit = 1 << 14;
    uint32_t x = sum_sq / win;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= rms + bit) {
            x -= rms + bit;
            rms = (rms >> 1) + bit;
        } else {
            rms >>= 1;
        }
        bit >>= 2;
    }

    uint32_t thr = (uint32_t)rms * THRESH_K + THRESH_OFFSET;
    if (thr < THRESH_MIN) thr = THRESH_MIN;
    if (thr > 65535) thr = 65535;
    return (uint16_t)thr;
}

/* ============================================================
 * ★★★ 第5步：峰值检测（上升沿跳变检测）★★★
 * 
 * 原理：不依赖绝对阈值，而是检测包络信号中是否存在
 *       突然的上升沿跳变（从低到高的变化）。
 * 
 * 优点：即使静态干扰很大（如50Hz工频），只要它变化缓慢，
 *       就不会被误判为敲击。敲击的特点是：幅度突然增大。
 * ============================================================ */
static uint32_t s_last_peak_tick = 0;

int Signal_DetectPeak(uint16_t threshold, uint32_t now_tick, int n)
{
    int i;
    int peak_index = -1;
    int avg_before = 0;
    int avg_after = 0;
    int samples = 8;

    /* 如果 threshold = 0，用默认值 200 */
    if (threshold == 0) threshold = 200;

    /* ★★★ 核心检测逻辑：寻找上升沿跳变 ★★★ */
    for (i = 10; i < n - 10; i++) {
        /* 计算当前位置之前 samples 个点的平均值（背景基线） */
        avg_before = 0;
        for (int j = i - samples; j < i; j++) {
            avg_before += s_smoothed[j];
        }
        avg_before /= samples;

        /* 计算当前位置之后 samples 个点的平均值（峰值区域） */
        avg_after = 0;
        for (int j = i; j < i + samples; j++) {
            avg_after += s_smoothed[j];
        }
        avg_after /= samples;

        /* ★★★ 关键判断：跳变幅度是否足够大 ★★★
         * 条件1：跳变幅度 > 阈值（默认500）
         * 条件2：当前值高于背景基线 + 偏移（防止缓慢漂移被误判）
         */
        if ((avg_after - avg_before) > threshold) {
            peak_index = i;
            break;
        }
    }

    /* 如果检测到跳变，检查去抖时间 */
    if (peak_index >= 0) {
        if ((now_tick - s_last_peak_tick) > DEBOUNCE_MS) {
            s_last_peak_tick = now_tick;
            return 1;
        }
    }

    return 0;
}

/* ============================================================
 * 获取处理后的包络数据（供OLED显示波形用）
 * ============================================================ */
uint16_t* Signal_GetEnvelope(void)
{
    return s_smoothed;
}