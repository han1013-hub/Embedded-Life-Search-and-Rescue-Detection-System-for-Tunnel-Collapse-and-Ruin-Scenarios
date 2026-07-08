/**
 * @file    pattern.c
 * @brief   模式识别模块（3次敲击判定）
 * @note    维护5秒滑动窗口，窗口内检测到3次敲击即触发报警
 *          状态机：STATE_IDLE → STATE_TAP1 → STATE_TAP2 → STATE_ALARM
 */

#include "pattern.h"
#include "ch32v30x.h"

#define TAP_WINDOW_MS   5000    /* 敲击时间窗口（5秒） */
#define ALARM_DURATION  3000    /* 报警持续时长（3秒） */
#define TAP_TARGET      3       /* 目标敲击次数 */

typedef enum {
    STATE_IDLE = 0,  /* 空闲监测 */
    STATE_TAP1,      /* 已检测到1次敲击 */
    STATE_TAP2,      /* 已检测到2次敲击 */
    STATE_ALARM      /* 报警中 */
} SysState;

static SysState s_state = STATE_IDLE;
static uint32_t s_last_tap_tick = 0;     /* 最近一次敲击时刻 */
static uint32_t s_first_tap_tick = 0;    /* 窗口内首次敲击时刻 */
static uint32_t s_alarm_start_tick = 0;  /* 报警开始时刻 */
static int s_tap_count = 0;              /* 当前窗口内敲击次数 */

/* ============================================================
 * 模式识别状态机
 * 输入：peak_count（本次扫描新检测到的敲击数）
 *       now_tick（当前系统毫秒时刻）
 * 返回：1表示触发报警，0表示未触发
 * ============================================================ */
int Pattern_Update(int peak_count, uint32_t now_tick)
{
    int alarm_trigger = 0;

    /* 检查窗口是否超时：从首次敲击开始5秒内有效 */
    if (s_state != STATE_IDLE && s_state != STATE_ALARM) {
        if ((now_tick - s_first_tap_tick) > TAP_WINDOW_MS) {
            /* 超时，完全复位 */
            s_state = STATE_IDLE;
            s_tap_count = 0;
            s_first_tap_tick = 0;
            s_last_tap_tick = 0;
        }
    }

    /* 处理报警结束 */
    if (s_state == STATE_ALARM) {
        if ((now_tick - s_alarm_start_tick) > ALARM_DURATION) {
            /* ★★★ 报警结束后完全复位所有状态变量 ★★★ */
            s_state = STATE_IDLE;
            s_tap_count = 0;
            s_first_tap_tick = 0;
            s_last_tap_tick = 0;
            s_alarm_start_tick = 0;
        }
        return 0;
    }

    /* 有新敲击时更新状态 */
    if (peak_count > 0) {
        s_tap_count++;
        s_last_tap_tick = now_tick;

        if (s_tap_count == 1) {
            s_first_tap_tick = now_tick;
            s_state = STATE_TAP1;
        } else if (s_tap_count == 2) {
            s_state = STATE_TAP2;
        } else if (s_tap_count >= TAP_TARGET) {
            /* 检测到第3次敲击，触发报警 */
            s_state = STATE_ALARM;
            s_alarm_start_tick = now_tick;
            alarm_trigger = 1;
        }
    }

    return alarm_trigger;
}

/* ============================================================
 * 手动触发报警（用于串口调试命令）
 * ============================================================ */
void Pattern_ForceAlarm(uint32_t now_tick)
{
    s_state = STATE_ALARM;
    s_alarm_start_tick = now_tick;
    s_tap_count = TAP_TARGET;
}

/* ============================================================
 * 强制复位模式识别状态机
 * ============================================================ */
void Pattern_Reset(void)
{
    s_state = STATE_IDLE;
    s_tap_count = 0;
    s_first_tap_tick = 0;
    s_last_tap_tick = 0;
    s_alarm_start_tick = 0;
}

/* ============================================================
 * 获取当前系统状态（供OLED显示用）
 * ============================================================ */
int Pattern_GetState(void)
{
    return (int)s_state;
}