/**
 * @file app_motor.h
 * @brief afp pawswiff motor control.
 */

#ifndef __APP_MOTOR_H__
#define __APP_MOTOR_H__

#include "tuya_cloud_types.h"
#include "tal_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_DIR_STOP      0
#define MOTOR_DIR_FORWARD   1
#define MOTOR_DIR_REVERSE   2

#define MOTOR_PWM_CH_FOR        MOTOR_FOR
#define MOTOR_PWM_CH_REV        MOTOR_REV
#define MOTOR_PWM_FREQ_HZ       100

/* tal_pwm_duty_set() on TLSR825x uses 0~1000000, independent of cfg.cycle. */
#define MOTOR_PWM_DUTY_100      1000000U
#define MOTOR_PWM_DUTY_95       950000U
#define MOTOR_PWM_DUTY_90       900000U
#define MOTOR_PWM_DUTY_85       850000U
#define MOTOR_PWM_DUTY_80       800000U
#define MOTOR_PWM_DUTY_75       750000U
#define MOTOR_PWM_DUTY_70       700000U
#define MOTOR_PWM_DUTY_65       650000U
#define MOTOR_PWM_DUTY_60       600000U
#define MOTOR_PWM_DUTY_55       550000U
#define MOTOR_PWM_DUTY_50       500000U
#define MOTOR_PWM_DUTY_40       400000U
#define MOTOR_PWM_DUTY_35       350000U
#define MOTOR_PWM_DUTY_30       300000U
#define MOTOR_PWM_DUTY_20       200000U
#define MOTOR_PWM_DUTY_0        0U

typedef enum {
    SPEED_DP_55 = 0,
    SPEED_DP_56 = 1,
    SPEED_DP_57 = 2,
    SPEED_DP_58 = 3,
    SPEED_DP_59 = 4,
} speed_dp_t;

typedef enum {
    WORK_MODE_FIXED   = 0,
    WORK_MODE_VARIABLE = 1,
} work_mode_t;

VOID_T app_motor_init(VOID_T);
VOID_T app_motor_set_mode(work_mode_t mode);
work_mode_t app_motor_get_mode(VOID_T);
VOID_T app_motor_set_speed_level(UINT8_T dp_level);
UINT8_T app_motor_get_speed_level(VOID_T);
VOID_T app_motor_start(VOID_T);
VOID_T app_motor_stop(VOID_T);
BOOL_T app_motor_is_running(VOID_T);

#ifdef __cplusplus
}
#endif

#endif
