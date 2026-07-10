/**
 * @file app_dp_parser.h
 * @brief afp pawswiff DP parser.
 */

#ifndef __APP_DP_PARSER_H__
#define __APP_DP_PARSER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DP_ID_SWITCH           1   /* bool rw: App on/off */
#define DP_ID_MODE             4   /* enum rw: fixed/variable */
#define DP_ID_BATTERY         18   /* value ro: 0~100 */
#define DP_ID_SPEED_LEVEL     22   /* enum rw: level_1~level_5 */
#define DP_ID_WORK_STATE      26   /* enum ro: work/sleep */

#define DP_ID_MOVEMENT_LEVEL  DP_ID_SPEED_LEVEL

#pragma pack(1)
typedef struct {
    UINT8_T  dp_id;
    UINT8_T  dp_type;
    UINT16_T dp_data_len;
    UINT8_T  dp_data[600];
} demo_dp_t;
#pragma pack()

extern demo_dp_t g_cmd;
extern demo_dp_t g_rsp;
extern UINT32_T  g_sn;

OPERATE_RET app_dp_parser(UINT8_T *buf, UINT32_T size);
OPERATE_RET app_dp_report(UINT8_T dp_id, UINT8_T *buf, UINT32_T size);
VOID_T app_dp_report_all(VOID_T);

#ifdef __cplusplus
}
#endif

#endif
