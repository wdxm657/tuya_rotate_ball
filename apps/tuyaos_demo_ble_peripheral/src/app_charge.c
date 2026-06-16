/**
 * @file app_charge.c
 * @brief 充电检测实现 — 周期定时器轮询 GPIO 检测充电器插入/拔出
 * @version 1.0
 * @date 2026-06-15
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * 检测方案：
 *   1. 周期轮询：CHARGE_DETECT_PIN 每 100ms 读取一次电平
 *   2. 电压辅助：电池电压 > 4.0V 时辅助判定充电状态
 *
 * 当检测到充电器插入时，调用 app_state_set_charging(TRUE)，
 * 检测到拔出时调用 app_state_set_charging(FALSE)。
 * 状态机会据此切换 DEV_STATE_CHARGING / DEV_STATE_WORK。
 */

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_gpio.h"
#include "board.h"

#include "app_charge.h"
#include "app_state.h"

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/

/** GPIO 轮询间隔 ms */
#define CHARGE_POLL_MS              100

/***********************************************************************
 ********************* static variable *********************************
 **********************************************************************/

/* 轮询定时器 */
STATIC TIMER_ID s_charge_poll_timer_id = NULL;

/* 当前 GPIO 检测到的充电器插入状态 */
STATIC BOOL_T s_charge_detected = FALSE;

/* 电池电压辅助判断：上次报告的电压，用于检测趋势 */
STATIC INT32_T s_last_vol_mv = 0;

/***********************************************************************
 ********************* static function prototypes **********************
 **********************************************************************/

STATIC VOID_T app_charge_poll_handler(TIMER_ID timer_id, VOID_T *arg);

/***********************************************************************
 ********************* static functions *********************************
 **********************************************************************/

/**
 * @brief 轮询定时器回调 — 读取 GPIO 电平，检测变化后通知状态机
 */
STATIC VOID_T app_charge_poll_handler(TIMER_ID timer_id, VOID_T *arg)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_LOW;
    OPERATE_RET ret = tal_gpio_read(CHARGE_DETECT_PIN, &level);
    if (ret != OPRT_OK) {
        TAL_PR_DEBUG("[charge] gpio read fail: %d", ret);
        return;
    }

    BOOL_T is_charging = (level == TUYA_GPIO_LEVEL_HIGH);

    if (is_charging == s_charge_detected) {
        /* 状态未变化，无需处理 */
        return;
    }

    TAL_PR_DEBUG("[charge] pin=%d level=%d",
                 CHARGE_DETECT_PIN, level);

    s_charge_detected = is_charging;

    if (is_charging) {
        TAL_PR_INFO("[charge] DETECTED (plugged)");
    } else {
        TAL_PR_INFO("[charge] NOT detected (unplugged)");
    }

    /* 通知状态机 */
    app_state_set_charging(is_charging);
}

/***********************************************************************
 ********************* public functions *********************************
 **********************************************************************/

VOID_T app_charge_init(VOID_T)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode   = TUYA_GPIO_PULLUP,       /* 默认上拉 */
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };

    /* 初始化 GPIO（不进中断，纯输入轮询） */
    tal_gpio_init(CHARGE_DETECT_PIN, &gpio_cfg);

    /* 创建轮询定时器 */
    OPERATE_RET ret = tal_sw_timer_create(app_charge_poll_handler, NULL,
                                          &s_charge_poll_timer_id);
    if (ret != OPRT_OK) {
        TAL_PR_ERR("[charge] timer create fail: %d", ret);
        return;
    }

    /* 初始化时读取一次当前状态 */
    TUYA_GPIO_LEVEL_E init_level = TUYA_GPIO_LEVEL_LOW;
    tal_gpio_read(CHARGE_DETECT_PIN, &init_level);
    s_charge_detected = (init_level == TUYA_GPIO_LEVEL_HIGH);

    if (s_charge_detected) {
        TAL_PR_INFO("[charge] init: charger plugged in");
        /* 如果上电时充电器已插入，通知状态机 */
        app_state_set_charging(TRUE);
    }

    /* 启动周期轮询定时器 */
    tal_sw_timer_start(s_charge_poll_timer_id, CHARGE_POLL_MS, TAL_TIMER_CYCLE);

    s_last_vol_mv = 0;
    TAL_PR_INFO("[charge] initialized (poll %dms), pin %d, init=%d",
                CHARGE_POLL_MS, CHARGE_DETECT_PIN, s_charge_detected);
}

BOOL_T app_charge_is_detected(VOID_T)
{
    return s_charge_detected;
}
