/**
 * @file app_motor.c
 * @brief Laser bug toy motor and laser control.
 */

#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_pwm.h"
#include "tal_gpio.h"

#include "board.h"
#include "app_motor.h"
#include "app_battery.h"
#include "app_charge.h"
#include "app_state.h"

#define BUG_SEQ_STEPS          2
#define MOTOR_STEP_MS          1000
#define BUG_PULL_STEP_MAX_MS   1000
#define BUG_PULL_STEP_MIN_MS   500
#define BUG_DIRECTION_STOP_MS  1000
#define BUG_PULL_REPEAT_COUNT  1
#define BUG_PULL_BOOST_PERCENT 30
#define MOTOR_DUTY_MIN_PERCENT 25
#define MOTOR_DUTY_MAX_PERCENT 60
#define MOTOR_DUTY_DEFAULT_PERCENT 32
#define MOTOR_STEPLESS_DEFAULT_PERCENT \
    (((MOTOR_DUTY_DEFAULT_PERCENT - MOTOR_DUTY_MIN_PERCENT) * 100) / \
     (MOTOR_DUTY_MAX_PERCENT - MOTOR_DUTY_MIN_PERCENT))

#define BATTERY_VOLTAGE_MIN    3300
#define BATTERY_VOLTAGE_MAX    4200
#define DUTY_BOOST_MAX         0

typedef struct {
    UINT8_T m1_dir;
    UINT8_T m2_dir;
    BOOL_T  m3_on;
    UINT16_T duration_ms;
    BOOL_T  m1_boost;
    BOOL_T  m2_boost;
} motor_step_t;

STATIC const motor_step_t s_bug_seq[BUG_SEQ_STEPS] = {
    {MOTOR_DIR_FORWARD, MOTOR_DIR_FORWARD, FALSE, BUG_PULL_STEP_MAX_MS, TRUE,  FALSE},
    {MOTOR_DIR_REVERSE, MOTOR_DIR_REVERSE, FALSE, BUG_PULL_STEP_MAX_MS, FALSE, TRUE},
};

STATIC game_mode_t s_game_mode = GAME_MODE_BUG_HUNT;
STATIC game_mode_t s_last_active_mode = GAME_MODE_BUG_HUNT;
STATIC UINT8_T s_stepless_percent = MOTOR_STEPLESS_DEFAULT_PERCENT;
STATIC BOOL_T s_motor_enabled = FALSE;
STATIC BOOL_T s_motor_running = FALSE;
STATIC TIMER_ID s_motor_timer_id = NULL;
STATIC UINT8_T s_seq_index = 0;
STATIC UINT8_T s_bug_repeat_count = 0;
STATIC BOOL_T s_bug_pause_active = FALSE;
STATIC UINT8_T s_alt_phase = 0;
STATIC UINT32_T s_alt_phase_elapsed_ms = 0;
STATIC UINT16_T s_alt_laser_time_s = 60;
STATIC UINT16_T s_alt_bug_time_s = 60;

STATIC UINT32_T app_motor_duty_get(VOID_T)
{
    UINT32_T base_percent;
    UINT32_T duty_percent;
    INT32_T voltage;
    UINT32_T range;
    UINT32_T drop;
    UINT32_T boost;

    base_percent = MOTOR_DUTY_MIN_PERCENT +
                   ((UINT32_T)s_stepless_percent *
                    (MOTOR_DUTY_MAX_PERCENT - MOTOR_DUTY_MIN_PERCENT)) / 100;

    if (app_charge_is_detected() || app_charge_is_full()) {
        return MOTOR_PWM_DUTY_1 * base_percent;
    }

    voltage = app_battery_get_voltage();
    if (voltage < (INT32_T)BATTERY_VOLTAGE_MIN) {
        voltage = (INT32_T)BATTERY_VOLTAGE_MIN;
    }
    if (voltage > (INT32_T)BATTERY_VOLTAGE_MAX) {
        voltage = (INT32_T)BATTERY_VOLTAGE_MAX;
    }

    range = BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN;
    drop = (UINT32_T)(BATTERY_VOLTAGE_MAX - (UINT32_T)voltage);
    boost = (drop * DUTY_BOOST_MAX) / range;
    duty_percent = base_percent + boost;
    if (duty_percent > 100) {
        duty_percent = 100;
    }

    return MOTOR_PWM_DUTY_1 * duty_percent;
}

