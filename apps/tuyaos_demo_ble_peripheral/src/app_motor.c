/**
 * @file app_motor.c
 * @brief 电机控制实现 — 游戏模式时序序列状态机
 * @version 1.0
 * @date 2026-06-15
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * 控制逻辑：
 *   M_INA high, M_INB low  → 正转
 *   M_INA low,  M_INB high → 反转
 *   M_INA low,  M_INB low  → 停止
 *
 * 快/慢模式：两者时序相同，差异在旋转速度（快模式高速，慢模式低速）。
 *   序列（12步循环）：
 *   正转7.2S → 停止5.2S → 反转5.2S → 停止3S → 正转3S → 停止7.2S
 *   → 反转7.2S → 停止3S → 正转3S → 停止5.2S → 反转7.2S → 停止7.2S
 *
 * 互动模式：振动激活一次完整序列（6步后停止）：
 *   正转7.2S → 停止5.2S → 反转5.2S → 停止3S → 正转3S → 停止5S
 *   旋转期间振动不激活，结束后振动可再次激活。
 */

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_gpio.h"
#include "board.h"

#include "app_motor.h"

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/

/** 快/慢模式时序序列 — 12步循环 */
#define MOTOR_SEQ_FAST_STEPS    12

/** 互动模式时序序列 — 6步单次 */
#define MOTOR_SEQ_INTERACTIVE_STEPS  6

/***********************************************************************
 ********************* static variable *********************************
 **********************************************************************/

/* 当前游戏模式 */
STATIC game_mode_t  s_game_mode = GAME_MODE_FAST;

/* 电机是否使能（由电源状态控制） */
STATIC BOOL_T       s_motor_enabled = FALSE;

/* 时序序列定时器 */
STATIC TIMER_ID     s_motor_timer_id = NULL;

/* 当前序列步进索引 */
STATIC UINT8_T      s_seq_index = 0;

/* 互动模式：序列是否已完整执行一次 */
STATIC BOOL_T       s_interactive_active = FALSE;

/* 互动模式：旋转期间锁定（防止振动重复触发） */
STATIC BOOL_T       s_rotation_locked = FALSE;

/***********************************************************************
 ********************* motor sequence tables ***************************
 **********************************************************************/

/**
 * 快/慢模式时序表（12步）
 * 注意：快和慢的时序相同，区别在于电机旋转速度。
 * 此处以相同时序实现，如需区分速度可使用 PWM 控制占空比。
 */
STATIC const motor_step_t g_motor_seq_fast[MOTOR_SEQ_FAST_STEPS] = {
    {MOTOR_DIR_FORWARD, 7200},
    {MOTOR_DIR_STOP,    5200},
    {MOTOR_DIR_REVERSE, 5200},
    {MOTOR_DIR_STOP,    3000},
    {MOTOR_DIR_FORWARD, 3000},
    {MOTOR_DIR_STOP,    7200},
    {MOTOR_DIR_REVERSE, 7200},
    {MOTOR_DIR_STOP,    3000},
    {MOTOR_DIR_FORWARD, 3000},
    {MOTOR_DIR_STOP,    5200},
    {MOTOR_DIR_REVERSE, 7200},
    {MOTOR_DIR_STOP,    7200},
};

/**
 * 互动模式时序表（6步单次）— 最后停止5S，不同于快/慢的7.2S
 */
STATIC const motor_step_t g_motor_seq_interactive[MOTOR_SEQ_INTERACTIVE_STEPS] = {
    {MOTOR_DIR_FORWARD, 7200},
    {MOTOR_DIR_STOP,    5200},
    {MOTOR_DIR_REVERSE, 5200},
    {MOTOR_DIR_STOP,    3000},
    {MOTOR_DIR_FORWARD, 3000},
    {MOTOR_DIR_STOP,    5000},  /* 最后停止 5S */
};

/***********************************************************************
 ********************* static function prototypes **********************
 **********************************************************************/

STATIC VOID_T app_motor_set_direction(UINT8_T direction);
STATIC VOID_T app_motor_seq_timeout_handler(TIMER_ID timer_id, VOID_T *arg);
STATIC VOID_T app_motor_seq_start(VOID_T);
STATIC VOID_T app_motor_seq_stop(VOID_T);

