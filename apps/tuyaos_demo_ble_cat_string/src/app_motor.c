/**
 * @file app_motor.c
 * @brief afp pawswiff motor control.
 */

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_pwm.h"

#include "board.h"
#include "app_motor.h"

#define VARIABLE_SEQ_STEPS 3
#define FIXED_SEQ_STEPS (sizeof(s_fixed_seq) / sizeof(s_fixed_seq[0]))
#define VARIABLE_SEQ_CYCLES 5
#define RANDOM_BURST_MS 3000

STATIC work_mode_t s_work_mode = WORK_MODE_FIXED;
STATIC speed_dp_t s_speed_dp = SPEED_DP_55;
STATIC BOOL_T s_motor_enabled = FALSE;
STATIC BOOL_T s_motor_running = FALSE;
STATIC TIMER_ID s_motor_timer_id = NULL;
STATIC UINT8_T s_seq_index = 0;
STATIC UINT8_T s_var_seq_cycle_count = 0;
STATIC UINT8_T s_var_rand_dir = MOTOR_DIR_FORWARD;
STATIC UINT8_T s_var_rand_seed = 0;

typedef enum
{
    VAR_SUB_SEQ,
    VAR_SUB_RANDOM,
} var_substate_t;

STATIC var_substate_t s_var_substate = VAR_SUB_SEQ;

STATIC const UINT32_T s_level_duty[5] = {
    MOTOR_PWM_DUTY_55,
    MOTOR_PWM_DUTY_60,
    MOTOR_PWM_DUTY_65,
    MOTOR_PWM_DUTY_70,
    MOTOR_PWM_DUTY_75,
};

/** 单步：方向 + 持续时间（毫秒）。调试时序只改 s_sm_slow_fast_seq[]。 */
typedef struct
{
    UINT8_T dir;
    UINT16_T duration_ms;
} sm_simple_seq_step_t;

STATIC const sm_simple_seq_step_t s_fixed_seq[] = {
    {MOTOR_DIR_FORWARD, 500},
    {MOTOR_DIR_REVERSE, 500},
    {MOTOR_DIR_FORWARD, 500},
    {MOTOR_DIR_REVERSE, 500},
    {MOTOR_DIR_FORWARD, 2500},
    {MOTOR_DIR_REVERSE, 500},
    {MOTOR_DIR_FORWARD, 500},
    {MOTOR_DIR_REVERSE, 500},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1500},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1000},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1000},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1500},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1000},
    {MOTOR_DIR_FORWARD, 1000},
    {MOTOR_DIR_REVERSE, 1000},
    {MOTOR_DIR_STOP, 3000},
};

STATIC const sm_simple_seq_step_t s_variable_seq[VARIABLE_SEQ_STEPS] = {
    {MOTOR_DIR_FORWARD, 100},
    {MOTOR_DIR_REVERSE, 100},
    {MOTOR_DIR_STOP, 100},
};

STATIC UINT32_T app_motor_duty_for_dp(speed_dp_t dp_level)
{
    if (dp_level < SPEED_DP_55)
    {
        dp_level = SPEED_DP_55;
    }
    if (dp_level > SPEED_DP_59)
    {
        dp_level = SPEED_DP_59;
    }
    return s_level_duty[(UINT8_T)(dp_level - SPEED_DP_55)];
}

STATIC VOID_T app_motor_set_direction(UINT8_T direction, speed_dp_t dp_level)
{
    UINT32_T duty = app_motor_duty_for_dp(dp_level);

    switch (direction)
    {
    case MOTOR_DIR_FORWARD:
        tal_pwm_duty_set(MOTOR_PWM_CH_FOR, duty);
        tal_pwm_duty_set(MOTOR_PWM_CH_REV, MOTOR_PWM_DUTY_0);
        s_motor_running = TRUE;
        break;
    case MOTOR_DIR_REVERSE:
        tal_pwm_duty_set(MOTOR_PWM_CH_FOR, MOTOR_PWM_DUTY_0);
        tal_pwm_duty_set(MOTOR_PWM_CH_REV, duty);
        s_motor_running = TRUE;
        break;
    case MOTOR_DIR_STOP:
    default:
        tal_pwm_duty_set(MOTOR_PWM_CH_FOR, MOTOR_PWM_DUTY_0);
        tal_pwm_duty_set(MOTOR_PWM_CH_REV, MOTOR_PWM_DUTY_0);
        s_motor_running = FALSE;
        break;
    }
}