STATIC VOID_T app_motor_pair_set(TUYA_PWM_NUM_E for_ch, TUYA_PWM_NUM_E rev_ch, UINT8_T dir, UINT32_T duty)
{
    switch (dir) {
    case MOTOR_DIR_FORWARD:
        tal_pwm_duty_set(for_ch, duty);
        tal_pwm_duty_set(rev_ch, MOTOR_PWM_DUTY_0);
        break;
    case MOTOR_DIR_REVERSE:
        tal_pwm_duty_set(for_ch, MOTOR_PWM_DUTY_0);
        tal_pwm_duty_set(rev_ch, duty);
        break;
    case MOTOR_DIR_STOP:
    default:
        tal_pwm_duty_set(for_ch, MOTOR_PWM_DUTY_0);
        tal_pwm_duty_set(rev_ch, MOTOR_PWM_DUTY_0);
        break;
    }
}

STATIC UINT32_T app_motor_duty_boost(UINT32_T duty)
{
    UINT32_T boosted = duty + (MOTOR_PWM_DUTY_1 * BUG_PULL_BOOST_PERCENT);
    UINT32_T max_duty = MOTOR_PWM_DUTY_1 * 100U;

    return (boosted > max_duty) ? max_duty : boosted;
}

STATIC VOID_T app_motor_all_stop(VOID_T)
{
    app_motor_pair_set(MOTOR_FOR_1, MOTOR_REV_1, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    app_motor_pair_set(MOTOR_FOR_2, MOTOR_REV_2, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    tal_pwm_duty_set(MOTOR_3, MOTOR_PWM_DUTY_0);
    tal_gpio_write(LASER, TUYA_GPIO_LEVEL_LOW);
    s_motor_running = FALSE;
}

STATIC VOID_T app_motor_apply_step(const motor_step_t *step)
{
    UINT32_T duty = app_motor_duty_get();
    UINT32_T m1_duty = step->m1_boost ? app_motor_duty_boost(duty) : duty;
    UINT32_T m2_duty = step->m2_boost ? app_motor_duty_boost(duty) : duty;

    app_motor_pair_set(MOTOR_FOR_1, MOTOR_REV_1, step->m1_dir, m1_duty);
    app_motor_pair_set(MOTOR_FOR_2, MOTOR_REV_2, step->m2_dir, m2_duty);
    tal_pwm_duty_set(MOTOR_3, step->m3_on ? duty : MOTOR_PWM_DUTY_0);
    tal_gpio_write(LASER, TUYA_GPIO_LEVEL_HIGH);
    s_motor_running = (step->m1_dir != MOTOR_DIR_STOP ||
                       step->m2_dir != MOTOR_DIR_STOP ||
                       step->m3_on);
}

STATIC VOID_T app_motor_laser_chase_start(VOID_T)
{
    UINT32_T duty = app_motor_duty_get() - 8 * MOTOR_PWM_DUTY_1;

    app_motor_pair_set(MOTOR_FOR_1, MOTOR_REV_1, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    app_motor_pair_set(MOTOR_FOR_2, MOTOR_REV_2, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    tal_pwm_duty_set(MOTOR_3, duty);
    tal_gpio_write(LASER, TUYA_GPIO_LEVEL_HIGH);
    s_motor_running = TRUE;
}

STATIC UINT32_T app_motor_min_u32(UINT32_T a, UINT32_T b)
{
    return (a < b) ? a : b;
}

STATIC VOID_T app_motor_alternating_finish(VOID_T)
{
    app_motor_enter_sleep_mode();
    app_state_enter_sleep();
}

STATIC VOID_T app_motor_alternating_start_bug_phase(VOID_T)
{
    s_alt_phase = 1;
    s_alt_phase_elapsed_ms = 0;
    s_seq_index = 0;
    s_bug_repeat_count = 0;
    s_bug_pause_active = FALSE;
}

STATIC BOOL_T app_motor_advance_bug_step(VOID_T)
{
    s_bug_repeat_count++;
    if (s_bug_repeat_count >= BUG_PULL_REPEAT_COUNT) {
        s_bug_repeat_count = 0;
        s_seq_index++;
        if (s_seq_index >= BUG_SEQ_STEPS) {
            s_seq_index = 0;
        }
        return TRUE;
    }
    return FALSE;
}

STATIC VOID_T app_motor_bug_stop(VOID_T)
{
    app_motor_pair_set(MOTOR_FOR_1, MOTOR_REV_1, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    app_motor_pair_set(MOTOR_FOR_2, MOTOR_REV_2, MOTOR_DIR_STOP, MOTOR_PWM_DUTY_0);
    tal_pwm_duty_set(MOTOR_3, MOTOR_PWM_DUTY_0);
    tal_gpio_write(LASER, TUYA_GPIO_LEVEL_HIGH);
    s_motor_running = FALSE;
}

STATIC UINT16_T app_motor_bug_pull_step_ms(VOID_T)
{
    UINT32_T percent = s_stepless_percent;
    UINT32_T range = BUG_PULL_STEP_MAX_MS - BUG_PULL_STEP_MIN_MS;

    if (percent < 1) {
        percent = 1;
    }
    if (percent > 100) {
        percent = 100;
    }

    return (UINT16_T)(BUG_PULL_STEP_MAX_MS - (((percent - 1) * range) / 99));
}

STATIC UINT16_T app_motor_bug_tick(VOID_T)
{
    const motor_step_t *step;

    if (s_bug_pause_active) {
        app_motor_bug_stop();
        s_bug_pause_active = FALSE;
        return BUG_DIRECTION_STOP_MS;
    }

    step = &s_bug_seq[s_seq_index];
    app_motor_apply_step(step);
    s_bug_pause_active = app_motor_advance_bug_step();
    return app_motor_bug_pull_step_ms();
}

STATIC VOID_T app_motor_alternating_handler(VOID_T)
{
    UINT32_T laser_ms = (UINT32_T)s_alt_laser_time_s * 1000UL;
    UINT32_T bug_ms = (UINT32_T)s_alt_bug_time_s * 1000UL;
    UINT32_T remaining;
    UINT32_T duration_ms;

    if (s_alt_phase == 0) {
        if (laser_ms == 0) {
            app_motor_alternating_start_bug_phase();
        } else {
            app_motor_laser_chase_start();
            remaining = laser_ms - s_alt_phase_elapsed_ms;
            duration_ms = app_motor_min_u32(MOTOR_STEP_MS, remaining);
            s_alt_phase_elapsed_ms += duration_ms;
            if (s_alt_phase_elapsed_ms >= laser_ms) {
                app_motor_alternating_start_bug_phase();
            }
            tal_sw_timer_start(s_motor_timer_id, duration_ms, TAL_TIMER_ONCE);
            return;
        }
    }

    if (bug_ms == 0) {
        app_motor_alternating_finish();
        return;
    }

    remaining = bug_ms - s_alt_phase_elapsed_ms;
    duration_ms = app_motor_min_u32(app_motor_bug_tick(), remaining);
    s_alt_phase_elapsed_ms += duration_ms;
    if (s_alt_phase_elapsed_ms >= bug_ms) {
        app_motor_alternating_finish();
        return;
    }
    tal_sw_timer_start(s_motor_timer_id, duration_ms, TAL_TIMER_ONCE);
}

STATIC VOID_T app_motor_timer_handler(TIMER_ID timer_id, VOID_T *arg)
{
    UINT16_T duration_ms;
    if (!s_motor_enabled || s_game_mode == GAME_MODE_SLEEP) {
        app_motor_all_stop();
        return;
    }

    if (s_game_mode == GAME_MODE_LASER_CHASE) {
        app_motor_laser_chase_start();
        return;
    }

    if (s_game_mode == GAME_MODE_ALTERNATING) {
        app_motor_alternating_handler();
        return;
    }

    if (s_game_mode == GAME_MODE_BUG_HUNT) {
        duration_ms = app_motor_bug_tick();
    } else {
        app_motor_all_stop();
        return;
    }

    tal_sw_timer_start(s_motor_timer_id, duration_ms, TAL_TIMER_ONCE);
}

VOID_T app_motor_init(VOID_T)
{
    TUYA_PWM_BASE_CFG_T pwm_cfg_m12 = {
        .polarity = TUYA_PWM_POSITIVE,
        .count_mode = TUYA_PWM_CNT_UP,
        .duty = 0,
        .cycle = 100,
        .frequency = MOTOR_PWM_FREQ_HZ,
    };
    TUYA_PWM_BASE_CFG_T pwm_cfg_m3 = {
        .polarity = TUYA_PWM_POSITIVE,
        .count_mode = TUYA_PWM_CNT_UP,
        .duty = 0,
        .cycle = 100,
        .frequency = MOTOR_PWM_FREQ_HZ_M3,
    };
    TUYA_GPIO_BASE_CFG_T laser_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    tal_pwm_init(MOTOR_FOR_1, &pwm_cfg_m12);
    tal_pwm_init(MOTOR_REV_1, &pwm_cfg_m12);
    tal_pwm_init(MOTOR_FOR_2, &pwm_cfg_m12);
    tal_pwm_init(MOTOR_REV_2, &pwm_cfg_m12);
    tal_pwm_init(MOTOR_3, &pwm_cfg_m3);
    tal_gpio_init(LASER, &laser_cfg);
    tal_sw_timer_create(app_motor_timer_handler, NULL, &s_motor_timer_id);

    s_game_mode = GAME_MODE_BUG_HUNT;
    s_last_active_mode = GAME_MODE_BUG_HUNT;
    s_stepless_percent = MOTOR_STEPLESS_DEFAULT_PERCENT;
    s_motor_enabled = FALSE;
    s_motor_running = FALSE;
    s_seq_index = 0;
    s_bug_repeat_count = 0;
    s_bug_pause_active = FALSE;
    s_alt_phase = 0;
    s_alt_phase_elapsed_ms = 0;
    s_alt_laser_time_s = 60;
    s_alt_bug_time_s = 60;
    app_motor_all_stop();

    TAL_PR_INFO("[motor] laser bug initialized");
}

VOID_T app_motor_set_mode(game_mode_t mode)
{
    if (mode > GAME_MODE_ALTERNATING) {
        return;
    }

    s_game_mode = mode;
    if (mode != GAME_MODE_SLEEP) {
        s_last_active_mode = mode;
    }
    s_seq_index = 0;
    s_bug_repeat_count = 0;
    s_bug_pause_active = FALSE;
    s_alt_phase = 0;
    s_alt_phase_elapsed_ms = 0;
    if (s_motor_enabled) {
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
    TAL_PR_INFO("[motor] game mode=%d", mode);
}

game_mode_t app_motor_get_mode(VOID_T)
{
    return s_game_mode;
}

game_mode_t app_motor_get_report_mode(VOID_T)
{
    return (s_game_mode == GAME_MODE_SLEEP) ? s_last_active_mode : s_game_mode;
}

VOID_T app_motor_enter_sleep_mode(VOID_T)
{
    s_game_mode = GAME_MODE_SLEEP;
    s_seq_index = 0;
    s_bug_repeat_count = 0;
    s_bug_pause_active = FALSE;
    s_alt_phase = 0;
    s_alt_phase_elapsed_ms = 0;
}

VOID_T app_motor_wake_last_mode(VOID_T)
{
    if (s_game_mode == GAME_MODE_SLEEP) {
        s_game_mode = s_last_active_mode;
        s_seq_index = 0;
        s_bug_repeat_count = 0;
        s_bug_pause_active = FALSE;
        s_alt_phase = 0;
        s_alt_phase_elapsed_ms = 0;
    }
}

VOID_T app_motor_set_stepless_percent(UINT8_T percent)
{
    if (percent > 100) {
        percent = 100;
    }

    s_stepless_percent = percent;
    if (s_motor_enabled && s_game_mode != GAME_MODE_SLEEP) {
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
    TAL_PR_INFO("[motor] speed percent=%d", s_stepless_percent);
}

UINT8_T app_motor_get_stepless_percent(VOID_T)
{
    return s_stepless_percent;
}

VOID_T app_motor_set_alt_laser_time(UINT16_T seconds)
{
    if (seconds > 180) {
        seconds = 180;
    }
    s_alt_laser_time_s = seconds;
    if (s_motor_enabled && s_game_mode == GAME_MODE_ALTERNATING) {
        s_alt_phase = 0;
        s_alt_phase_elapsed_ms = 0;
        s_seq_index = 0;
        s_bug_repeat_count = 0;
        s_bug_pause_active = FALSE;
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
}

UINT16_T app_motor_get_alt_laser_time(VOID_T)
{
    return s_alt_laser_time_s;
}

VOID_T app_motor_set_alt_bug_time(UINT16_T seconds)
{
    if (seconds > 180) {
        seconds = 180;
    }
    s_alt_bug_time_s = seconds;
    if (s_motor_enabled && s_game_mode == GAME_MODE_ALTERNATING) {
        s_alt_phase = 0;
        s_alt_phase_elapsed_ms = 0;
        s_seq_index = 0;
        s_bug_repeat_count = 0;
        s_bug_pause_active = FALSE;
        app_motor_timer_handler(s_motor_timer_id, NULL);
    }
}

UINT16_T app_motor_get_alt_bug_time(VOID_T)
{
    return s_alt_bug_time_s;
}

UINT32_T app_motor_get_mode_timeout_ms(VOID_T)
{
    if (s_game_mode == GAME_MODE_ALTERNATING) {
        return ((UINT32_T)s_alt_laser_time_s + (UINT32_T)s_alt_bug_time_s) * 1000UL;
    }
    return WORK_PERIOD_MS;
}

VOID_T app_motor_start(VOID_T)
{
    if (s_motor_enabled) {
        return;
    }

    s_motor_enabled = TRUE;
    s_seq_index = 0;
    s_bug_repeat_count = 0;
    s_bug_pause_active = FALSE;
    s_alt_phase = 0;
    s_alt_phase_elapsed_ms = 0;
    app_motor_timer_handler(s_motor_timer_id, NULL);
}

VOID_T app_motor_stop(VOID_T)
{
    if (s_motor_timer_id != NULL) {
        tal_sw_timer_stop(s_motor_timer_id);
    }
    s_motor_enabled = FALSE;
    app_motor_all_stop();
}

BOOL_T app_motor_is_running(VOID_T)
{
    return s_motor_running;
}