/***********************************************************************
 ********************* static functions *********************************
 **********************************************************************/

/**
 * @brief 设置电机旋转方向（GPIO 电平控制 H 桥）
 * @param[in] direction MOTOR_DIR_STOP/FORWARD/REVERSE
 */
STATIC VOID_T app_motor_set_direction(UINT8_T direction)
{
    switch (direction) {
        case MOTOR_DIR_FORWARD:
            /* M_INA=H, M_INB=L → 正转 */
            tal_gpio_write(M_INA, TUYA_GPIO_LEVEL_HIGH);
            tal_gpio_write(M_INB, TUYA_GPIO_LEVEL_LOW);
            TAL_PR_DEBUG("[motor] -> FORWARD");
            break;

        case MOTOR_DIR_REVERSE:
            /* M_INA=L, M_INB=H → 反转 */
            tal_gpio_write(M_INA, TUYA_GPIO_LEVEL_LOW);
            tal_gpio_write(M_INB, TUYA_GPIO_LEVEL_HIGH);
            TAL_PR_DEBUG("[motor] -> REVERSE");
            break;

        case MOTOR_DIR_STOP:
        default:
            /* M_INA=L, M_INB=L → 停止 */
            tal_gpio_write(M_INA, TUYA_GPIO_LEVEL_LOW);
            tal_gpio_write(M_INB, TUYA_GPIO_LEVEL_LOW);
            TAL_PR_DEBUG("[motor] -> STOP");
            break;
    }
}

/**
 * @brief 时序定时器回调 — 执行下一步
 *
 * 每步执行：
 *   1. 若已停止（非首次），先自增索引
 *   2. 设置方向
 *   3. 重启定时器为本步持续时间
 */
STATIC VOID_T app_motor_seq_timeout_handler(TIMER_ID timer_id, VOID_T *arg)
{
    UINT16_T duration_ms;
    UINT8_T  direction;
    UINT8_T  total_steps;

    /* 互动模式使用自己的时序表 */
    if (s_game_mode == GAME_MODE_INTERACTIVE) {
        total_steps = MOTOR_SEQ_INTERACTIVE_STEPS;
    } else {
        total_steps = MOTOR_SEQ_FAST_STEPS;
    }

    /* 获取当前步 */
    if (s_game_mode == GAME_MODE_INTERACTIVE) {
        direction  = g_motor_seq_interactive[s_seq_index].direction;
        duration_ms = g_motor_seq_interactive[s_seq_index].duration_ms;
    } else {
        direction  = g_motor_seq_fast[s_seq_index].direction;
        duration_ms = g_motor_seq_fast[s_seq_index].duration_ms;
    }

    /* 更新旋转锁定状态（互动模式用） */
    if (s_game_mode == GAME_MODE_INTERACTIVE) {
        s_rotation_locked = (direction != MOTOR_DIR_STOP);
    }

    /* 设置方向 */
    app_motor_set_direction(direction);

    /* 前进到下一步 */
    s_seq_index++;

    /* 判断是否循环或停止 */
    if (s_seq_index >= total_steps) {
        if (s_game_mode == GAME_MODE_INTERACTIVE) {
            /* 互动模式：一次序列完成，停止 */
            s_interactive_active = FALSE;
            s_rotation_locked = FALSE;
            TAL_PR_INFO("[motor] interactive sequence complete");
            return;
        }
        /* 快/慢模式：从头循环 */
        s_seq_index = 0;
    }

    /* 如果还有下一步，获取其持续时间重启定时器 */
    /* 注意：休息间隔是上一步 stop 的持续时间，所以这里用本步的 duration */
    /* 实际上我们需要的是"执行当前步并等待其完成"，所以用当前步的 duration */
    tal_sw_timer_start(s_motor_timer_id, duration_ms, TAL_TIMER_ONCE);
}

/**
 * @brief 启动时序序列（从第一步开始）
 */