STATIC VOID_T app_motor_timer_handler(TIMER_ID timer_id, VOID_T *arg)
{
    if (!s_motor_enabled)
    {
        app_motor_set_direction(MOTOR_DIR_STOP, s_speed_dp);
        return;
    }

    if (s_work_mode == WORK_MODE_FIXED)
    {
        /* 固定模式：按 s_fixed_seq 表逐歩运动，占空比使用 dp 最新挡位 */
        const sm_simple_seq_step_t *step = &s_fixed_seq[s_seq_index];
        app_motor_set_direction(step->dir, s_speed_dp);
        s_seq_index++;
        if (s_seq_index >= FIXED_SEQ_STEPS)
        {
            s_seq_index = 0;
        }
        tal_sw_timer_start(s_motor_timer_id, step->duration_ms, TAL_TIMER_ONCE);
        return;
    }

    /* 随机模式 */
    if (s_var_substate == VAR_SUB_SEQ)
    {
        /* s_variable_seq 循环 5 次 */
        const sm_simple_seq_step_t *step = &s_variable_seq[s_seq_index];
        app_motor_set_direction(step->dir, s_speed_dp);
        s_seq_index++;
        if (s_seq_index >= VARIABLE_SEQ_STEPS)
        {
            s_seq_index = 0;
            s_var_seq_cycle_count++;
            if (s_var_seq_cycle_count >= VARIABLE_SEQ_CYCLES)
            {
                /* 5 次循环结束 → 随机方向 3s 爆发 */
                s_var_substate = VAR_SUB_RANDOM;
                s_var_seq_cycle_count = 0;
                s_var_rand_seed = (s_var_rand_seed * 13U + 7U) & 0xFFU;
                s_var_rand_dir = (s_var_rand_seed & 1U) ? MOTOR_DIR_FORWARD : MOTOR_DIR_REVERSE;
                app_motor_set_direction(s_var_rand_dir, s_speed_dp);
                tal_sw_timer_start(s_motor_timer_id, RANDOM_BURST_MS, TAL_TIMER_ONCE);
                return;
            }
        }
        tal_sw_timer_start(s_motor_timer_id, step->duration_ms, TAL_TIMER_ONCE);
    }
    else
    {
        /* VAR_SUB_RANDOM：3s 爆发结束，切回 seq 循环 */
        s_var_substate = VAR_SUB_SEQ;
        s_seq_index = 0;
        const sm_simple_seq_step_t *step = &s_variable_seq[0];
        app_motor_set_direction(step->dir, s_speed_dp);
        s_seq_index = 1;
        tal_sw_timer_start(s_motor_timer_id, step->duration_ms, TAL_TIMER_ONCE);
    }
}

VOID_T app_motor_init(VOID_T)
{
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .polarity = TUYA_PWM_POSITIVE,
        .count_mode = TUYA_PWM_CNT_UP,
        .duty = 0,
        .cycle = 100,
        .frequency = MOTOR_PWM_FREQ_HZ,
    };

    tal_pwm_init(MOTOR_PWM_CH_FOR, &pwm_cfg);
    tal_pwm_init(MOTOR_PWM_CH_REV, &pwm_cfg);
    tal_sw_timer_create(app_motor_timer_handler, NULL, &s_motor_timer_id);

    s_work_mode = WORK_MODE_FIXED;
    s_speed_dp = SPEED_DP_55;
    s_motor_enabled = FALSE;
    s_motor_running = FALSE;
    s_seq_index = 0;
    s_var_substate = VAR_SUB_SEQ;
    s_var_seq_cycle_count = 0;
    s_var_rand_seed = 0;

    TAL_PR_INFO("[motor] initialized");
}

VOID_T app_motor_set_mode(work_mode_t mode)
{
    if (mode > WORK_MODE_VARIABLE)
    {
        return;
    }

    s_work_mode = mode;
    s_seq_index = 0;
    s_var_substate = VAR_SUB_SEQ;
    s_var_seq_cycle_count = 0;
    if (s_motor_enabled)
    {
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
    TAL_PR_INFO("[motor] mode=%d", mode);
}

work_mode_t app_motor_get_mode(VOID_T)
{
    return s_work_mode;
}

VOID_T app_motor_set_speed_level(UINT8_T dp_level)
{
    if (dp_level < SPEED_DP_55)
    {
        dp_level = SPEED_DP_55;
    }
    if (dp_level > SPEED_DP_59)
    {
        dp_level = SPEED_DP_59;
    }
    s_speed_dp = (speed_dp_t)dp_level;
    /* 仅更新挡位，由定时器处理函数在下一步自动使用新占空比 */
    TAL_PR_INFO("[motor] speed dp=%d", s_speed_dp);
}

UINT8_T app_motor_get_speed_level(VOID_T)
{
    return (UINT8_T)s_speed_dp;
}

VOID_T app_motor_start(VOID_T)
{
    if (s_motor_enabled)
    {
        return;
    }
    s_motor_enabled = TRUE;
    s_seq_index = 0;
    s_var_substate = VAR_SUB_SEQ;
    s_var_seq_cycle_count = 0;
    app_motor_timer_handler(s_motor_timer_id, NULL);
}

VOID_T app_motor_stop(VOID_T)
{
    if (s_motor_timer_id != NULL)
    {
        tal_sw_timer_stop(s_motor_timer_id);
    }
    s_motor_enabled = FALSE;
    app_motor_set_direction(MOTOR_DIR_STOP, s_speed_dp);
}

BOOL_T app_motor_is_running(VOID_T)
{
    return s_motor_running;
}
