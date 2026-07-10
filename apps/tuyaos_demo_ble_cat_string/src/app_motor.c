/**
 * @file app_motor.c
 * @brief afp pawswiff motor control.
 */

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_pwm.h"

#include "board_cat_string.h"
#include "app_motor.h"

#define VARIABLE_SEQ_STEPS  8
#define FIXED_STEP_MS       1000

STATIC work_mode_t s_work_mode = WORK_MODE_VARIABLE;
STATIC UINT8_T s_speed_level = 2;
STATIC BOOL_T s_motor_enabled = FALSE;
STATIC BOOL_T s_motor_running = FALSE;
STATIC TIMER_ID s_motor_timer_id = NULL;
STATIC UINT8_T s_seq_index = 0;

STATIC const UINT32_T s_level_duty[5] = {
    MOTOR_PWM_DUTY_35,
    MOTOR_PWM_DUTY_50,
    MOTOR_PWM_DUTY_65,
    MOTOR_PWM_DUTY_80,
    MOTOR_PWM_DUTY_100,
};

STATIC const motor_step_t s_variable_seq[VARIABLE_SEQ_STEPS] = {
    {MOTOR_DIR_FORWARD, 1200, 1},
    {MOTOR_DIR_STOP,     350, 1},
    {MOTOR_DIR_REVERSE, 1500, 3},
    {MOTOR_DIR_STOP,     500, 3},
    {MOTOR_DIR_FORWARD, 2200, 5},
    {MOTOR_DIR_REVERSE,  900, 2},
    {MOTOR_DIR_STOP,     600, 2},
    {MOTOR_DIR_FORWARD, 1600, 4},
};

STATIC UINT32_T app_motor_duty_for_level(UINT8_T level)
{
    if (level < 1) {
        level = 1;
    }
    if (level > 5) {
        level = 5;
    }
    return s_level_duty[level - 1];
}

STATIC VOID_T app_motor_set_direction(UINT8_T direction, UINT8_T level)
{
    UINT32_T duty = app_motor_duty_for_level(level);

    switch (direction) {
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
    if (!s_motor_enabled) {
        app_motor_set_direction(MOTOR_DIR_STOP, s_speed_level);
        return;
    }

    if (s_work_mode == WORK_MODE_FIXED) {
        app_motor_set_direction(MOTOR_DIR_FORWARD, s_speed_level);
        tal_sw_timer_start(s_motor_timer_id, FIXED_STEP_MS, TAL_TIMER_ONCE);
        return;
    }

    const motor_step_t *step = &s_variable_seq[s_seq_index];
    app_motor_set_direction(step->direction, step->speed_level);
    s_seq_index++;
    if (s_seq_index >= VARIABLE_SEQ_STEPS) {
        s_seq_index = 0;
    }
    tal_sw_timer_start(s_motor_timer_id, step->duration_ms, TAL_TIMER_ONCE);
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

    s_work_mode = WORK_MODE_VARIABLE;
    s_speed_level = 2;
    s_motor_enabled = FALSE;
    s_motor_running = FALSE;
    s_seq_index = 0;

    TAL_PR_INFO("[motor] initialized");
}

VOID_T app_motor_set_mode(work_mode_t mode)
{
    if (mode > WORK_MODE_VARIABLE) {
        return;
    }

    s_work_mode = mode;
    s_seq_index = 0;
    if (s_motor_enabled) {
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
    TAL_PR_INFO("[motor] mode=%d", mode);
}

work_mode_t app_motor_get_mode(VOID_T)
{
    return s_work_mode;
}

VOID_T app_motor_set_speed_level(UINT8_T level)
{
    if (level > 4) {
        level = 4;
    }
    s_speed_level = level + 1;
    if (s_motor_enabled && s_work_mode == WORK_MODE_FIXED) {
        app_motor_set_direction(MOTOR_DIR_FORWARD, s_speed_level);
    }
    TAL_PR_INFO("[motor] speed level=%d", s_speed_level);
}

UINT8_T app_motor_get_speed_level(VOID_T)
{
    return s_speed_level - 1;
}

VOID_T app_motor_start(VOID_T)
{
    if (s_motor_enabled) {
        return;
    }
    s_motor_enabled = TRUE;
    s_seq_index = 0;
    app_motor_timer_handler(s_motor_timer_id, NULL);
}

VOID_T app_motor_stop(VOID_T)
{
    if (s_motor_timer_id != NULL) {
        tal_sw_timer_stop(s_motor_timer_id);
    }
    s_motor_enabled = FALSE;
    app_motor_set_direction(MOTOR_DIR_STOP, s_speed_level);
}

BOOL_T app_motor_is_running(VOID_T)
{
    return s_motor_running;
}
