/**
 * @file app_key.c
 * @brief 按键实现 — 周期轮询检测，短按开关机，长按3s松开恢复出厂
 * @version 1.1
 * @date 2026-06-15
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * 按键物理引脚：C1 (TUYA_GPIO_NUM_17)，低电平有效（按下=低）
 *
 * 行为：
 *   短按（<3s 松开）-> 开关机（切换低功耗模式）
 *   长按（>=3s 松开）-> 蓝牙恢复出厂设置
 *
 * tal_key 状态说明（由 tal_key 库驱动，10ms 周期轮询）：
 *   count_array = {5, 300, 500} -> {50ms, 3000ms, 5000ms}
 *   case 1: 短按检测到（50ms去抖后）
 *   case 2: 长按阈值到达（3000ms）
 *   case 3: 超长超时（5000ms）
 *   case 5: 短按松开
 *   case 6: 长按松开
 */

#include "board.h"

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_gpio.h"
#include "tal_key.h"

#include "tuya_ble_api.h"
#include "tuya_ble_protocol_callback.h"

#include "app_key.h"
#include "app_state.h"

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/

/** 按键引脚 */
#define APP_KEY_PIN BOARD_KEY_PIN

/** 按键轮询间隔 ms */
#define KEY_POLL_MS                 10

/***********************************************************************
 ********************* static variable *********************************
 **********************************************************************/

/* 按键轮询定时器（10ms 周期，一直运行） */
STATIC TIMER_ID s_app_key_timer_id = NULL;

/* 标记：是否已达到长按阈值（用于区分短按松/长按松） */
STATIC BOOL_T s_long_press_triggered = FALSE;

/***********************************************************************
 ********************* static functions *********************************
 **********************************************************************/

/**
 * @brief tal_key 状态回调
 * @param[in] state 按键状态码
 */
STATIC VOID_T app_key_handler(UINT32_T state)
{
    switch (state) {
        /* 短按检测（50ms 去抖通过） */
        case 1: {
            s_long_press_triggered = FALSE;
        } break;

        /* 长按阈值到达（3000ms） */
        case 2: {
            s_long_press_triggered = TRUE;
            TAL_PR_INFO("[key] long press 3s detected");
        } break;

        /* 超长超时（5000ms）— 暂不处理 */
        case 3: {
        } break;

        /* 短按松开 */
        case 5: {
            if (!s_long_press_triggered) {
                TAL_PR_INFO("[key] short press release -> toggle power");
                app_state_toggle_power();
            }
        } break;

        /* 长按松开 */
        case 6: {
            if (s_long_press_triggered) {
                TAL_PR_INFO("[key] long press release -> factory reset");
                tuya_ble_device_factory_reset();
                tuya_ble_disconnect_and_reset_timer_start();
            }
        } break;

        default:
            break;
    }
}

/**
 * @brief 设置 tal_key 按键参数
 */
STATIC tal_key_param_t key_press_param = {
    .pin         = APP_KEY_PIN,
    .valid_level = TUYA_KEY_LEVEL_LOW,
    .count_len   = 3,
    .count_array = {5, 300, 500},  /* {50ms, 3000ms, 5000ms} */
    .handler     = app_key_handler,
};

/**
 * @brief 轮询定时器回调 — 驱动 tal_key 状态机
 *
 * 每 10ms 读取一次按键电平，tal_key 内部处理去抖和长短按判定。
 * 不再依赖 GPIO 中断触发，定时器一直运行。
 */
STATIC VOID_T app_key_poll_handler(TIMER_ID timer_id, VOID_T *arg)
{
    tal_key_timeout_handler(&key_press_param);
}

/***********************************************************************
 ********************* public functions *********************************
 **********************************************************************/

VOID_T app_key_init(VOID_T)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode   = TUYA_GPIO_PULLUP,         /* 上拉输入 */
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };

    /* 初始化 GPIO（不进中断，纯输入轮询） */
    tal_gpio_init(APP_KEY_PIN, &gpio_cfg);

    /* 初始化 tal_key 库 */
    tal_key_init(&key_press_param);

    /* 创建并启动 10ms 周期轮询定时器 */
    tal_sw_timer_create(app_key_poll_handler, NULL, &s_app_key_timer_id);
    tal_sw_timer_start(s_app_key_timer_id, KEY_POLL_MS, TAL_TIMER_CYCLE);

    s_long_press_triggered = FALSE;

    TAL_PR_INFO("[key] initialized (poll %dms), pin %d", KEY_POLL_MS, APP_KEY_PIN);
}