STATIC VOID_T app_motor_seq_start(VOID_T)
{
    UINT16_T first_duration;
    UINT8_T  first_dir;

    if (s_motor_timer_id == NULL) {
        TAL_PR_ERR("[motor] timer not initialized");
        return;
    }

    s_seq_index = 0;

    /* 从时序表第一步开始 */
    if (s_game_mode == GAME_MODE_INTERACTIVE) {
        first_dir       = g_motor_seq_interactive[0].direction;
        first_duration  = g_motor_seq_interactive[0].duration_ms;
        s_interactive_active = TRUE;
        s_rotation_locked = TRUE;
    } else {
        first_dir       = g_motor_seq_fast[0].direction;
        first_duration  = g_motor_seq_fast[0].duration_ms;
    }

    app_motor_set_direction(first_dir);
    s_seq_index = 1;

    TAL_PR_INFO("[motor] sequence started, mode=%d", s_game_mode);

    /* 启动第一步的定时器 */
    tal_sw_timer_start(s_motor_timer_id, first_duration, TAL_TIMER_ONCE);
}

/**
 * @brief 停止时序序列
 */
STATIC VOID_T app_motor_seq_stop(VOID_T)
{
    /* 停止定时器 */
    if (s_motor_timer_id != NULL) {
        tal_sw_timer_stop(s_motor_timer_id);
    }

    /* 电机停止 */
    app_motor_set_direction(MOTOR_DIR_STOP);

    /* 清除状态 */
    s_seq_index = 0;
    s_interactive_active = FALSE;
    s_rotation_locked = FALSE;

    TAL_PR_INFO("[motor] sequence stopped");
}

/***********************************************************************
 ********************* public functions *********************************
 **********************************************************************/

VOID_T app_motor_init(VOID_T)
{
    /* 创建时序定时器（不自动启动） */
    tal_sw_timer_create(app_motor_seq_timeout_handler, NULL, &s_motor_timer_id);

    s_game_mode      = GAME_MODE_FAST;
    s_motor_enabled  = FALSE;
    s_seq_index      = 0;
    s_interactive_active = FALSE;
    s_rotation_locked    = FALSE;

    TAL_PR_INFO("[motor] initialized");
}

VOID_T app_motor_set_mode(game_mode_t mode)
{
    if (mode > GAME_MODE_INTERACTIVE) {
        TAL_PR_ERR("[motor] invalid mode: %d", mode);
        return;
    }

    /* 如果正在运行，先停止 */
    if (s_motor_enabled) {
        app_motor_seq_stop();
    }

    s_game_mode = mode;
    TAL_PR_INFO("[motor] mode set to %d", mode);
}

game_mode_t app_motor_get_mode(VOID_T)
{
    return s_game_mode;
}

VOID_T app_motor_start(VOID_T)
{
    if (!s_motor_enabled) {
        s_motor_enabled = TRUE;
        app_motor_seq_start();
        TAL_PR_INFO("[motor] started");
    }
}

VOID_T app_motor_stop(VOID_T)
{
    if (s_motor_enabled) {
        app_motor_seq_stop();
        s_motor_enabled = FALSE;
        TAL_PR_INFO("[motor] stopped");
    }
}

VOID_T app_motor_interactive_trigger(VOID_T)
{
    /* 仅互动模式下 && 已使能 && 未激活 && 不在旋转锁定期间 才触发 */
    if (s_game_mode != GAME_MODE_INTERACTIVE) {
        return;
    }
    if (!s_motor_enabled) {
        return;
    }
    if (s_interactive_active) {
        TAL_PR_DEBUG("[motor] interactive already active, ignore");
        return;
    }
    if (s_rotation_locked) {
        TAL_PR_DEBUG("[motor] rotation locked, cannot trigger");
        return;
    }

    TAL_PR_INFO("[motor] interactive triggered by vibration");
    app_motor_seq_start();
}

BOOL_T app_motor_is_running(VOID_T)
{
    /* 电机正在旋转：当前方向非 STOP 且已使能 */
    if (!s_motor_enabled) {
        return FALSE;
    }
    /* 若互动模式未激活，不算运行 */
    if (s_game_mode == GAME_MODE_INTERACTIVE && !s_interactive_active) {
        return FALSE;
    }
    /* 检查是否有定时器在运行 */
    return (s_seq_index > 0);
}
